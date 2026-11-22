# Android ARM64 preparation

**VERIFIED 2026-11-22:** a signed API 29+ Android ARM64 hybrid game APK now
links one exact Tekken 6 AOT function with PortableKit's shared interpreter,
kernel, ISO, FFmpeg and Vulkan host. It installed and executed its AOT and
fallback paths on an x86_64 AVD with ARM64 translation. The final package
stopped at a strict CancelWakeup guard; an earlier fontless package rendered
the game opening logo. Physical ARM64, graphical ISO picker and title/menu
behavior remain unverified. See [Android evidence](../research/hybrid-android-apk-2026-11-22.md)
and [build instructions](../development/android-game.md).

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

Both the synthetic diagnostic and the hybrid game APK use SDL3's Android
activity and Java classes from the pinned SDL source. The synthetic
`libmain.so` owns a checked Vulkan clear presenter and sampled input adapter;
the game package links the shared PortableKit host instead.
[ADR-0003](../decisions/ADR-0003-android-host-probe.md) selects SDK-tool
packaging for the diagnostic. The hybrid APK also uses SDK tools and local
debug signing; a distribution build process remains planned.

PortableKit's Android app includes a file picker for user game imports. Its
graphical behavior has not been verified on the translated AVD; the measured
runtime used exact inputs placed in app-private storage by `run-as`. A content
URI is not a filesystem path: import through a granted descriptor or bounded
streaming copy into app-private storage, hash the copied bytes, reject
incompatible executables and commit the import atomically. Preserve user
permission grants and report disk-space, cancellation and partial-copy errors.
Never request broad storage access just to avoid URI handling. Separate
immutable game input, caches, saves and diagnostics.

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
