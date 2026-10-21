# Setup and reproducible builds

Requirements: Git, Python 3.10+, CMake 3.22+, C++20. Windows was tested with Visual
Studio 2022 Build Tools/MSVC 19.43.34809, Windows SDK 10.0.26100.0 and Python 3.14.4.
Linux uses GCC/Clang and CMake; CI is Ubuntu 24.04/Python 3.12. No Python packages
are needed. Run commands from the repository root.

## Native framework and tests

```sh
python tools/framework.py fetch
python tools/framework.py build
python tools/framework.py test
python -m unittest discover -s tests -v
cmake -S . -B local/build/native
cmake --build local/build/native --config Release --parallel 2
ctest --test-dir local/build/native -C Release --output-on-failure
```

The framework helper configures `-DPSPRECOMP_PROFILE=`. Its source defaults to
`local/deps/yakumo`; `--source` can select another clean checkout at the locked
commit. Existing mismatched/dirty checkouts are refused, not reset. The source lock
is an exact Git commit; generated reports also record tool binary SHA-256 values.
For one integrated build use `-DAZAZEL6_FRAMEWORK=ON` with the root CMake project.

Windows multi-config tools are in `local/build/framework/Release`. On Linux
single-config tools are directly in `local/build/framework`; add
`-DCMAKE_BUILD_TYPE=Release` to a manual configure when desired. Builds use at most
two jobs. In the restricted Windows agent sandbox, MSBuild stalled at its first
custom rule; ordinary authorized native execution resolved this. It was not a
source/build-system defect.

## Local game analysis and generation

Place your decrypted ULUS10466 EBOOT at `local/game/EBOOT.BIN` (or pass its existing
path). Expected size/hash and format are in the profile. Never rename an encrypted
file or ISO and assume it is decrypted.

```sh
python tools/analyze.py local/game/EBOOT.BIN
python tools/analyze.py local/game/EBOOT.BIN --tools-dir local/build/framework/Release --output local/analysis/full --generate
```

On Linux remove `/Release`. Outputs include `report.json`, raw upstream JSON/CSV,
logs and `generated/`. The profile module/import report supplements the raw upstream
section-based report. Generation defaults to a 300-second subprocess timeout. A
failed stage records an error and completed stages in `report.json`. Use a fresh
`--output` directory for a new generation; a nonempty generated directory is refused
to prevent stale units entering a later build. No tool downloads game data.

The 1,015-unit exploratory corpus has not been fully compiled and has known
code/data/import limitations. Do not interpret generation success as a game build.

## Bounded native entry probe

```sh
python tools/entry_probe.py local/game/EBOOT.BIN --tools-dir local/build/framework/Release
cmake -S . -B local/build/probe -DAZAZEL6_FRAMEWORK=ON -DAZAZEL6_ENTRY_DIR=local/entry-probe -DBUILD_TESTING=OFF
cmake --build local/build/probe --config Release --target azazel6_entry_probe --parallel 2
python tools/run_probe.py local/game/EBOOT.BIN --binary local/build/probe/profiles/tekken6/Release/azazel6_entry_probe.exe
```

For faster coverage iterations, configure with `-DAZAZEL6_PROBE_FAST_BUILD=ON`.
This disables optimization only for the research probe; the default remains
optimized, and kernel tests/framework settings are unchanged.

On Linux remove `/Release` and `.exe`. The generator discovers a bounded startup
control-flow graph, follows both branch paths and call continuations, and excludes
callees. Unsupported indirect transfers and invalid delay slots fail closed. The
current startup profile yields 23187 instruction words, including delay slots. Regenerate
older probe outputs: the current host requires generated `imports.cpp` wrappers.
The native process
returns 3 at a research boundary; the runner records it and returns 0 only to mark
the experiment as completed. Inspect `execution.log` and `execution.json`; this is
never a game-boot success. The runner limits native execution to 15 seconds, the
host to 32768 dispatches, and interpreter fallback is explicitly disabled. The two
[settings services](../research/startup-imports.md) and bounded
[dormant thread creation/deletion](../research/dormant-threads.md) are implemented;
StartThread and exit services now support bounded priority handoff; waiting, timers and general scheduling remain unimplemented. Dispatch exhaustion, uncovered code and rejected
SDK versions are experiment failures, not completed research boundaries.

## Android cross-compilation

Tested NDK package: `27.1.12297006`. Set `ANDROID_NDK_HOME` to that installed NDK,
and place Ninja on PATH, or supply `--ninja` with an installed SDK Ninja executable.

```sh
python tools/android_smoke.py --ndk "$ANDROID_NDK_HOME"
python tools/android_smoke.py --ndk "$ANDROID_NDK_HOME" --entry-dir local/entry-probe
```

PowerShell uses `$env:ANDROID_NDK_HOME` instead. For example, with the tested SDK:

```powershell
python tools/android_smoke.py --ndk "$env:ANDROID_HOME/ndk/27.1.12297006" --ninja "$env:ANDROID_HOME/cmake/3.22.1/bin/ninja.exe" --entry-dir local/entry-probe
```

The smoke build uses `arm64-v8a`, API 26, and disables host-executed tests during
cross-compilation. It compiles native components only. The separate
[synthetic Android host probe](android-host.md) builds an SDL/Vulkan diagnostic
APK without game inputs. Guest rendering, product Gradle integration and real-device
execution remain separate work.

## Repository audit

Stage only explicitly reviewed source/configuration/docs, then run:

```sh
git diff --cached --check
python tools/repository_check.py
git diff --cached --stat
```

The audit checks the index for forbidden paths, binary signatures, large files,
credential patterns, documentation links and required ignore coverage. It complements
manual review; it cannot prove arbitrary renamed content is redistributable.
