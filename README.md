# Azazel6

**An independent, unofficial project researching a standalone native Android
ARM64 port of Tekken 6 for PSP.** Azazel6 recompiles reviewed Allegrex/MIPS
functions ahead of time and uses a PSP interpreter for uncovered code. Users
must supply compatible files from their own game copy.

**Status (2026-11-22): the first genuine hybrid Android ARM64 debug APK is
built and signed; Milestones 003 and 004 remain in progress. The game is not
playable and the title screen and menus have not been reached.** The APK links
one verified Tekken 6 USA function as ARM64 code into the shared PortableKit
interpreter, PSP kernel, ISO, FFmpeg and Vulkan runtime. It contains no game
files. On an x86_64 emulator with ARM64 translation, the final APK installed,
validated the exact game input, executed that AOT function once and then
interpreted 54,086,580 instructions. Strict HLE stopped that run at a
CancelWakeup context guard. An earlier fontless hybrid APK rendered the actual
Bandai Namco Games opening logo on the same translated emulator. Neither run
establishes physical ARM64 device behavior or title-screen boot. See the
[Android checkpoint](docs/research/hybrid-android-apk-2026-11-22.md).

## What works

- Exact executable fingerprint and bounded ELF validation for Tekken 6 USA.
- Sectionless PSP module/GP discovery and 268-import inventory.
- Pinned Yakumo/PSPRecomp framework fetch, native builds and regression tests.
- A verified one-function Tekken 6 AOT corpus, compiled for Windows and Android
  ARM64, with interpreter fallback in the same PortableKit runtime.
- Exact-ISO Windows hybrid execution with one native AOT call, 73,261,212
  interpreted instructions and a Vulkan opening-logo capture.
- A signed Android ARM64 hybrid debug APK with exact input checks, strict HLE,
  FFmpeg decoding, Vulkan rendering and a pinned fallback CJK font. Its AOT
  counter and interpreter fallback ran on an ARM64-translating x86_64 emulator.
- A separate primary AOT research lane with bounded thread, display/VBLANK and
  GE startup contracts. It has not reached a visible game frame.
- Tested semantic HUD aspect mathematics, with no gameplay hooks installed.
- A separate synthetic Android Vulkan/input/lifecycle diagnostic APK; its
  results are not game-execution evidence.

The long-term product is a dedicated Android application. Startup still needs
correct movie-player, ATRAC, GE and kernel behavior. Performance relative to
PPSSPP is unmeasured. Physical Android hardware, game audio, controls, title
screen and gameplay remain unverified.

**Current limits:** the final Android run stopped at a strict
`sceKernelCancelWakeupThread` context check; a longer Windows hybrid run reaches
the movie-player current-PTS barrier. The primary AOT lane is maintained
separately and still stops at a missing direct function. See
[progress](docs/project/progress.md) and the
[Windows hybrid evidence](docs/research/hybrid-aot-portablekit-2026-11-21.md).

## Start without game files

Prerequisites: Git, Python 3.10+, CMake 3.22+, a C++20 compiler. On Windows use
Visual Studio 2022 Build Tools with Desktop development with C++.

```sh
python -m unittest discover -s tests -v
cmake -S . -B local/build/native
cmake --build local/build/native --config Release --parallel 2
ctest --test-dir local/build/native -C Release --output-on-failure
```

Add the framework and its tests:

```sh
python tools/framework.py fetch
cmake -S . -B local/build/all -DAZAZEL6_FRAMEWORK=ON
cmake --build local/build/all --config Release --parallel 2
ctest --test-dir local/build/all -C Release --output-on-failure
```

For your own decrypted EBOOT, run `python tools/analyze.py local/game/EBOOT.BIN`.
See [building](docs/development/building.md) for analysis, generation and the
native probe, and [hybrid Android build](docs/development/android-game.md) for
the signed game APK. Game inputs, generated translations, APKs and local reports
must remain under ignored paths. No game files are distributed.

The [Android host probe](docs/development/android-host.md) is a separate
synthetic Vulkan/input diagnostic. Its APK does not run Tekken 6.

## Project knowledge

- [Documentation index](docs/README.md)
- [Progress and compatibility](docs/project/progress.md)
- [First hybrid Android APK evidence](docs/research/hybrid-android-apk-2026-11-22.md)
- [Hybrid Android build instructions](docs/development/android-game.md)
- [Roadmap](docs/project/roadmap.md)
- [Milestone 001 evidence](docs/milestones/001-project-foundation.md)
- [Milestone 002 checkpoint and next task](docs/milestones/002-native-bring-up.md)
- [Architecture and boundaries](docs/architecture/overview.md)
- [Framework decision](docs/decisions/ADR-0001-framework-strategy.md)
- [Contributing instructions](AGENTS.md)

## License and independence

Original Azazel6 code is [MIT licensed](LICENSE). External code retains its own
notices; see [third-party provenance](THIRD_PARTY.md). No game executable, ISO,
generated game source, assets, private cheat payloads or mods are distributed.
Tekken and associated intellectual property belong to their respective owners.
Azazel6 is not affiliated with or endorsed by Bandai Namco or Sony.
