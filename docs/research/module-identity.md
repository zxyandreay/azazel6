# Resident module identity

**VERIFIED, 2026-10-18:** after shared heap allocation, the native startup executes
observed helpers at `0x08B7B1B4` and `0x08B7A3BC`, then asks
`sceKernelGetModuleIdByAddress(0x08804124)`. The single resident image returns
profile-owned UID `0x20000000`. Threads and partition blocks use separate ranges;
partition UID exhaustion cannot consume the module range.

## Source and contract

Pinned uOFW `28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9`,
`src/kd/modulemgr/queryModule.c:207` and `src/kd/loadcore/module.c:421`, show
address lookup over resident text/data/BSS, returning UNKNOWN_MODULE when absent.
The user-only profile rejects high-bit kernel addresses with ILLEGAL_ADDR and
compares other addresses without dereferencing them. The resident interval is
this validated image's single PT_LOAD memory range; general module loading,
interrupt-context calls and kernel-mode callers remain unsupported.

Synthetic tests cover both interval edges, unaligned interior addresses, unknown
addresses, aliases, overflow, zero-size images and import-return ABI. The next
observed helper at `0x08B7AFD0` is included in AOT. The six roots total 530 words.
The bounded real trace stops at missing interrupt suspension
`Kernel_Library::0x092968F4`, stub `0x08B88D38`, continuation `0x08B7AFF8`.
Eight CTest suites, 21 Python tests and Android ARM64 compilation passed.
