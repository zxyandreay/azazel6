# Worker entry and immediate address guards

**VERIFIED, 2026-10-19:** the startup thread creates two workers with entry
`0x0889B254`, priority 48 and 12288-byte stacks. Each receives a copied eight-byte
argument record. The wrapper loads its callback and object pointer from that
record and calls through register 2 at `0x0889B264`. Its return path calls
ExitThread. Both workers reach Ready while the priority-40 parent continues;
that observation does not establish worker callback execution.

The creator at `0x089B55A0` constructs callback address `0x089B5A70` using the
adjacent LUI/ADDIU pair at `0x089B5788`, stores it into each argument record and
passes that record to StartThread. This provenance is established from exact-hash
code inspection. The argument records themselves are runtime data, not file-backed
function-pointer slots.

`immediate_calls` therefore names instruction sources rather than literal allowed
targets. Generation validates canonical JALR sites and adjacent LUI/ADDIU or
LUI/ORI pairs with matching nonzero destination registers. Signed ADDIU and
zero-extended ORI resolve 32-bit addresses; source instructions and destinations
must be aligned and file-backed executable bytes. Duplicate, malformed, conflicting
and uncovered declarations reject. The existing generated guard permits only the
derived destinations before executing the transfer or delay slot. It does not
claim general data-flow analysis: each source-to-call relationship needs separate
reviewed evidence, as above.

Unresolved member-pointer tail calls can now use `trapped_tails`. These require
canonical JR with a nonzero register other than RA, and authorize no destinations.
They terminate CFG traversal and stop execution before the transfer or delay slot.
They cannot overlap any authorized map. This allows other branches of a function
to execute without assuming unresolved callbacks are unreachable.

Synthetic tests cover both address-construction forms, signed low halves, source
and target bounds, permissions, duplicates, malformed opcodes/registers, terminal
tail behavior, conflict rejection and guard placement. Native guard tests verify
that an empty allowed set stops without changing guest state. Raw code and traces
remain local.

**VERIFIED checkpoint:** 240 roots cover 21704 words. After 15833 dispatches,
the main thread stops at guarded call `0x0885FCF4`, observing vtable `0x08B8FE80`
and target `0x0880830C`. Its constructor installs this table at `0x08861694`.
The adjacent linked-object helper also contains unresolved member-pointer tails
`0x0885FC94` and `0x0885FCB0`; both remain terminal stops. All 46 Python tests,
12 CTest suites, Windows execution and Android ARM64 compilation pass. The next
experiment permits the verified vtable slots while retaining other guards.
