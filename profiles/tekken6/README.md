# Tekken 6 USA profile

Only `ULUS10466` with the SHA-256 in [profile.json](config/profile.json) is accepted.
Provide a decrypted ELF, not an ISO or encrypted PSP executable. Other regions,
patched executables and PRX inputs fail closed.

`tools/analyze.py` validates the ELF and reads the PSP module through the first
PT_LOAD header's `p_paddr` file offset. This convention is verified for this exact
input, not assumed for arbitrary ELF files. The upstream section-based scanner
does not find this sectionless module.

`host/aspect.cpp` is pure logic; it has no memory writes or hooks. `host/entry_probe.cpp`
loads and registers a tiny generated slice, then records the first import boundary.
Its experimental stack and zero entry arguments do not establish the game's full
startup ABI. There are no implemented PSP services behind its rejecting HLE traps.

Use [the build procedure](../../docs/development/building.md). Local inputs and
outputs belong in root `local/`; this profile contains only distributable source
and configuration. Full auto generation is exploratory and is not automatically
linked into the entry probe.
