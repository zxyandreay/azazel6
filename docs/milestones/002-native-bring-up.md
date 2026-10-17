# Milestone 002: native bring-up

Current continuation: [thread startup and bootstrap handoff](../research/thread-startup.md).

**Status: IN PROGRESS, checkpoint 2026-10-17.** This advances the foundation probe;
it does not complete bootstrap ABI research or produce a playable port.

## Implemented checkpoint

- Bounded intraprocedural startup discovery replaces the first-call-only slice.
  The exact input yields 69 instruction words, including delay slots; indirect
  transfers, invalid targets and unsupported branch forms fail closed.
- Generated local import wrappers preserve HLE redirects/context changes and
  perform ordinary returns only when the service remains in the same context.
- Profile-owned SDK/compiler settings implement two source-backed service
  contracts without changing the external pinned framework.
- Diagnostics show import arguments and modeled state, with explicit stops for
  missing services and separate failure results for other stop reasons.

The first checkpoint completed SDK/compiler calls and reached CreateThread; see
[settings-service evidence](../research/startup-imports.md). The second checkpoint
adds owned dormant threads, bounded guest stacks and deletion/reuse contracts.
**VERIFIED current outcome:** startup creates one dormant thread and reaches
`sceKernelStartThread` at `0x08B88E70`. No thread is started. See
[thread contracts and observations](../research/dormant-threads.md).

## Validation

- 19 Python tests passed using synthetic input, including six CFG test methods.
- Six integrated Release CTest suites passed: aspect, bootstrap HLE, dormant
  threads, and three upstream suites. HLE tests include state lifetime, invalid
  arguments, stack ownership/exhaustion and returns across redirects/context switches.
- Generated startup and wrappers compiled with MSVC 19.43.34809. A hash-validated
  real-input experiment stopped at the missing StartThread service within its
  15-second timeout and 64-dispatch budget; no interpreter fallback was enabled.
- Expanded probe and bootstrap HLE cross-compiled/linked for Android ARM64/API 26
  with NDK 27.1.12297006. No Android device execution or APK packaging was tested.
- Repository CI runs synthetic tests only. Hosted execution status is available
  from the commit's GitHub Actions run; the local real-game trace is not uploaded.

## Remaining milestone work

**PLANNED:** establish module-start ABI and the first thread lifecycle, validate
guest-owned names/options/stack/UIDs, and expand verified AOT coverage as required.
Kernel scheduling and subsequent boot services may span Milestone 003. Full-corpus
compilation, graphics, audio, input and gameplay remain unimplemented.

The next concrete task is StartThread argument ownership and bootstrap scheduling,
including caller priority and return lifecycle. Keep that boundary until semantics
and failure tests exist; fabricated success is not progress.
