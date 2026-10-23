# Azazel6

**An independent, unofficial project researching a standalone native Android
ARM64 port of Tekken 6 for PSP.** Azazel6 translates verified Allegrex/MIPS code
ahead of time to C++ and supplies the PSP runtime around it. Users must supply
compatible files from their own game copy.

**Status (2026-10-23): Milestone 002 implemented; Milestones 003 and 004 in
progress. Not playable. No title screen, real game rendering, audio or guest
controls.** The bounded translated-game startup reaches graphics initialization,
two VBLANK returns, GE state setup and guarded graphics-object calls, then
executes the matrix-setter group and stops at guarded resource call `0x08ACF848`. A **separate synthetic** SDL/Vulkan
Android probe APK builds for ARM64 and x86_64, and has run on an official x86_64
AVD. That APK does not execute Tekken 6.

## What works

- Exact executable fingerprint and bounded ELF validation for Tekken 6 USA.
- Sectionless PSP module/GP discovery and 268-import inventory.
- Pinned Yakumo/PSPRecomp framework fetch, native builds and regression tests.
- Real executable analysis and local exploratory AOT generation.
- Bounded translated startup with modeled thread, display/VBLANK and GE initialization
  services. The title-screen boot gate is still open.
- Checked, local-only inspection of the next uncovered helper via
  `python tools/inspect_root.py local/game/EBOOT.BIN --root 0x08ACE2D8`;
  this does not authorize new AOT roots.
- Tested semantic HUD aspect mathematics, with no gameplay hooks installed.
- Android ARM64 cross-compilation of the expanded entry probe and a separate
  synthetic Android Vulkan/input/lifecycle diagnostic APK.

The long-term product is a dedicated Android application, not a PPSSPP frontend
or PRX plugin. AOT still requires memory semantics, kernel/HLE, GE graphics,
audio, file I/O and a controlled fallback for uncovered code. Performance relative
to PPSSPP is unmeasured.

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
See [building](docs/development/building.md) for analysis, generation, the native
probe and Android commands. Game inputs, generated translations and local reports
must remain under ignored paths. No game download or decryption service is provided.

The separate [Android host probe](docs/development/android-host.md) builds a
synthetic Vulkan/input diagnostic APK without game files. It is not a playable
Tekken application.

## Project knowledge

- [Documentation index](docs/README.md)
- [Progress and compatibility](docs/project/progress.md)
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
