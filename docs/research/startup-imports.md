# Startup imports and thread boundary

## Question and sources

Which services must return before translated startup can reach its next boundary?

**VERIFIED source inspection:** uOFW revision
`28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9`, read-only under `local/references/uofw`.
The [SysMem export map](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/sysmem/exports.exp)
identifies both NIDs; [misc.c](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/sysmem/misc.c)
provides the modeled state/return contracts. This public firmware reconstruction
is evidence for implementation, not proof of equivalence to every PSP firmware.
The [ThreadMan export map](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/threadman/exports.exp)
identifies the next service. No reference source was copied or changed.

## Implemented contracts

Both services belong to `SysMemUserForUser`. State belongs to each runtime and is
retained by its registered callbacks; there is no process-global settings object.

| NID | Service | Modeled behavior |
| --- | --- | --- |
| `0x91DE343C` | `sceKernelSetCompiledSdkVersion500_550` | Accept argument upper half `0x0500` or `0x0505`, set game-info flag `0x1000`, return zero |
| `0xF77D77CB` | `sceKernelSetCompilerVersion` | Store the full 32-bit argument as compiler version, return zero |

The first service does **not** assign the firmware's compiled-SDK-version field.
The host's `last_sdk_argument` is only a diagnostic. The compiler setter does not
set flag `0x2000`. No getter is implemented and no getter behavior is inferred.
Firmware privilege/K1 handling is outside this experimental host's model.

**VERIFIED host policy:** unsupported SDK families stop without returning or
mutating settings. The reference's invalid-version path resets/hangs; deliberately
stopping is a fail-closed research policy, not emulation of that hardware behavior.
Unimplemented library/NID pairs remain missing-HLE errors.

Import wrappers save the guest return address and execution-context token. They
return to RA only if the service leaves PC at its stub, does not stop, and keeps
the same execution context. A redirect or context switch is preserved. Synthetic
tests exercise those conditions; this does not implement thread scheduling.

## Native observation

This section records the initial settings-service checkpoint. The newer
[dormant-thread checkpoint](dormant-threads.md) advances to StartThread.

**VERIFIED 2026-10-17, Windows x64/MSVC:** exact supported executable hash was
checked before generation and again before execution. The bounded startup CFG
contains 69 instruction words across `[0x08804124, 0x08804238)`, including branch
paths and delay slots. This is generated coverage, not a count of instructions
executed on the observed path. Callees are excluded.

Six outer dispatches were observed: entry, SDK stub, return continuation, compiler
stub, return continuation, CreateThread stub. The SDK argument was `0x05050010`;
compiler argument was `0x00030306`. Two HLE calls completed, producing host
game-info flags `0x00001000` and compiler version `0x00030306`.

The next stop was `ThreadManForUser::sceKernelCreateThread`, NID `0x446D8DE6`:

| Field | Observed guest value |
| --- | --- |
| Stub PC | `0x08B88EE0` |
| Return address | `0x088041FC` |
| SP | `0x09FFFFD0` |
| a0, name pointer | `0x08B89E38` |
| a1, requested entry | `0x08804238` |
| a2, priority | `0x00000028` (40) |
| a3, stack size | `0x00008000` (32768) |
| t0, attributes | `0x80000000` |
| t1, options pointer | `0x00000000` |

This trace follows translated game instructions; host SDK/compiler behavior is
the explicitly modeled part. Entry arguments are still experimental zeros, GP
comes from verified module metadata, and the initial guest stack is a separate
64 KiB host reservation. The observed request does not validate bootstrap ABI,
thread stack allocation or lifecycle behavior.

## Reproduction and next experiment

Use the [bounded probe build commands](../development/building.md), regenerating
the entry output before building. Local evidence is `manifest.json`,
`generation.log`, `execution.log` and `execution.json` in the chosen output
directory. Native exit 3 is recorded as a research boundary, never game success.
The external timeout remains 15 seconds, outer dispatch budget 64, and interpreter
fallback is disabled. Both limits are necessary because generated loops may stay
inside one dispatch.

The bounded CreateThread subset now owns dormant objects and reserved stacks; see
[its contracts and remaining work](dormant-threads.md). StartThread and module-start
argument transfer still require investigation before permitting the new thread's
entry to execute. Expand only verified code ranges.
