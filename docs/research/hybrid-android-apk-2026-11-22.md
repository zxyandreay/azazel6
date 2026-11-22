# First genuine hybrid Android APK checkpoint

**VERIFIED build, 2026-11-22.** The project built and signed an installable
`arm64-v8a` APK from pinned PortableKit, SDL3 and FFmpeg. The Android native
library contains a reviewed Tekken 6 USA function at `0x08804124`, generated
from the exact executable and compiled as AArch64 code. The shared PortableKit
runtime dispatches uncovered guest code to its interpreter; no second PSP
kernel, ISO mount, decoder or renderer state was introduced. The APK contains
no ISO, EBOOT, generated source or other game input.

The one-function generated source has SHA-256
`fd6592155b90db1fa9a5954db31b420027f3f00dab4b292b850200ecd0da5e46`.
The accepted source ELF has SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The runtime checks the exact ISO, encrypted disc EBOOT, prepared ELF, module,
GP and import layout before guest execution. Its strict HLE table contains
zero logging stubs. The CMake build links FFmpeg 7.1.5 decoders for ATRAC3,
ATRAC3plus and H.264. The package check confirms an AArch64 ELF, 16 KiB LOAD
alignment, Android 29 strong import closure, APK signature and ZIP alignment.
The final package also carries the upstream SHA-pinned Noto CJK font and license
for game text. Its signed APK SHA-256 is
`a1b1103f7e1801a65f7a85dc510702a534c531f5116f4125ec8341994ad925b6`.
ZIP integrity passed, and inspection found four ARM64 native libraries and
no ISO, EBOOT or ELF payload.

**VERIFIED runtime for the final font-bearing package, 2026-11-22:** Android
installed and launched the signed APK; the app unpacked the 16,467,736-byte
Noto font and logged game text initialization from it. It rechecked the exact
ISO, prepared ELF and AOT source ELF hashes below. It recorded
`native_aot_entry_calls=1` and then 54,086,580 interpreted instructions
across 62,460 entries and 1,762 unique guest addresses. This run stopped
under strict HLE with `Cancel wakeup requires a running interruptible thread`.
Its log is in ignored `local/android-game/game-final-run.log`.
The stop is an observed result from this run, not a successful
movie-player or title-screen continuation. The `interpreter_instructions_after_aot=0`
line was printed immediately when the AOT entry returned, before interpreter
fallback; the final interpreter summary is the actual fallback measure.

**VERIFIED rendering for the initial fontless package, 2026-11-21:** a local API
37 x86_64 Android virtual device advertised `arm64-v8a` through
`libndk_translation.so`; Android installed the APK and reported the app's
primary ABI as `arm64-v8a`. That earlier APK has SHA-256
`ddcea3673d9f06d1b500dfc7779ee39c1e4e4a012e609deec0dbb5e994d558f4`.
The log rechecked ISO SHA-256
`45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8`,
prepared ELF SHA-256
`60eb876806e8625161c24e65aab6d906fc9d5740777fbcfafe7ef0fa9920c01d`,
and the AOT source ELF hash above. It recorded `native_aot_entry_calls=1`
followed by interpreter per-PC instruction counts above one million. The
bounded run stopped at its 100,000-dispatch limit at `0x0883EB1C`.

The captured 1280x720 Android frame visibly showed the game's Bandai Namco
Games opening logo, with 201,520 nonblack pixels and PNG SHA-256
`07128fef1ad5ce320abc6fd48fadba93e886c4254047c7d56e0892bcba5a007f`.
The logs and capture live only in ignored `local/android-game/`. For these runs,
the exact user-supplied ISO and hash-verified prepared ELF were placed in
app-private storage using `run-as`; this bypassed the graphical first-run
picker, but not runtime identity checks. The picker showed a black surface
on this translated AVD after SystemUI ANR and remains unverified.

**LIMITS:** translated x86_64 execution does not prove behavior on a physical
ARM64 device. The earlier package's logo is actual game rendering, not the
Tekken title screen, functional menus or gameplay. The final font-bearing
run did not establish logo rendering; an emulator System UI ANR obscured its
screen capture. The strict CancelWakeup context, movie-player current PTS,
ATRAC ID-pool and GE completion contracts remain startup work. Audio output
and controller behavior during the game remain untested.

Build commands and the package verifier are in
[Android game build](../development/android-game.md). The independently
verified Windows hybrid run is in
[the host checkpoint](hybrid-aot-portablekit-2026-11-21.md).
