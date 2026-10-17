# Startup interrupt state

**VERIFIED, 2026-10-18:** the profile models the interrupt-enable state used by
Kernel_Library suspend/resume, including nested save/restore. This is not interrupt
delivery, timer emulation or a complete kernel critical-section implementation.

Pinned uOFW `28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9`,
`src/kd/interruptman/end.S`, reads the previous controller state on suspend,
clears it and restores the supplied state on resume. Resume does not write v0.
Pinned PSPSDK `6ef2b21dc440ef8d4125f000301faaa2a470975e`,
`src/user/Kernel_Library.S` and `pspintrman.h`, identify the user import NIDs and
void resume ABI. Independently authored HLE accepts the modeled 0/1 flags only;
other controller bits stop the experiment. The synchronized variant has the same
state transition because this runtime has one execution thread and no devices.

Thread mutations and exits while interrupts are disabled stop before mutation;
no priority switch is allowed to silently cross an unsupported critical section.
Synthetic tests cover nesting, v0 preservation, return PC, unsupported flags,
per-runtime isolation and rejected scheduling without changing dormant state.

The real startup completes two critical sections, including a nested suspend
returning zero, and reaches `0x088040C4` through `0x08804018`. Nine observed roots
now contain 653 instruction words. The next scanner boundary is an indirect
transfer in that helper; its reachability/target must be proven before expansion.
21 Python tests, eight CTest suites and Android ARM64 compilation passed.
