# Startup coverage and reachability

**VERIFIED, 2026-10-18:** the exact ELF helper at `0x088040C4` assigns zero to
t9 and branches around its indirect jump at `0x08804110`. The scanner now has an
opt-in constant lattice for LUI, ADDIU and ORI. It checks branch conditions before
delay-slot effects, handles branch-likely annulment, merges conflicting facts to
unknown and invalidates facts across calls and unmodeled instructions. Unknown
control flow remains rejecting. Only this root enables constant branch pruning.

Tail jumps to validated import stubs terminate discovery rather than including
unpatched stub instructions in generated code. Stub addresses are checked integer
guest addresses. This matters for the observed helper at `0x08B778FC`, which tail
calls interrupt restoration.

The eleven roots contain 709 words. The bounded native trace executes these helpers
and reaches `0x08B88BF8`, whose constructor loop performs JALR at `0x08B88C24`.
Its backward table walk starts at `0x08BFA1E8`; 59 entries precede the sentinel at
`0x08BFA0FC`. This is a verified static table, not permission to execute arbitrary
indirect targets. The next task is a checked target map and per-call runtime guard.

27 Python tests, eight CTest suites and Android ARM64 compilation passed. New
synthetic cases cover joins, call clobbers, arithmetic wrapping, zero-extension,
delay slots, constant branch proofs and import tail boundaries. Raw instructions,
table contents and generated translations remain local.
