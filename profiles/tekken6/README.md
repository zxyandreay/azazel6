# Tekken 6 USA profile

Only `ULUS10466` with the SHA-256 in [profile.json](config/profile.json) is accepted.
Provide a decrypted ELF, not an ISO or encrypted PSP executable. Other regions,
patched executables and PRX inputs fail closed.

`tools/analyze.py` validates the ELF and reads the PSP module through the first
PT_LOAD header's `p_paddr` file offset. This convention is verified for this exact
input, not assumed for arbitrary ELF files. The upstream section-based scanner
does not find this sectionless module.

`host/aspect.cpp` is pure logic; it has no memory writes or hooks. `host/entry_probe.cpp`
loads a bounded generated startup CFG and records unimplemented import boundaries.
Its experimental stack and zero entry arguments do not establish the game's full
startup ABI. `host/bootstrap.cpp` implements two narrowly scoped SDK/compiler
services. `host/threads.cpp` owns a bounded dormant-thread registry and guest stack
arena with CreateThread/DeleteThread handlers; StartThread is not implemented.
See [service evidence](../../docs/research/startup-imports.md) and
[thread limitations](../../docs/research/dormant-threads.md).

Use [the build procedure](../../docs/development/building.md). Local inputs and
outputs belong in root `local/`; this profile contains only distributable source
and configuration. Full auto generation is exploratory and is not automatically
linked into the entry probe.
