# Inspected sources

**VERIFIED source inspection, 2026-10-17.** Revisions were obtained with
`git rev-parse HEAD` after read-only shallow clones; upstream projects were not
modified or pushed. The fetchable framework dependency is separately sparse and
pinned. Licenses/notices are summarized in [THIRD_PARTY.md](../../THIRD_PARTY.md).

| Repository | Pinned inspection revision | Inspected areas |
| --- | --- | --- |
| [Yakumo](https://github.com/TeamGDB/Yakumo/tree/39ac05f1a166703430d18826bc5877f4feb6b511) | `39ac05f1a166703430d18826bc5877f4feb6b511` | README, AGENTS, license, architecture/profile/build/compatibility/provenance/device guides; root CMake, ELF loader, analyzer, generator, memory/runtime APIs; MHP profile README and Android packaging |
| [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp/tree/f6e7d415c7f447b934cc3865a31eb725f353d659) | `f6e7d415c7f447b934cc3865a31eb725f353d659` | README, license, framework/profile split and Windows-oriented VCS baseline |
| [Ultrawide](https://github.com/zxyandreay/tekken6-ultrawide/tree/d50cd5f19053a2f0aeda7f2970619b984aebad8f) | `d50cd5f19053a2f0aeda7f2970619b984aebad8f` | README, plugin aspect source, renderer/AutoHUD architecture, auto-aspect history index, HP downstream, EXP4A and winner-glow findings |
| [Jin8](https://github.com/zxyandreay/tekken6-jin8/tree/dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0) | `dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0` | README, roadmap, executable fingerprint, memory map, cheat-format inventory, latest multi-move freeze incident; standalone/KiwiYu invalidation and memcpy source paths |

The upstream guides describe a playable Monster Hunter profile. That status does
not transfer to Tekken. Their device results and the two Tekken plugins' historical
experiments are DOCUMENTED evidence until independently reproduced.

The implementation reads local EBOOT bytes supplied for this assignment. No game
download was attempted. ISO inspection used installed 7-Zip 24.09 to list paths
only; no assets were extracted for distribution. Raw inventories, generated code,
framework logs and analyzer reports remain in ignored `local/`.
