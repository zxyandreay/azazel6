# Renderer, audio and performance plan

**PLANNED:** a Vulkan backend interprets PSP GE state and consumes guest display
lists with explicit ownership and bounded memory reads. Preserve primitive/vertex
formats, skinning, CLUTs, texture swizzles, blending, depth, framebuffer aliasing and
list synchronization before adding widescreen behavior. Keep GPU resource lifetime
separate from guest allocations. Shader cache keys must include all relevant state
and device/compiler identity.

Yakumo's GE/Vulkan and texture replacement code is a reference inside its MHP
profile, not linked by Azazel6. Extracting reusable components will require removing
game-specific predicates and writing GE contract tests. No renderer is implemented
in Milestone 001. The HUD math library does not alter display lists.

PPSSPP-compatible packs require verified texture identity/hash semantics, palette
handling, wildcard rules and bounded decoder allocations. Compatibility is a goal,
not an outcome. Never silently apply a replacement for a different game version.
No texture pack or original asset is distributed.

Audio will require PSP audio/SAS/ATRAC behavior and timing integration. The import
inventory also includes PSMF/MPEG. Review any codec dependencies and notices before
linking them; do not import Yakumo's codecs merely to satisfy names in a table.

## Measurement contract

Record separate scopes for AOT guest execution, interpreter coverage, HLE/scheduler,
renderer CPU, GPU timestamps, shader compilation, texture decode/upload, I/O, mods
and allocation. Use monotonic host time for profiling and a separate guest clock
for simulation. Frame reports distinguish simulation progress, presentation rate,
percentiles and stalls. GPU timestamps need availability/disjoint handling.

Compare with unmodified PPSSPP on the same Android device, game hash, scenario,
render resolution, thermal state and controller input sequence. Save raw samples
locally and publish methodology plus aggregate results. No performance advantage
has been measured, and no FPS unlock or gameplay clock change is part of this work.
