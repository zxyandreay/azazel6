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
