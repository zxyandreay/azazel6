# Synthetic Android host probe

The separate `platform/android` application exercises Android surface ownership,
Vulkan 1.1 presentation and SDL input without a game program. It does not import
EBOOT, link Yakumo or contain generated guest translations. The clear color is
synthetic: dark blue at rest, red when a mapped gamepad button is held and green
when an accepted touch contact is active. There is no menu or touch button overlay.
See [ADR-0003](../decisions/ADR-0003-android-host-probe.md).

**VERIFIED, 2026-10-21:** the pinned SDL/native host and Java activity compile;
the ARM64 APK passes APKSigner verification, ZIPAlign 16 KiB checks and ELF
load-segment checks. AAPT2 reports API 29 minimum, SDK 35 target and ARM64 only.
Native dependencies resolve to packaged SDL plus Android system libraries.
53 Python tests and 18 integrated CTest suites pass. **BLOCKED:** `adb devices -l`
lists no attached device; no presentation, lifecycle or physical input claim is made.

## Build

Use installed JDK 17, Android SDK platform 36, build tools 36.0.0, NDK
27.1.12297006 and Ninja from SDK CMake 3.22.1. Set `ANDROID_HOME` and `JAVA_HOME`
to the installed SDK/JDK roots. From the repository:

```powershell
python tools/android_host.py --fetch-sdl
```

Fetching is explicit and only needed for an absent checkout. Subsequent builds
can omit `--fetch-sdl`; both Python and CMake reject a different revision or dirty
SDL source. Java and native SDL components come from the same pinned checkout.
SDK/JDK roots can also be passed through `--sdk` and `--java-home`.
The `Synthetic Android host` GitHub workflow runs the same build on relevant
source/dependency changes and supports manual dispatch. It uses no game inputs
and does not upload keys or APKs.

The driver compiles ARM64/API 29, links against the system Vulkan loader, creates
DEX with D8, packages with AAPT2, aligns with ZIPAlign and debug-signs with
APKSigner. It checks ARM64 ELF load-segment alignment, APK signature and 16 KiB
ZIP alignment before publishing `local/android-host/azazel6-host-probe.apk`.
The local debug key has the conventional public `android` password; it is a
development identity, never a release credential. All intermediates, reports,
keys and APKs remain under ignored `local/`. Each packaging run gets a fresh
directory so deleted Java classes cannot remain in the APK.

## Ownership and input contracts

- The presenter checks Vulkan 1.1, graphics/present queue support, swapchain
  extension, color formats and transfer-destination usage. Unsupported devices
  fail with a diagnostic. It submits a clear only, without GE emulation.
- One submission is in flight. Acquiring an image and waiting for a submission
  use 100 ms timeouts. Present semaphores belong to swapchain images, not frame
  slots. Device-idle calls serialize teardown; a hung driver can still block them.
- An SDL event watch suspends presentation before Android backgrounding. A mutex
  serializes the callback with Vulkan ownership. The watch is removed before
  presenter/window destruction. Surface loss and resizing recreate resources;
  device loss terminates with a diagnostic.
- The first available SDL gamepad is sampled using PSP button bits and unsigned
  analog axes. Disconnect closes its handle and clears state; a remaining pad
  can replace it. SDL mappings are sampled afresh each iteration.
- Touch identity is `(device, finger)`, capped at 16 contacts. Coordinates map
  through the window safe area and a centered stock 480x272 viewport. Invalid
  values, letterbox contacts and invalid moves reject/cancel. Focus loss,
  backgrounding and geometry changes clear contacts. No global HUD transform or
  virtual button mapping is implied.
- Host sampling/presentation never advances a guest clock. A future observed
  `sceCtrl` contract must connect sampled state to guest code explicitly.

## Real-device verification gate

Compilation and signing do not verify Android runtime behavior. With an authorized
ARM64 Android device visible to `adb devices`, install the local debug APK:

```powershell
adb install -r local/android-host/azazel6-host-probe.apk
adb shell am start -n org.azazel6.probe/.HostActivity
adb logcat -s SDL
```

Record the actual device/OS/GPU, first synthetic frame, touch rejection outside
the stock viewport, two independent contacts, controller connect/disconnect,
focus loss, both landscape rotations and repeated Home/resume. Check logcat for
Vulkan/lifecycle failures and resource growth. Clear-color input feedback is a
host diagnostic, not proof of working game input. Test on a 16 KiB device as well
as a 4 KiB device before claiming page-size runtime compatibility. Keep captures
and raw logs local. Visible Tekken menus and advancing GE draw lists remain the
separate Milestone 003/004 exit gate.
