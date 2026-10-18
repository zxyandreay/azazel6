# Formatter callback boundaries

**VERIFIED, 2026-10-18:** exact-hash disassembly of the formatter caller at
`0x08B7C444` initializes a string-buffer descriptor with flags `0x0208`.
The shared output helper at `0x08B78EA4` also contains three JALR sites that
load a callback from descriptor offset `0x24`: `0x08B7901C`, `0x08B79084`,
and `0x08B79238`. Their targets have not been established.

The profile declares these sites as terminal traps. Generation validates each
canonical JALR instruction and register, rejects duplicate/conflicting or unused
declarations, and inserts an empty-target guard before the transfer or its delay
slot. CFG discovery does not follow a trapped call's continuation. Other paths
remain available. This does not authorize callbacks or assume they are dead.
An actual callback attempt records its target and stops without changing guest
registers. No generic runtime source or game instruction is patched.

The bounded native experiment takes the string-buffer path, reaches the verified
direct helper `0x08B7A100`, returns through the formatter and reaches
`ThreadManForUser::0x94AA61EE`, with return address `0x08B03554`. No callback
trap fires. Twenty-four roots now cover 3359 instruction words. This proves the
current formatting call returned; it does not validate all format specifiers or
file streams. The missing import remains a research boundary, not boot success.

35 Python cases and nine CTest suites pass, including terminal CFG behavior,
invalid descriptors, conflicting maps, invalid delay slots, guard placement and
empty-map rejection with guest-register preservation. Windows execution uses
the optional unoptimized probe configuration; Android ARM64 retains optimized
compilation. The run remains bounded to 4096 dispatches and 15 seconds with no
interpreter fallback. Raw disassembly, generated code and logs stay local.

**PLANNED:** implement the observed priority query from pinned kernel/SDK
contracts, then continue the second constructor. File callbacks need independent
target and lifetime evidence before their guards can permit execution.
