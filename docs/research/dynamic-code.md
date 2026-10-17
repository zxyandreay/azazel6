# Dynamic code and disc inventory

**Question:** is all executable code in EBOOT? **Status:** unresolved; additional
module resources are VERIFIED, game overlays/runtime code generation are unknown.

Read-only command, with the user's own local ISO:

```sh
7z l -slt local/game/tekken6.iso > local/iso-inventory.txt
```

The supplied disc inventory contains `PSP_GAME/USRDIR/modules/libpsmfplayer.prx`
(32,144 bytes) and `psmf.prx` (6,800 bytes), plus `DATA401.IDX`, `DINF_400.BIN`,
movies and the system executables/update resources. No commercial content is copied
into this repository. The EBOOT profile inventory also contains seven
`ModuleMgrForUser` imports. Neither observation proves which modules are loaded
during a particular scenario or that those are all dynamic code sources.

**PLANNED experiments:** trace module-load/import requests and their paths during
stock boot; inventory archive/index extents; monitor writes into executable guest
pages followed by dispatch; hash any newly executed regions and compare with
file-backed input. Separate media-library PRX loading from game overlays. Detect
code copied from data, decompression and self-modification without assuming a fixed
overlay address window.

When an executable region changes, unregister stale AOT mappings before installing
verified replacements. Yakumo provides missing-function and unsupported hooks plus
range unregistration, but its 355-overlay MHP layout is not a Tekken layout. Keep
fallback counts/PCs in diagnostics and never claim AOT completeness from static
EBOOT generation alone.
