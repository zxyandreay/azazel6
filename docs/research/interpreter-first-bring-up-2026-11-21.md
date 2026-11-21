# Interpreter-first Tekken 6 bring-up comparison

**2026-11-21, bounded Windows host experiment.** This compares the current
Azazel6 AOT checkpoint with an isolated profile built against pinned PortableKit
`33800c4060c53f3233e08f8ab2cc4590a0872063`. It changes neither the
primary runtime nor Yakumo's external checkout. All game inputs, logs, generated
code, screenshots and build artifacts remain ignored under `local/` or are the
user's separately supplied ISO.

## Strategy audit

**DOCUMENTED:** [PortableKit's bring-up guide](https://github.com/TeamGDB/PortableKit/blob/33800c4060c53f3233e08f8ab2cc4590a0872063/docs/BRINGING_UP_A_GAME.md)
starts with interpreter execution against a checked disc, uses strict HLE and
subsystem traces to find the first real blocker, and delays the expensive broad
AOT compile until the game reaches a meaningful screen. Its interpreter and
generated code share the same kernel/HLE/GE host. Its final validation disables
interpreter fallback through opening gameplay. This is a development sequence,
not proof that any other game's profile works for Tekken.

**DOCUMENTED:** [Yakumo](https://github.com/TeamGDB/Yakumo) demonstrates a
shipping Android ARM64 Vulkan/SDL game profile and broad recompilation for a
different PSP title. [PortableKit's architecture](https://github.com/TeamGDB/PortableKit/blob/33800c4060c53f3233e08f8ab2cc4590a0872063/docs/ARCHITECTURE.md)
extracts a generic core and system layer with interpreter, ISO services, GE
decoder and Vulkan backend. The pinned Azazel6 primary Yakumo core already has
an interpreter, but the current primary run explicitly disables fallback and
the host implements only the reviewed Tekken startup subset. The optional
PortableKit profile is the faster way to discover the whole-system path.

**VERIFIED:** Azazel6's strict per-root approach provides stronger reviewed
ownership, scheduler and GE completion contracts for its covered path, but
continuously adding one direct startup root does not by itself establish a
visible frame. The separate PortableKit probe can reach later game work without
adding those roots. Its upstream HLE/GE answers are not automatically equivalent
to the primary contracts: the clock adapter, GE completion and color/depth alias
issues in [PortableKit reuse](portablekit.md) remain relevant.

## Exact input and measured comparison

The decrypted ELF is 4,158,480 bytes with SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The explicitly supplied ISO is 855,638,016 bytes with SHA-256
`45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8`.
The primary media provider independently validated ISO9660 geometry, bounded
extents, ULUS10466 SFO identity and 68 entries. The optional runner checks the
hash before launch; its native process repeats the hash and structural check
before it waits for a one-byte execution grant. Both executions are bounded
after the grant to 15 wall seconds. No substituted region or media was used.

| Measurement | Primary AOT, exact ISO | PortableKit interpreter, exact ISO |
| --- | ---: | ---: |
| Guest coverage | 1,134 reviewed roots / 65,487 words | No AOT corpus; interpreter |
| Stop | Missing direct AOT function `0x0886546C` | Successive strict barriers: `sceKernelCancelWakeupThread` (`0xFCCFAD26`), decoder-disabled `sceAtracReinit(0, 3)`, then `scePsmfPlayerGetCurrentPts` (`0x3ED62233`) after existing movie setup was bound |
| Display/GE calls | 16 display frames, 15 GE lists | 19 VBLANKs/17 enqueues before wakeup handler; 188/103 with real decoder; 657/337 at the movie-player barrier |
| Modeled guest time | 266,933 µs | 383 ms after wakeup handler |
| Output pixels | 0 nonblack pixels in 480×272 scanout | The bounded 100,000-dispatch Vulkan run captured the Bandai Namco Games opening logo at 960×544; the later 250,000-dispatch stop captured a black transition frame |

**VERIFIED:** the primary run returned failure at the uncovered direct function
and captured a black scanout. The PortableKit run stopped on real missing imports
with zero logging-success stubs; its no-disc control instead exhausted 100,000
dispatches in a GE/display loop. On the exact disc the initial strict run
interpreted 3,827,887 instructions and visited 651 unique addresses before
`sceKernelCancelWakeupThread`. A profile-scoped handler copied the already
tested Azazel6 count-clearing contract, with synthetic null, dormant, ready and
waiting cases. The rerun reached 814 unique addresses and
`sceAtracReinit(0, 3)`, reporting 37 currently unimplemented imports out of 268.
It interpreted 47,959,668 instructions; repeated interpreter work and different
guest clock policies make these numbers unsuitable for a performance comparison.

**VERIFIED:** with FFmpeg disabled, PortableKit deliberately does not register
ATRAC handlers. The next stop is a real decoder dependency. Returning zero at
that import without an available decoder would conceal unsupported behavior.
The optional build now permits an explicit FFmpeg selection in a separate local
build directory; its default remains OFF. The pinned LGPL decoder build
advanced the exact-ISO headless run to its 100,000-dispatch bound: 73,261,067
interpreted instructions, 2,972 unique addresses, 188 VBLANKs and 103 GE
enqueues. Four optional CTest suites passed in the Vulkan/decoder build.

**VERIFIED first visible game output, research lane only:** with that decoder
and the pinned Vulkan runtime enabled, the same 100,000-dispatch run captured
a 960×544 Bandai Namco Games opening logo (1,787 distinct RGB colors) from the
game target. The local BMP SHA-256 is
`0f268c2a92c873dc6b1c4d0450b0ae0264b2c6bec336bdc85c59fb3a35fc69a1`.
An independent repeat with the final profile and the same 100,000-dispatch
limit produced the identical BMP hash and 522,240 nonblack target pixels.
No capture or game asset is committed. A bounded 250,000-dispatch continuation
reached a black transition frame at the first missing movie-player callback
NID `0x58B83577`. [The pinned PortableKit source](https://github.com/TeamGDB/PortableKit/blob/33800c4060c53f3233e08f8ab2cc4590a0872063/host/hle/hle_psmfplayer.cpp)
already has a `scePsmfPlayerSetPsmfCB` branch guarded by a NID-table lookup;
the [pspdev NID database](https://github.com/pspdev/psp-ghidra-scripts/blob/master/ppsspp_niddb.xml)
identifies that exact NID. Adding only the mapping to this profile exercised
the existing checked movie setup and reached the next strict stop,
`scePsmfPlayerGetCurrentPts` (`0x3ED62233`), after 657 VBLANKs and 337 GE
enqueues. The last capture there remained black. This is a real media/HLE gate
before the title screen, not a reason to enlarge AOT roots.

**VERIFIED source limitation:** PortableKit's current `sceAtracReinit` handler
returns success without modeling the requested ID pool partition. Its
`sceGeDrawSync` and some other host paths also have weaker completion/error
contracts than the primary runtime. Thus post-ATRAC interpreter progress and
the opening logo are diagnostic evidence for the graphics path; they do not
establish correct movie, audio, scheduling or gameplay behavior. No
success-return stub was added in Azazel6. Correct ID ownership and current-PTS
semantics need reviewed contracts before claiming title boot.

## Broad AOT feasibility and GE/Vulkan reuse

**VERIFIED:** the pinned PortableKit recompiler generated 1,015 source units,
10,072 function seeds and 458,194 emitted code PCs from the exact ELF in 45.84 s.
The generated corpus totals 271,160,594 C++ bytes in 1,016 `.cpp` files,
including registration code, and contains 62,036 `unsupported(...)` sites.
The largest groups are 35,401 control-flow-in-delay-slot sites, 10,755
`special?` sites and 8,540 `unknown` sites. The earlier Yakumo generation
produced 65,027 such sites. Broad discovery can treat data as code; these
static counts are not executed failures, but they rule out treating generation
as compile-ready coverage. No broad corpus was compiled or executed.

**VERIFIED synthetic only:** the existing pinned PortableKit Vulkan contract
again decoded and drew fifteen colored clear rectangles and checked all 139,264
framebuffer and padding words on the Windows Radeon RX 560X. This demonstrates
a reusable GE decoder/render path, not Tekken scene rendering. The backend does
not yet consume the draw call's guest depth-buffer address, and the observed
Tekken clear aliases color and depth. The primary runtime's checked clear and
GE completion rules must not be replaced with upstream success behavior without
equivalent tests.

## Architecture decision and next gate

**RECOMMENDED:** keep the primary implementation intact. Use the pinned
PortableKit system layer as an isolated interpreter-first bring-up lane, sharing
only explicitly reviewed Tekken profile semantics. The opening logo proves
that the existing GE/Vulkan path can render real game output. Implement and
test movie-player current-PTS and ATRAC ID ownership next, then trace the
first title/menu draw with checked list/vertex/depth ownership. Verify a
repeatable visible Tekken scene and input before broad AOT compilation becomes
the priority; address generated unsupported categories and code/data
classification before paying that cost. Validate subsequent opening and
gameplay with interpreter fallback disabled before replacing the primary
architecture. This sequence targets actual rendering rather than a larger
root count.

**BLOCKED:** the visible logo is not a Tekken title or gameplay frame. The
next strict barrier is movie-player current-PTS handling, with ATRAC ID-pool
and GE completion semantics still requiring validation. Android game graphics,
physical ARM64 GPU/input behavior and playability remain unverified.

## Reproduction

Replace `"<exact.iso>"` with the user's private exact ISO path; do not commit
or echo that path into source. Outputs must be fresh paths under `local/`.
The Vulkan build needs the local SDL3, Vulkan and glslang paths described in
[PortableKit reuse](portablekit.md); the bundled FFmpeg archive is pinned and
hash-checked by PortableKit at configure time.

```sh
python tools/portablekit_probe.py build
python tools/portablekit_probe.py run local/game/EBOOT.BIN --iso "<exact.iso>" --output local/portablekit-disc-check
python tools/portablekit_probe.py build --build local/build/portablekit-vulkan --renderer --ffmpeg bundled
python tools/portablekit_probe.py render-game local/game/EBOOT.BIN --iso "<exact.iso>" --build local/build/portablekit-vulkan --output local/portablekit-game-frame-check
python tools/portablekit_probe.py render-game local/game/EBOOT.BIN --iso "<exact.iso>" --build local/build/portablekit-vulkan --dispatches 250000 --output local/portablekit-movie-barrier-check
python tools/run_probe.py local/game/EBOOT.BIN --binary local/build/probe/profiles/tekken6/Release/azazel6_entry_probe.exe --iso "<exact.iso>" --capture-framebuffer --output local/native-disc-check
python tools/portablekit_probe.py render-test --build local/build/portablekit-vulkan --output local/portablekit-vulkan-check
```

For broad generation, first run `tools/elf.py` validation and confirm the clean
PortableKit pin; then use its `psp_recomp` with
`--auto <fresh-local-generated-dir> 0x08804000 4096 --prefix tekken6` and a
300-second process limit. Keep the C++ corpus local. This experiment used no
GitHub Actions.
