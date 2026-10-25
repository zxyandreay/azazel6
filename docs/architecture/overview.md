# Architecture and runtime boundaries

**VERIFIED implementation:** the root project contains an independently testable
HUD math library, Python input/analysis tools and optional integration of a pinned
PSPRecomp core. The Tekken profile owns executable identity and the entry probe.

```text
User's decrypted ELF -> strict profile validation -> PSPRecomp analysis/generation
                                       |                         |
                              module/import report         local C++ only
                                                                 |
Android/desktop host <- Tekken bootstrap + HLE policy <- native compiler
                              |
                  generic PSP memory and dispatch
```

The framework supplies Allegrex state, ELF loading, memory access, NID registration,
native function registration, AOT dispatch and fallback interpretation. It does
not supply a ready-to-use generic PSP OS or a Tekken renderer. Yakumo's actual
kernel, Vulkan GE, audio, SDL input and mod implementations live in its game profile;
they require an extraction/design review before reuse.

The newer [PortableKit evaluation](../research/portablekit.md) already exposes
much of that system layer through a generic profile. It is pinned separately
for optional headless comparison, checked vertex decoding and a synthetic Vulkan
readback contract; GE synchronization and depth-address/ownership gaps still
prevent replacing the verified primary runtime. The existing AOT build is unchanged.
Only original bootstrap metadata and checked clock conversion are shared between
the two research hosts; each retains its own kernel and memory ownership.

**PLANNED runtime:** keep guest CPU state and deterministic PSP scheduling on an
owned execution thread. HLE models guest-visible objects and returns scheduling
events. Platform code owns Android surfaces, file descriptors and input queues.
Graphics/audio consume explicit submissions. Do not expose host pointers in guest
registers or let a Java/UI callback mutate guest memory during execution.

Use runtime registrations for verified native overrides. Overlay loading must
invalidate registrations for the replaced guest range, validate the new bytes,
then install matching functions. A fallback has to record coverage and maintain
scheduler boundaries. The entry probe disables interpretation and models settings,
thread lifecycle, priority scheduling, shared stack/heap ownership and resident
module identity. Other imports stop. No default-success syscall behavior is
installed. See [native bring-up outcomes](../milestones/002-native-bring-up.md).

See [the decision](../decisions/ADR-0001-framework-strategy.md),
[memory/recompilation](recompilation.md) and [Android plan](android.md).

## Focused Yakumo reuse review (2026-10-23)

**VERIFIED source inspection:** the read-only full reference checkout still matches
`39ac05f1a166703430d18826bc5877f4feb6b511`. The following paths are relative to
[`profiles/mhp3rd/host`](https://github.com/TeamGDB/Yakumo/tree/39ac05f1a166703430d18826bc5877f4feb6b511/profiles/mhp3rd/host).
Source availability does not demonstrate compatibility with Tekken.

| Component | Reuse assessment and next seam |
| --- | --- |
| Core memory, dispatch, interpreter | Already reused from the pinned framework. Keep interpreter disabled until it preserves service/scheduler boundaries. |
| Kernel threads, synchronization, timers, interrupts | `kernel/kernel.*` contains useful context-switch and owned-callback models, but includes settings, load tracing and fast-loading/fast-forward clock policy. Preserve Azazel6's tested ownership and deterministic clock; adapt observed missing contracts individually. |
| Partition memory | `hle/hle_sysmem.cpp` connects to profile HLE globals and GE state. Azazel6 already supplies checked partition/stack/fixed-pool ownership; importing the other allocator would create conflicting owners. |
| GE decode | `gpu/ge_state.*` is substantially backend-independent and uses core guest memory; frame-stat dependencies remain. A promising extraction boundary is decoded vertices/state, with bounded guest reads and Azazel6's list/callback ownership retained. The observed clear vertices now pass through the separately pinned PortableKit decoder under checked bounds; the primary queue retains ownership. |
| Vulkan | `gpu/vulkan_renderer.cpp` imports game HUD, replacement textures, interpolation, installation identity and UI. Reusing the full renderer would pull in title policy. Review texture decoding and primitive/index helpers separately when an observed draw subset is available. |
| Audio | `audio/guest_pcm.*` supplies a small checked staging interface; SAS/ATRAC/sink layers need HLE and host integration. Consider compiling the external PCM helper when a real audio import is reached, with its own range tests. |
| I/O and modules | `kernel/iso_image.*` offers a relatively isolated read-only ISO9660 reader. `hle/hle_io.cpp` also imports mods, translation and fast-loading behavior. Prefer the reader behind a checked Tekken file-handle adapter when boot requests files. |
| SDL input | Upstream input/UI adapters share profile bindings and camera features. Retain the existing synthetic stock Azazel6 input host; connect checked PSP controller services on observed demand. |
| Executable overlays | `overlays.cpp` uses profile slots, generated shared libraries and dump hashes. Core registration/invalidation is reusable; MHP slot/layout assumptions are not. Tekken requires independently verified module identity, ranges and lifetime. |

**PLANNED:** extract only a demonstrated needed seam, retaining upstream notices
and an explicit pinned build dependency. No large profile implementation was
copied, no reference checkout changed, and no architecture rewrite is justified
by the present static setter/coverage boundary. The first useful runtime reuse
candidate is ISO reading or GE decoding, depending on the next observed import
or draw command; speculative subsystem replacement would not advance this stop.
