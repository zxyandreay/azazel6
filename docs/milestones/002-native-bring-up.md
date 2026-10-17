# Milestone 002: native executable bring-up

**Status: IMPLEMENTED, 2026-10-18.** The original milestone requires guest memory,
ELF loading, AOT registration, PSP imports and first verified Tekken execution.
Those foundations now execute the exact-hash module bootstrap and its first child
thread. This is not completion of the boot sequence or a playable port.

## Verified outcomes

- The validated ELF loads into checked guest RAM; GP and bootstrap thread
  parameters come from module metadata and exports.
- Bounded CFG discovery supplies six observed roots, 530 instruction words,
  generated native C++ and returning wrappers for all 268 imports. Unsupported
  control flow fails closed and missing HLE remains rejecting.
- SDK/compiler services, thread create/start/exit/delete, argument copying,
  service-boundary priority scheduling and shared stack/heap allocation work.
- The real startup returns from module_start, executes its child, allocates an
  aligned 18 MiB-class partition block and resolves its resident module ID.
- Native execution now reaches interrupt suspension in runtime initialization:
  `Kernel_Library::0x092968F4` at `0x08B88D38`.

## Validation and limits

21 synthetic Python tests and eight integrated CTest suites passed on Windows.
The generated probe compiled and linked on Windows and Android ARM64/API 26.
The real-input probe retained a 15-second timeout, 64-dispatch limit and disabled
interpreter fallback. Linux CI has passed the preceding memory checkpoint;
subsequent commit results are tracked by GitHub Actions.

Only observed startup code is covered. Full-corpus compilation, general code/data
classification, overlays, complete kernel ABI, interrupts, waits, file I/O and
boot progression remain Milestone 003 dependencies. Android execution, graphics,
audio and input remain unverified or unimplemented.

## Evidence trail

- [SDK/compiler settings](../research/startup-imports.md)
- [Dormant thread ownership](../research/dormant-threads.md)
- [Thread start and bootstrap handoff](../research/thread-startup.md)
- [Shared partition memory](../research/partition-memory.md)
- [Resident module identity](../research/module-identity.md)

Milestone 003 continues with interrupt state and bounded startup expansion.
