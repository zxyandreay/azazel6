# Android ARM64 preparation

**VERIFIED 2026-10-17:** NDK `27.1.12297006` (r27b), Clang 18.0.2,
CMake 4.0.3 and Ninja from SDK CMake 3.22.1 cross-compiled the math library,
PSPRecomp core/tools and generated entry probe for `arm64-v8a`, API 26. The
Milestone 002 expanded probe, import wrappers and bootstrap HLE also compile/link.
No Android binary was executed at that checkpoint. NDK/CMake deprecation
warnings and existing upstream conversion warnings did not prevent linking.

API 26 is a native smoke-build floor, not a promised product minimum. Yakumo's
current application requires API 29 and Vulkan 1.1. The separate synthetic
[host probe](../development/android-host.md) adopts API 29 and Vulkan 1.1;
record actual supported devices and extensions rather than assuming the loader
implies a compatible driver. Resolve device features, formats and queues at startup.

## Application integration

Use SDL3's Android activity and Java classes from the same pinned SDL source as
the native library. The synthetic `libmain.so` owns a checked Vulkan clear
presenter and sampled input adapter. [ADR-0003](../decisions/ADR-0003-android-host-probe.md)
selects SDK-tool packaging for this diagnostic, with the external SDL activity.
A conventional Gradle externalNativeBuild integration remains the planned
product route once a running game host warrants it.

Use Android's Storage Access Framework picker for user game imports. A content URI
is not a filesystem path: use a granted descriptor or bounded streaming copy into
app-private storage, hash the copied bytes, reject incompatible executables and
commit the import atomically. Preserve user permission grants and report disk-space,
cancellation and partial-copy errors. Never request broad storage access just to
avoid URI handling. Separate immutable game input, caches, saves and diagnostics.

Recreate Vulkan swapchains on surface/size changes without destroying guest game
state. Pause presentation/audio as required on backgrounding; resume with valid
surface ownership. Test both landscape rotations, Home/resume and surface loss.
Use display/window insets for safe areas and cutouts. Keep usable content bounds
distinct from full display aspect so HUD semantics and touch coordinates agree.

SDL gamepad events and multi-touch pointer IDs feed a sampled PSP controller state.
Track each finger independently; cancel contacts on focus loss; handle disconnects
and mapping changes. Polling input must not change the emulated simulation rate.

## Memory, stack and diagnostics

Generated code may create deep native call chains. Yakumo documents a 64 MiB host
stack for its game; that is not a discovered Tekken requirement. Azazel6's 64 KiB
guest stack experiment is a different address space entirely. Measure host stack
high-water marks and chaining depth before selecting the execution thread's stack.
Budget guest RAM, generated code, GPU buffers, texture caches and native heaps separately.

Keep unstripped binaries/build IDs for `ndk-stack`/LLVM symbolization of tombstones
and logcat. Record guest PC, import NID, module generation and last lifecycle event
alongside a native crash. Avoid logging payload bytes or personal paths by default.
Real-device thermal, memory-pressure and long-session tests remain BLOCKED on a
playable stock host. See [exact smoke commands](../development/building.md).
