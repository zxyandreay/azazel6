# Inspected sources

**VERIFIED source inspection, 2026-10-17.** Revisions were obtained with
`git rev-parse HEAD` after read-only shallow clones; upstream projects were not
modified or pushed. The fetchable framework dependency is separately sparse and
pinned. Licenses/notices are summarized in [THIRD_PARTY.md](../../THIRD_PARTY.md).

| Repository | Pinned inspection revision | Inspected areas |
| --- | --- | --- |
| [Yakumo](https://github.com/TeamGDB/Yakumo/tree/39ac05f1a166703430d18826bc5877f4feb6b511) | `39ac05f1a166703430d18826bc5877f4feb6b511` | README, AGENTS, license, architecture/profile/build/compatibility/provenance/device guides; root CMake, ELF loader, analyzer, generator, memory/runtime APIs; MHP profile README and Android packaging |
| [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp/tree/f6e7d415c7f447b934cc3865a31eb725f353d659) | `f6e7d415c7f447b934cc3865a31eb725f353d659` | README, license, framework/profile split and Windows-oriented VCS baseline |
| [uOFW](https://github.com/uofw/uofw/tree/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9) | `28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9` | COPYING, SysMem export map and SDK/compiler setters in misc.c, ThreadMan export map; [startup findings](startup-imports.md) |
| [PSPSDK](https://github.com/pspdev/pspsdk/tree/6ef2b21dc440ef8d4125f000301faaa2a470975e) | `6ef2b21dc440ef8d4125f000301faaa2a470975e` | Thread API header, startup crt0 and license notices |
| [PSP autotests](https://github.com/hrydgard/pspautotests/tree/071a2a6f14477b8b186bd53201e2f2c0adc80679) | `071a2a6f14477b8b186bd53201e2f2c0adc80679` | Thread create/start expected results and fixture argument definitions; [dormant-thread findings](dormant-threads.md); no binaries executed |
| [Ultrawide](https://github.com/zxyandreay/tekken6-ultrawide/tree/d50cd5f19053a2f0aeda7f2970619b984aebad8f) | `d50cd5f19053a2f0aeda7f2970619b984aebad8f` | README, plugin aspect source, renderer/AutoHUD architecture, auto-aspect history index, HP downstream, EXP4A and winner-glow findings |
| [Jin8](https://github.com/zxyandreay/tekken6-jin8/tree/dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0) | `dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0` | README, roadmap, executable fingerprint, memory map, cheat-format inventory, latest multi-move freeze incident; standalone/KiwiYu invalidation and memcpy source paths |

The upstream guides describe a playable Monster Hunter profile. That status does
not transfer to Tekken. Their device results and the two Tekken plugins' historical
experiments are DOCUMENTED evidence until independently reproduced.

The implementation reads local EBOOT bytes supplied for this assignment. No game
download was attempted. ISO inspection used installed 7-Zip 24.09 to list paths
only; no assets were extracted for distribution. Raw inventories, generated code,
framework logs and analyzer reports remain in ignored `local/`.

**Additional inspection, 2026-10-19:** PPSSPP's
[`sceImpose.cpp` at `cd94e414cde964bf25ec67267f64fcd3d01283bb`](https://github.com/hrydgard/ppsspp/blob/cd94e414cde964bf25ec67267f64fcd3d01283bb/Core/HLE/sceImpose.cpp)
was read through GitHub's API at that fixed revision. Only language-mode API
behavior was used as reference evidence. No PPSSPP implementation was copied into
Azazel6 or linked; this is emulator behavior, not a reproduced hardware result.

**Additional inspection, 2026-10-20:** the same pinned PSPSDK GE/display and
interrupt headers/export tables, and uOFW GE callback registration/unset behavior
were inspected for the graphics handoff. No source implementation was copied.

**Additional inspection, 2026-10-20:** PPSSPP `Core/HLE/sceKernelInterrupt.cpp`
and `Core/HLE/sceDisplay.cpp` at the same fixed `cd94e414cde964bf25ec67267f64fcd3d01283bb`
revision were read through the GitHub connector for interrupt ownership, LCD
frequency, display wait and framebuffer latch contracts. No implementation was
copied or linked. See [display foundation](display-foundation.md).

**Additional inspection, 2026-10-21:** PPSSPP `Core/HLE/sceKernelEventFlag.cpp`,
`Core/HLE/sceGe.cpp` and `Core/HLE/sceDisplay.cpp` at the same fixed revision were
read through the GitHub connector for queued event flags, GE submission/sync and
display-mode wait behavior. Pinned PSPSDK `guInternal.h` and `sceGuInit.c` were read
for command/register and float24 contracts. No implementation was copied.
