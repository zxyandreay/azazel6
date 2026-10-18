# Constructor coverage and floating-point branches

The validated constructor table contains 59 aligned executable destinations.
These are code-discovery roots, not proof that all constructors have executed.
The profile can include a table-derived root when bounded discovery succeeds and
its ranges do not overlap other owners. Unsupported indirect transfers and
instruction-budget exhaustion remain exclusions requiring separate evidence.
Direct callees are still added only after investigation; there is no recursive
whole-image scan.

**VERIFIED, 2026-10-18:** native startup reaches the floating-point helper at
`0x08B049A0`, whose conditional edge is BC1T. Pinned Yakumo's decoder, generator
and runtime support the BC1F/T/FL/TL family. The profile CFG now accepts the four
canonical condition-zero forms, conservatively following both outcomes. It never
infers FPU condition flags from GPR facts. Branch-likely fallthrough annuls the
delay slot; the taken path executes it. Other coprocessor branches and reserved
condition encodings remain rejected.

Independent synthetic fixtures exercise both outcomes of all four forms, likely
annulment with downstream GPR propagation, unsupported encodings and a branch
into a delay slot. No generic runtime change or floating-point replacement was
introduced. Raw code and generated output remain excluded from the repository.

The batch passes bounded discovery and overlap checks for 42 of the 59 table
entries. Seventeen remain excluded: two contain unresolved indirect transfers,
one exceeds the 4096-word per-root budget, and fourteen overlap shared code that
needs explicit ownership. Generating those entries does not mark them executed.

The native probe returns through the first twelve constructors and enters the
thirteenth. It reaches uncovered helper `0x08B61174`, return `0x08B612C0`, while
retaining two mutexes and eight inactive timers. Seventy-five roots cover 6273
instruction words, including investigated direct helpers. No clock or callback
stop is bypassed. 36 Python cases, nine CTest suites, Windows native execution and
Android ARM64 compilation pass; the existing execution limits remain unchanged.

**PLANNED:** continue the observed helper, then investigate excluded constructor
roots when reached. Full constructor completion and title-screen boot remain
unverified.

## Shared-range ownership

**VERIFIED:** the fourteen overlap exclusions share four existing ranges at
`0x089B6DCC`, `0x0887C40C`, `0x0887C640` and `0x0885F3E0`. Giving these ranges
explicit roots allows all fourteen constructors to pass ownership checks.
Coverage is now 56/59 constructor entries, 95 roots and 7144 words. The remaining
three exclusions are the two unresolved indirect transfers and the per-root
instruction-budget limit above.

Discovery also recognizes known external entries at conditional targets,
ordinary fallthrough and call continuations. For example, the error path in
helper `0x08B6134C` calls an uncovered helper and would return at the separately
owned `0x08B613C0`. It must not duplicate that next function's instructions.
These edges remain runtime transfers, not assumptions that a call never returns.
Synthetic tests cover all three edge forms and ensure declaring an external
entry cannot hide an invalid branch into a delay slot or alias the root itself.

The thirteenth constructor now creates and locks a third mutex through guest
static-initialization helpers. It reaches uncovered `0x08B61438`, return
`0x08AD0D84`. No contention or callback behavior is inferred from this run.
37 Python cases, nine native suites and Android ARM64 compilation pass.

## Coverage of all constructor entries

**VERIFIED, 2026-10-19:** all 59 constructor-table entry points now pass bounded
CFG discovery and global range-ownership checks. This is coverage, not successful
execution of all 59 constructors or all their callees.

The wrappers at `0x08A95A3C` and `0x0891534C` explicitly load arguments 1 and
`0xFFFF` before tail-jumping to shared initializer/destructor code. The existing
conservative constant analysis excludes the destructor branches and their
unresolved virtual calls without assuming any incoming register values. These
roots contain 121 and 35 instruction words respectively.

The same analysis on `0x08939FE8` finds a 5109-word initializer. Its first direct
call returns at `0x08936020`; assigning that continuation a separate root yields
1027 and 4082 words. Both remain within the unchanged 4096-word per-root bound.
The continuation begins with unknown register facts; no facts are smuggled across
its boundary. Calls remain external, and range/delay-slot checks apply normally.

The profile has 152 roots/15120 words including observed helper coverage. New
object-call guards use vtable `0x08BB1620`, installed by `0x0897E510`, and
`0x08BB16E8`, installed by `0x0897DBF4`. Their offset `0x6C`/`0x48` calls and the
first object's offset `0x4C` tail derive destinations from exact file-backed
slots; unknown targets still stop. No game binaries or translations are committed.

**VERIFIED execution:** the first 26 constructors return. Constructor 27 reaches
uncovered helper `0x08A9504C`, return `0x08A954F8`, after passing the new object
calls. Three mutexes/eight inactive timers remain. All 39 Python cases, nine
CTest suites, Windows native execution and Android ARM64 compilation pass.
