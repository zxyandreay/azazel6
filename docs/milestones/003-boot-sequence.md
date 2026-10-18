# Milestone 003: boot sequence

**IN PROGRESS, 2026-10-19.** Native bring-up is implemented; title-screen boot is
not. The current bounded experiment completes the first 32 constructors, creates
three mutexes and eight inactive timers, and reaches uncovered helper `0x089B6DB8` in constructor 33. See
[guarded object-dispatch evidence](../research/virtual-dispatch.md).

Implemented foundations include thread lifecycle and priority handoff, shared
stack/partition allocation, resident module identity, nested interrupt state,
conservative startup reachability and guarded constructor dispatch. These remain
restricted contracts, not a complete PSP kernel.

Next dependencies are synchronization waits and timeouts, broader verified AOT,
filesystem/ISO access and module loading as observed. General timers, callbacks,
interrupt delivery, overlays, rendering and title-screen progression remain
unimplemented. Preserve the external timeout and rejecting unknown-import policy.
