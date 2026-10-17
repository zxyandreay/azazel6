# Vision and current scope

Azazel6's intended product is a standalone Android ARM64 application running
Tekken 6 PSP code translated ahead of time, with a game-specific PSP runtime.
The user supplies their own compatible game. Windows/Linux support makes analysis,
debugging and deterministic regression tests accessible during development.

Long-term work includes Vulkan GE rendering, audio, saves, touch/gamepad input,
stock simulation timing, semantic ultrawide presentation, PPSSPP-format replacement
textures where compatible, and lifecycle-aware character moveset packages.

Milestone 001 establishes identity checks, executable analysis, framework builds,
first bounded native instructions and Android compilation. It does not implement
a bootable game, user-facing APK, gameplay renderer, cheat interpreter or moveset
loader. Synthetic tests prove tooling contracts, not game compatibility.

Performance improvement is a research question. Preserve stock simulation timing;
do not add FPS unlocks to make benchmark numbers look better. Public playable
releases require reproducible stock-game correctness on real Android devices first.
