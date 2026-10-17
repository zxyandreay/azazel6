# Guarded constructor calls

**VERIFIED, 2026-10-18:** the exact-hash constructor loop at `0x08B88BF8`
loads a target from the backward sentinel-terminated table ending at `0x08BFA1EC`,
then calls it through a0 at `0x08B88C24`. The table has 59 entries and sentinel
`0xFFFFFFFF` at `0x08BFA0FC`. Profile configuration records the descriptor, not a
copy of the game table. Generation revalidates the JALR encoding, alignment,
bounded sentinel search and every target's executable file-backed range.

The scanner permits this declared call and follows its continuation without
recursively treating all table targets as discovered functions. A profile-owned
generation pass inserts a guard immediately before the generated JALR label.
It requires that label to occur exactly once, failing closed if the pinned
upstream emitter changes. The runtime guard rejects targets outside the validated
list before RA, the delay slot or target dispatch changes guest state. Listed
but uncovered targets still stop in ordinary AOT dispatch. No interpreter is used.

The external framework remains unmodified. This explicit generated-code adaptation
is confined to the Tekken profile/tooling; generated C++ and target values stay
under ignored local output. This does not validate arbitrary function pointers,
overlays or runtime-modified constructor tables.

**Execution:** the first observed target `0x08B03CE4` is now compiled. Its CFG also
reaches shared initialization helpers and calls `sceKernelCreateMutex` with a
valid name, attributes zero, initial count zero and null options. The trace stops
at missing `ThreadManForUser::0xB7D098C6`, stub `0x08B88F88`, return `0x08B01DD4`.
Thirteen startup roots contain 779 words; later constructors have not executed.

31 synthetic Python tests, nine CTest suites and Android ARM64 compilation passed.
New tests reject missing/empty/oversized tables, malformed targets, call mismatch,
duplicate labels and unauthorized runtime targets without guest-state mutation.
