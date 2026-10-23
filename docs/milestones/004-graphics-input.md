# Milestone 004: graphics and input

**IN PROGRESS, 2026-10-22.** The bounded native startup completes registration and
enable of its VBLANK handler. The [display foundation](../research/display-foundation.md)
adds synthetic clock, interrupt delivery, wait/cancellation and framebuffer contracts.
No visual gate is complete. At the initial handoff, startup reached registration of
its VBLANK handler after owning a GE finish callback. The EDRAM size query is
implemented against allocated guest VRAM. All 59 constructors, the 2048-record
initialization loop and the documented startup kernel services complete; two
worker threads now wait on their shared event flag. See [handoff evidence](../research/ge-callbacks.md).

This is a dependency handoff, not completion of Milestone 003. Its title-screen
gate remains open: display events and graphics integration are now required to
continue toward it. Filesystem, module-loading and additional kernel contracts
remain Milestone 003 work when observed. Neither milestone has a playable result.

## Ordered implementation tasks

1. Implement checked VBLANK subinterrupt registration, enable/disable/release and
   ownership. Begin with observed interrupt 30/subinterrupt 0; validate the handler
   `0x08AC6C74` and its required AOT coverage before allowing execution.
2. Establish a separate guest display clock and deterministic event ordering.
   Preserve documented LCD timing; host profiling time is not guest simulation
   time. Integrate interrupt suspension, full CPU context preservation, callback
   return, waiting threads and cancellation. Test synthetic time explicitly;
   never call synthetic ticks real game timing or return successful waits early.
3. Model framebuffer descriptors and latch behavior, with bounded guest RAM/VRAM
   ranges. Implement only observed display services and verify their error and
   synchronization contracts against pinned references.
4. Implement bounded GE list ownership, stalls and completion, then SIGNAL/FINISH
   dispatch to owned callbacks. Test pointer arithmetic, command budgets, invalid
   control flow, deletion/reuse and callback ordering before connecting rendering.
   Unsupported commands remain explicit failures.
5. Add the native Android surface and Vulkan backend for the observed command
   subset. Keep Yakumo external and unchanged; reusable renderer extraction needs
   a reviewed dependency/ownership decision and independent contract tests.
6. Connect gamepad/touch input and verify menus on Android. Keep stock 480x272
   semantics while validating graphics. Ultrawide, texture replacements and combat
   modifications remain later milestones.

## Implementation checkpoint

- **VERIFIED:** observed VBLANK registration/enable, handler AOT coverage and
  synthetic full-context delivery on an owned stack.
- **VERIFIED:** exact rational LCD event clock, VBLANK phases, waiting/cancellation
  and integrated event ordering in synthetic execution. An explicit bounded idle producer executes the real handler once all threads
  block. Runnable guest-time accounting remains required; dispatch counts are not
  elapsed guest cycles.
- **VERIFIED:** checked framebuffer descriptors/latches and the observed display-off
  SetFrameBuf/WaitVblank path. Other display services remain observation-driven.
- **VERIFIED:** [GE control-stream contracts](../research/ge-lists.md), including
  stalls, control-flow budgets, stale callback generations and actual synthetic
  AOT callback returns. The first real initialization list completes EnQueue/ListSync with retained
  state. Drawing commands and asynchronous guest submissions remain rejecting.
- **VERIFIED:** a separate [Android host probe](../development/android-host.md)
  compiles SDL/Vulkan surface ownership, a synthetic clear presenter and a sampled
  input adapter into signed ARM64 and x86_64 APKs. ELF and APK 16 KiB alignment checks pass.
  Synthetic input tests cover stable device/finger identity, safe-area letterboxing,
  invalid contacts, capacity, cancellation and analog boundaries.
- **PLANNED:** connect an observed GE drawing subset and guest controller services.
  A 211-command game initialization stream has executed; no draw stream or menu
  has yet executed. **VERIFIED:** the official API 37 x86_64 / 16 KiB AVD installs
  and launches the host, presents Vulkan pixels, handles touch and a virtual
  controller (including held disconnect), survives three Home/resume cycles and
  continues rendering over a subsequent 30-second interval. This is synthetic
  host execution. **BLOCKED:** physical ARM64 GPU/controller validation requires
  attached hardware. Game drawing and menu input remain unimplemented; the
  milestone is not ready to advance to stock combat.

The event scheduler now queues both observed worker waits, with checked FIFO
wakeup, cancellation and output ownership. The explicit research idle producer resumes the graphics thread after one
VBLANK and completes the first GE initialization list and the stock display-mode wait
after two VBLANK handler returns. No rendering is claimed.
The 323-root startup probe also passes guarded graphics object methods, six
checked writebacks and matrix-state setup. After a terminal
diagnostic captured target `0x08AC39A8` at call site `0x08AC2C00`, exact-input
slot review and synthetic regressions permitted that single target. Native
execution reaches the method and returns through `0x08AC2C08`. A subsequent
checked-slot mapping at `0x08AC7358` executes graphics memory setup and returns;
the checked method at `0x08AC757C` and three guarded resource-query sites now
return and complete layout setup. The cleanup helper's empty-list path returns;
the checked next state callback `0x08862200` now executes and reaches a
512 KiB writeback within its live partition. The next stop captures matrix
method `0x08AC3CD8` at the terminal guard `0x08AC3DF0`.
The cleanup callback remains terminal and has not executed. See the
[guarded diagnostic](../research/startup-indirect-2026-10-22.md).
See [continued coverage](../research/ge-lists.md).

Validation: 70 Python tests, 18 integrated CTest suites, Android ARM64
cross-compilation and synthetic APK build/signature/alignment checks pass.
Raw traces, translations, APKs and debug keys remain local.

## Exit evidence

Require advancing display lists, correct visible menu rendering, working input,
and bounded memory/resource lifetime on a real Android device. Complete Milestone
003's title-screen gate only after that screen is actually reached and verified.
Synthetic graphics tests, cross-compilation, callback registration and headless
startup alone do not meet either visual gate. Publish reviewed results only;
captures, game-derived translations and logs remain under ignored `local/`.
