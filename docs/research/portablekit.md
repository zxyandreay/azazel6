# PortableKit compatibility investigation

**2026-10-24: optional research integration; primary AOT architecture unchanged.**
The user suggested [PortableKit](https://github.com/TeamGDB/PortableKit).
Revision `33800c4060c53f3233e08f8ab2cc4590a0872063` is pinned separately in
`dependencies.json`. Its README describes a generic system layer extracted from
Yakumo; Yakumo's existing copy had not migrated to it. Neither another game's
playable status nor an upstream interpreter run establishes Tekken compatibility.

## Verified local results

- The unchanged Windows host builds with renderer and FFmpeg disabled, without
  downloading media dependencies. Sectionless module discovery recognizes the
  exact input, entry `0x08804124`, GP `0x08C022B0` and 268 imports.
- With strict imports, unadapted execution stops after 13 interpreted instructions
  at SDK-version import `SysMemUserForUser::0x91DE343C`. It registers zero fallback
  logging/success stubs. That mode does not prove every explicitly named upstream
  handler implements its full PSP contract.
- An exploratory local harness compiling Azazel6's existing SDK/compiler metadata
  handlers unchanged progressed to a 1,000,000-dispatch limit at `0x089FD4B8`,
  with file-worker threads and repeated GE/display calls. No ISO, renderer or
  audio was supplied. The trace is **diagnostic only**, not verified title boot:
  known semantic gaps below prevent promoting that run to milestone evidence.
- The reproducible probe initially reproduced that wide-clock refusal. It now
  shares Azazel6's checked conversion helper, using PortableKit's live thread
  stack owner. Metadata and clock compatibility tests pass against the external
  core. With this adapter, the bounded no-disc run reaches its 100,000-dispatch
  limit at `0x08AC6C64`, with two waiting event workers and file-worker threads.
  This remains upstream diagnostic evidence, not title boot or native AOT progress.

## Admission and reproduction

Clone the official repository under ignored `local/deps/portablekit` and check
out the pinned revision above. Both configuration and the runner reject a
different revision or a dirty checkout; neither resets or edits the dependency.

```sh
python tools/portablekit_probe.py build
python tools/portablekit_probe.py run local/game/EBOOT.BIN --output local/portablekit-investigation
```

The output must be a fresh directory under `local/`. The executable is checked
before analysis or execution, again inside the native program, and its module
identity is independently checked. Runs have a 15-second wall limit and 100,000
dispatch limit. Inherited upstream switches are removed; settings and memory-stick
paths stay local, strict imports are enabled and busy-clock/fast-loading options
are disabled. No installer, decryption, ISO, generated corpus or Vulkan surface is
used by this headless target. Output is private diagnostic evidence.

## Reuse decision and concrete gaps

**VERIFIED source review:** `host/profile.hpp` removes many Monster Hunter
constants from kernel, ISO, GE, Vulkan, audio and input services. Its external
`portablekit_add_game` interface is a substantially better starting point for
future whole-host reuse than copying Yakumo's game profile. However:

| Component | Decision / dependency |
| --- | --- |
| Core/bootstrap | Optional side-by-side evaluation uses the existing original metadata handlers; no core lock or production configuration change. |
| Clock conversion | `hle_threadman.cpp` returns a remainder for a null-first wide-clock output. The profile overrides this with the same checked helper used by the primary runtime, including atomic output preflight and live-stack ownership. |
| GE synchronization | `hle_media.cpp` DrawSync unconditionally returns zero, including potential pending work. Azazel6 retains its tested completed-queue predicate and refusing asynchronous contract. |
| GE decoder and Vulkan | PortableKit separates profile policy and exposes backend-independent decoded draw calls. Prefer external compilation of that interface, with checked producer/vertex ownership, when integrating observed draws. Headless execution is not rendering evidence. |
| File services | `kernel/iso_image.*` offers a read-only ISO9660 reader; validate the supplied disc independently before mounting. The exploratory no-disc loop is not a filesystem success test. |
| Scheduling/overlays | Keep separate runtime owners. Verify module/thread initialization, interrupt/callback order, clock policy and exact overlay ranges before replacing existing behavior. |
| Audio/input | Available upstream; not exercised here. Media-disabled behavior cannot establish stock audio or movie progression. |

**PLANNED:** resolve the remaining compatibility gaps in a checked profile adapter,
then compare bounded traces with a validated disc and review actually invoked
services. Adopt larger components only with an explicit architecture decision
and regression evidence. Do not disable strict admission or import refusal to
make the comparison advance.

**VERIFIED validation:** 79 Python cases cover input refusal, revision/dirty
checkout rejection and environment isolation in addition to the existing suite.
The standalone target compiles on Windows and both bootstrap and clock
compatibility suites pass. At that initial checkpoint its Android, renderer, ISO and audio paths had not
been tested; the later Windows renderer contract is recorded below. All 20 primary CTest suites, including clock ownership/refusal tests,
pass; the primary Android ARM64 target also compiles with the shared helper.

PortableKit/PSPRecomp MIT notices are retained in
[THIRD_PARTY_LICENSE_PORTABLEKIT.txt](../../THIRD_PARTY_LICENSE_PORTABLEKIT.txt).
The unchanged external checkout retains notices for its dependencies. Shipping
SDL, FFmpeg, fonts or other optional assets requires their own applicable notices;
none of those binaries or assets is committed by this integration.


## Checked clear-vertex reuse (2026-10-25)

**VERIFIED:** the external `gpu::decode_vertices` implementation is now used by
an original bounded adapter for the observed `0x80011C` vertex format. Admission
requires aligned canonical RAM in one supplied lease, 2..64 vertices in pairs,
and a complete 12-byte-per-vertex extent. Decoded output must describe ordered
rectangles with constant per-pair color/depth in the reviewed through-mode domain.
This adapter does not render or complete a GE command. Allocation lifetime remains
the submitting runtime's responsibility; an offline packet contains copied bytes.

The native probe captures at most 64 vertices inside the failed GE list lease.
The optional tool rechecks the executable hash and external source pin, requires
one exact-input trace marker and complete contiguous words, then sends a bounded
local packet to the external decoder. Trace files and decoded vertices stay local.

```sh
python tools/portablekit_probe.py build
python tools/portablekit_probe.py decode-clear local/game/EBOOT.BIN --trace local/native-run/execution.log --output local/clear-evidence
```

**VERIFIED actual captured data:** all 30 vertices decode into fifteen adjacent
32-by-272 rectangles covering x=0..480, with color 0 and depth 16. The first pair
is (0,0)..(32,272), and the last (448,0)..(480,272). The original run still stops
at PRIM `0x09BF42B0`; no pixels or successful draw are claimed. Evidence is under
`local/re-clear-trace-execution` and `local/re-clear-decoded`.

**VERIFIED synthetic tests:** nonzero packed colors, unsigned near-maximum depth,
padding, count limits, truncated/misaligned/aliased ranges, overflow, negative
coordinates and inconsistent rectangles. Three optional compatibility suites and
80 Python cases pass. The primary Windows/ARM64 probe compiles with bounded trace
capture. No reference checkout was modified.

**DOCUMENTED source limitation:** upstream `run_ge_list` catches GE errors and
marks the list finished; its decoder can also skip malformed primitive data.
Those policies are unsuitable for the primary strict queue. Reuse the decoder
behind preflight checks and a verified render sink, not the upstream completion
policy. The observed initial target also aliases color/depth addresses; renderer
attachment and VRAM readback semantics need explicit validation before adoption.


## Optional Vulkan contract (2026-10-25)

**VERIFIED:** a separate Windows renderer build compiles the unchanged pinned
PortableKit backend and GE shaders. An original synthetic test submits fifteen
alternating red/green clear sprites, waits for framebuffer readback, and checks
all **139,264** words in 272 rows at stride 512: 480 visible pixels per row plus
32 untouched padding words. This passed on the local Radeon RX 560X / Vulkan
1.3.260 host. An empty window or a skipped draw cannot satisfy the colored-pixel
and sentinel checks. No game executable runs in this test, and depth readback,
Android integration, general geometry and texture behavior are not claimed.

The default build remains headless. Renderer evaluation must be requested with
`AZAZEL6_PORTABLEKIT_RENDERER=ON`; missing dependencies produce a configuration
error rather than silently building headless. Configure a separate build directory
with SDL3, Vulkan headers/import library and glslang available, then run:

```sh
cmake -S profiles/tekken6/portablekit -B local/build/portablekit-vulkan -DAZAZEL6_PORTABLEKIT_RENDERER=ON
python tools/portablekit_probe.py build --build local/build/portablekit-vulkan
python tools/portablekit_probe.py render-test --build local/build/portablekit-vulkan --output local/vulkan-contract
```

Use CMake's `SDL3_DIR`, `Vulkan_INCLUDE_DIR`, `Vulkan_LIBRARY` and
`PORTABLEKIT_GLSLANG` overrides for local dependencies when needed. This machine
used the already pinned SDL 3.4.18 checkout, official installed NDK 27.1 Vulkan
headers, Android emulator glslang 14.3.0 and an import library generated from the
installed Windows Vulkan loader exports. Build artifacts stayed under `local/`;
no system installation or reference-source change was needed. The wrapper uses a
fresh local data directory and a 30-second process limit. The test may briefly
open a renderer window; pixel readback, not UI inspection, is the verification.

**VERIFIED source gap:** the Vulkan backend does not consume `DrawCall`'s guest
depth-buffer address. Tekken's first clear aliases color and depth addresses, so
blindly installing this sink would not establish the required guest-memory
behavior. The primary runtime instead executes its independently tested bounded
clear into VRAM, preserving reference-supported depth-before-color order. Reuse
of the full Vulkan backend remains optional pending depth/alias and completion
contracts. The adapter's three headless suites and all 81 Python cases pass.
