# Guarded object dispatch

**VERIFIED, 2026-10-18:** constructor 15 reaches object initialization through
`0x08ACD424` and `0x08ACD8B8`. Shared helper `0x08ACC318` bounds its selector below
eight and dispatches through the file-backed table at `0x08BCF828`. The profile
validates all eight destinations and guards its JR at `0x08ACC34C`.

An initial rejecting experiment stopped at JALR `0x08ACC368`, target
`0x08ACC214`. The setup at `0x08ACD1BC` writes vtable `0x08BCFB08`; offset `0x10`
resolves that same target. Another branch writes vtable `0x08BCFAD0` and calls its
offset `0x10` at `0x08ACC3DC`. The profile now declares those exact pointer slots,
not arbitrary target literals. Generation verifies the call instruction/register,
file-backed pointer slot, aligned executable destination, unique site ownership,
coverage and guard placement. Unknown dynamic targets still stop before the
transfer or delay slot. Other vtable types require their own evidence.

The target function uses five virtual tail transfers through offset `0x18` of
these same two vtables. Pointer-tail descriptors validate canonical JR instructions
and guard destinations from those slots. CFG discovery records a terminal
function boundary, preserving the distinction from an intraprocedural jump table;
it does not inline the callee or invent a new return address. Uncompiled allowed
targets retain the normal missing-AOT stop.

The native experiment passes the call guard, executes `0x08ACC214` and reaches
uncovered tail target `0x08AEBB70` with return address `0x08ACC370`. The first
fourteen constructors have returned; constructor 15 is still active. There are
104 roots and 7545 words, with three mutexes and eight inactive timers. No graphics,
timer activation, title screen or general C++ exception support is established.

39 Python cases, nine CTest suites, Windows execution and Android ARM64 compilation
pass. Synthetic tests cover malformed pointers, unmapped/unaligned slots, invalid
targets, duplicate sites, call/tail encoding mismatch, empty maps and preservation
of tail-call boundaries. Native guards retain their register-preservation and
unknown-target rejection contracts. The external framework remains unchanged.

**PLANNED:** cover the observed tail target and continue object initialization.

## Constructor variants and direct helpers

The next rejecting run reported target `0x08ACC738` from object vtable
`0x08BCF990`. This is another verified switch branch, not an arbitrary new type.
Inspection of all branch assignments identifies eight direct vtable bases
(`0x08BCF8D0`, `0x08BCF910`, `0x08BCF950`, `0x08BCF990`, `0x08BCF9D0`,
`0x08BCFA50`, `0x08BCFA90`, `0x08BCFAD0`) plus the two helper-installed bases
`0x08BCFB08`/`0x08BCFB30`. The call and tail maps now derive their permitted
destinations from the relevant slots in those ten tables. This covers the
selector/global-flag variants visible in the helper; it does not authorize
unrelated object types or mutable callback values.

Failure diagnostics now report a0/v0/v1/t9 and, only for aligned canonical RAM,
the first word at a0. These are local research logs; reading a bounded word is
not a claim that the address denotes a live object or a vtable.

A bounded batch added 16 direct-call targets from already-executed functions'
validated CFGs. Each target passed the same per-root limit and global ownership
check; the batch was capped at 2048 additional words and did not recursively
follow new callees. Both branches of those callers may contribute targets, so
coverage remains distinct from execution. There are now 123 roots/8013 words.
Native execution passes the observed variant and reaches uncovered `0x08ACD3C4`
at return `0x08AEBB8C`, still inside constructor 15. 39 Python cases, nine CTest
suites and Android ARM64 compilation pass. Execution bounds are unchanged.
