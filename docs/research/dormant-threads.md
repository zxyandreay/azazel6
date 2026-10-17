# Dormant thread ownership and StartThread boundary

Current continuation: [thread startup and bootstrap handoff](thread-startup.md).

## Evidence and scope

**VERIFIED source inspection, 2026-10-17:** uOFW's ThreadMan exports identify
CreateThread (`0x446D8DE6`) and DeleteThread (`0x9FA03CD3`), but its implementations
are empty TODOs. They cannot justify a scheduler or stack ABI.

Additional read-only references:

- [PSPSDK thread API](https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/user/pspthreadman.h),
  revision `6ef2b21dc440ef8d4125f000301faaa2a470975e`: create/delete signatures,
  user-mode attributes, no-fill and clear-stack flags, dormant state and GP field.
- [PSP autotests creation expectations](https://github.com/hrydgard/pspautotests/blob/071a2a6f14477b8b186bd53201e2f2c0adc80679/tests/threads/threads/create.expected),
  revision `071a2a6f14477b8b186bd53201e2f2c0adc80679`: null/long names, stack
  minimum/rounding, priority bounds, attributes and dormant null-entry acceptance.
- [PSP autotests start expectations](https://github.com/hrydgard/pspautotests/blob/071a2a6f14477b8b186bd53201e2f2c0adc80679/tests/threads/threads/start.expected):
  argument copying and priority-dependent scheduling, plus no-fill/clear-stack
  observations. These are DOCUMENTED reference results, not hardware tests run here.

Only API facts and observations informed original code and synthetic tests. No
reference implementation, test body or binary is copied into distributable files.

## Implemented subset

`profiles/tekken6/host/threads.cpp` owns per-runtime dormant objects and their
reserved guest memory. CreateThread returns a positive UID only after the object
and stack exist. DeleteThread releases an existing dormant object and its stack.
Duplicate names are allowed. Deleting a stale UID fails; UIDs are never reused
within that registry. Snapshots copy metadata and do not expose host pointers or
references whose lifetime depends on the object remaining alive.

- User priorities 8 through 119 are accepted; other values return `0x80020193`.
- Stacks below 512 bytes return `0x80020194`; accepted sizes round up to 256 bytes
  using widened arithmetic. Arena exhaustion returns `0x80020190` without writes
  or consuming a UID. The same applies to oversized/wrapping requests.
- Null names return `0x80020001`. Names copy at most 31 bytes; empty names work.
  Pointer checks precede reads. Unsupported noncanonical/out-of-RAM name pointers
  stop the host rather than attempting hardware privilege/alias behavior.
- Supported input flags are USER, NO_FILLSTACK and CLEAR_STACK, including zero.
  Stored attributes include user mode and low byte `0xFF`. Other flags and all
  nonnull options stop without changing objects, guest memory, PC or return value.
- Null entry is allowed for a dormant object. Other entries must be aligned and
  in the image's file-backed executable span. An unsupported entry layout stops;
  this restriction does not establish PSP's general entry-validation behavior.
- Stacks fill with `0xFF` unless NO_FILLSTACK is set. CLEAR_STACK zeroes owned
  stack memory before deletion. Guest memory outside the allocation is preserved.

**Host policy, not firmware layout:** the arena begins at the loaded image's
rounded-up end and ends below the separate 64 KiB bootstrap stack. Allocation
uses the highest fitting free interval; deletion makes holes reusable. This is
not a PSP partition allocator. Registration rejects image overlap, unaligned or
out-of-RAM arenas, invalid executable spans and GP outside the image. The host
accepts exactly one loaded segment. GP metadata comes from the verified owning
module, not the caller's temporary GP; no new CPU context has been activated.

Opaque UID numbers, allocation addresses and simultaneous-invalid-argument error
precedence are not claimed to match firmware. Unknown UIDs use the documented
kernel not-found-thread code; UID zero uses illegal-thread. Only dormant objects
exist, so deleting running or waiting threads is outside this implementation.

## Native result

**VERIFIED Windows x64/MSVC:** the hash-validated ULUS10466 probe completed the
two settings services and CreateThread, then stopped at missing StartThread after
eight outer dispatches. Probe coverage remains the same 69-word startup CFG;
the new thread entry has not been compiled into this probe or executed.

| Field | Observed value |
| --- | --- |
| Owned dormant UID | 1 (host allocation) |
| Thread entry / priority | `0x08804238` / 40 |
| Stack allocation | `[0x09FE8000, 0x09FF0000)`; 32768 bytes |
| Owning module GP | `0x08C022B0` |
| Next import | `ThreadManForUser::sceKernelStartThread`, NID `0xF475845D` |
| Stop PC / RA / SP | `0x08B88E70` / `0x0880420C` / `0x09FFFFD0` |
| StartThread a0 / a1 / a2 | UID 1 / length 0 / pointer 0 |

The zero arguments originate from this experiment's zero module-start arguments;
they do not establish the actual loadexec argument block. The runner's 15-second
timeout and 64-dispatch limit remain in force. Interpreter fallback stays disabled.
Local logs are under `local/entry-probe-threads`; no raw game data is published.

## Tests and next task

**VERIFIED:** six native CTest suites and 19 Python tests pass. New synthetic tests
exercise stack bounds, rounding overflow, exhaustion, hole reuse, fill/clear,
register preservation, UID lifetime, state isolation and explicit unsupported
paths. Android ARM64/API 26 compilation/linking passes; device execution is untested.
DeleteThread is exercised synthetically, not reached in the native game trace.

**PLANNED:** establish the bootstrap caller's priority and return lifecycle, then
implement StartThread argument ownership, initial CPU/stack state and scheduling
decisions with synthetic multi-context tests. Reference start results show that
immediately switching to every created thread would be wrong. Determine the
module-start argument block and generate verified coverage of the requested
entry before allowing the new context to run. No runnable-thread success is
reported at this checkpoint.
