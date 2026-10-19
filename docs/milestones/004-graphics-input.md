# Milestone 004: graphics and input

**READY TO START, 2026-10-20.** The bounded native startup reaches registration of
its VBLANK handler after owning a GE finish callback. The EDRAM size query is
implemented against allocated guest VRAM. All 59 constructors, the 2048-record
initialization loop and the documented startup kernel services complete; two
worker threads remain Ready. See [handoff evidence](../research/ge-callbacks.md).

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

## Exit evidence

Require advancing display lists, correct visible menu rendering, working input,
and bounded memory/resource lifetime on a real Android device. Complete Milestone
003's title-screen gate only after that screen is actually reached and verified.
Synthetic graphics tests, cross-compilation, callback registration and headless
startup alone do not meet either visual gate. Publish reviewed results only;
captures, game-derived translations and logs remain under ignored `local/`.
