# Watched startup callback table

## Question and pinned input

**VERIFIED, 2026-11-18:** the bounded startup reached an indirect call at
`0x0893A320`. Its target came from a writable BSS table, so a file-backed
pointer declaration could not describe it. The input was the exact USA
decrypted executable and separately admitted read-only ISO pinned in
[`profile.json`](../../profiles/tekken6/config/profile.json). The executable
SHA-256 is `e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
Other variants remain refused.

## Method and observations

`tools/inspect_root.py` first held `0x0893A320` as a terminal trap.
`PSPRECOMP_WATCH_WRITE` then watched each guest slot during bounded
`tools/run_probe.py --iso` runs. These local logs and generated translations
remain ignored under `local/`.

| Table index | BSS slot | Observed value | Writer dispatch |
| --- | --- | --- | --- |
| 0 | `0x08C16AE8` | `0x08934C00` | `0x08936020` |
| 68 | `0x08C18248` | `0x08934BF8` | `0x08939204` |

**VERIFIED source:** the table base is `0x08C16AD8`, entries are `0x58`
bytes apart, and the call reads the function pointer at entry offset `0x10`.
Both slots lie in the writable BSS part of the validated PT_LOAD. Exact
instructions at `0x089351F8`/`0x08936068` construct the first value, which
is stored at `0x08936078`. Instructions at `0x08939228`/`0x0893923C`
construct the second, stored at `0x08939260`. The watch recorded each
nonzero store after an earlier clear. The call's load is at `0x0893A31C`;
the two observed targets are executable file-backed two-word roots.

The profile's `watched_slot_calls` declaration checks the exact call,
source, store and load instruction forms, derives both targets and slot
addresses, checks writable BSS ownership and table stride, then guards
the live target before the call or its delay slot. Every other target is
still rejected. Synthetic tests cover two slots sharing one call and
wrong instruction, register, slot, target, stride, duplicate and
ownership cases. The full Python suite and CMake tests are run for the
checkpoint separately.

**VERIFIED execution:** after admitting only those two targets, the
729-root / 44,242-word native probe executed them and next stopped at
missing direct root `0x088788B8`. It retained fifteen GE lists and
sixteen uniformly black 480×272 frames. This is host-native exact-input
execution, not title boot.

## Limits and next experiment

The source words alone do not prove that the distant upper register is
preserved across all control paths; the bounded write watches establish
the two stores in this run. Later table indices, writes and target values
remain unverified. No timing, scene presentation, physical ARM64 input or
lifecycle behavior follows from this trace. **NEXT:** inspect
`0x088788B8` and its direct callees, then rerun with later indirect
sites trapped until their live provenance is observed.
