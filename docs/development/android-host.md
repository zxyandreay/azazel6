# Synthetic Android host probe

The separate `platform/android` application exercises Android surface ownership,
Vulkan 1.1 presentation and SDL input without a game program. It does not import
EBOOT, link Yakumo or contain generated guest translations. The clear color is
synthetic: dark blue at rest, red when a mapped gamepad button is held and green
when an accepted touch contact is active. There is no menu or touch button overlay.
See [ADR-0003](../decisions/ADR-0003-android-host-probe.md).

**VERIFIED, 2026-10-22:** both ARM64 and x86_64 host APKs build, sign and
pass ELF/ZIP 16 KiB alignment checks. The x86_64 APK executed on the official
Android Emulator 36.5.10 using WHPX, an API 37 preview Google Play 16 KiB x86_64
image and SwiftShader Vulkan through Goldfish GFXStream. A matching native ABI
was used; the image's ARM64 translation capability is not ARM64 execution evidence.

The automated AVD run verified installation, launch, Vulkan presentation with
captured pixels, touch down/up, a virtual controller's connection/button/release/
held disconnect, three Home/resume cycles retaining the process and clearing
contacts, and 30 seconds of subsequent advancing frames. It reached frame 3000.
Captured center pixels were (13,20,46) at rest, (13,128,46) for touch and
(128,20,46) for the controller. This is synthetic host execution, not game rendering.
Physical ARM64 GPU/controller behavior, multitouch and long sessions remain unverified.

The first attempt correctly failed when a System UI ANR dialog obscured the app;
the clean run passed after the boot-time dialog was dismissed. Early console key
injection did not reach SDL, and shell `input keyevent` used virtual device -1.
The supported Android `uinput` command provided a discoverable virtual controller.
Testing exposed and fixed landscape orientation and selection of newly attached
controllers when the emulator's built-in D-pad was already open.

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
Local builds and official AVD execution are the default validation path. Both
GitHub workflows are manual-only: pushes and pull requests do not start Actions
runs. Keep the `Synthetic Android host` workflow as an explicit fallback if local
emulation is unavailable. It uses no game inputs and does not upload keys or
APKs. When manually dispatched, it builds x86_64 and runs the same smoke test on
an API 35 Google APIs AVD with KVM and SwiftShader, retaining synthetic runtime
logs, screenshots and results as a short-lived CI artifact. Workflow configuration
alone is not evidence of a passing Android run.

The driver defaults to ARM64/API 29, links against the system Vulkan loader, creates
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
  analog axes. A newly connected pad becomes active, so an emulator D-pad cannot
  monopolize input. Disconnect closes its handle and clears state; a remaining pad
  can replace it. SDL mappings are sampled afresh each iteration.
- Touch identity is `(device, finger)`, capped at 16 contacts. Coordinates map
  through the window safe area and a centered stock 480x272 viewport. Invalid
  values, letterbox contacts and invalid moves reject/cancel. Focus loss,
  backgrounding and geometry changes clear contacts. No global HUD transform or
  virtual button mapping is implied.
- Host sampling/presentation never advances a guest clock. A future observed
  `sceCtrl` contract must connect sampled state to guest code explicitly.

## Repeatable AVD execution

Inspect `emulator -accel-check`, installed system images and `adb devices -l`
before testing. Use the official Android Emulator with a matching x86_64 image,
hardware virtualization (WHPX on Windows or KVM on Linux), Vulkan-capable
SwiftShader graphics and sufficient RAM (the local run used 3072 MiB). Keep a
separate AVD under `local/`; do not modify a user's existing device. Reuse the
installed SDK/NDK/JDK. Local command-line tools were installed under `local/`
from Google's repository metadata with the published archive checksum verified.
On Windows, command-line tool batch launchers can mishandle `!` in paths; invoke
their Java entry point directly when necessary.

```powershell
python tools/android_host.py --abi x86_64
python -m pip install -r tools/requirements-android-test.txt
python tools/android_runtime_smoke.py --serial emulator-5556 --apk local/android-host-x86_64/azazel6-host-probe.apk
```

Use the actual serial reported by ADB. The runner requires an emulator, a build
report matching the APK hash, the AVD's primary ABI and a fresh evidence directory.
It uses Android's [uinput test interface](https://android.googlesource.com/platform/frameworks/base/+/HEAD/cmds/uinput/)
for a virtual controller; it does not inject SDL events inside the application.
The latest local passing evidence is under
`local/android-runtime/api37-16k-controller/`. Build reports alone deliberately
retain `device_execution: NOT TESTED`; separate runtime reports identify the exact
APK hash and executed checks. Per-ABI output directories have separate debug keys;
uninstall the diagnostic before switching between differently signed builds.

## Real-device verification gate

Compilation and signing do not verify Android runtime behavior. With an authorized
ARM64 Android device visible to `adb devices`, install the local debug APK:

```powershell
adb install -r local/android-host/azazel6-host-probe.apk
adb shell am start -n org.azazel6.probe/.HostActivity
adb logcat -s SDL/APP SDL AndroidRuntime libc
```

Record the actual device/OS/GPU, first synthetic frame, touch rejection outside
the stock viewport, two independent contacts, controller connect/disconnect,
focus loss, both landscape rotations and repeated Home/resume. Check logcat for
Vulkan/lifecycle failures and resource growth. Clear-color input feedback is a
host diagnostic, not proof of working game input. Test on a 16 KiB device as well
as a 4 KiB device before claiming page-size runtime compatibility. Keep captures
and raw logs local. Visible Tekken menus and advancing GE draw lists remain the
separate Milestone 003/004 exit gate.
