# Guarded startup vtables and callback boundary

## Pinned input and method

**VERIFIED, 2026-11-18:** this checkpoint uses the decrypted USA
ULUS10466 executable with SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`
and the separately validated read-only ISO with SHA-256
`45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8`.
`tools/inspect_root.py` inspected each new file-backed CFG. The guarded
`tools/entry_probe.py` slice was compiled and run with `tools/run_probe.py`
under its 15-second guest bound. Generated code, logs, frames and game inputs
remain ignored in `local/`.

## Observed continuation

**VERIFIED source and execution:** the missing direct helper `0x088788B8`
and its callee execute. The object call at `0x0885FCF4` then reads the
file-backed vtable slot `0x08B90D84`, whose value is `0x0886A774`.
That helper reaches `0x08869EA8`, a table tail dispatch. The observed
index was zero and the checked file-backed slot `0x08B91480` contains
`0x0886A740`. Other table entries remain unadmitted. The paths from
`0x08869E00` and `0x0886A740` share return fragments at
`0x08869EFC` and `0x08869F00`; separate AOT roots prevent overlapping
generated ranges.

The subsequent call at `0x08869F78` reads `0x08B913E8`, which contains
`0x0886ABE4`. The call at `0x0886A878` reads `0x08B913A0`, which
contains `0x0886ABF4`. The existing dispatch site `0x0885FBA4`
next uses `0x08B90D88` to reach `0x08868F0C` and its inspected direct
callees. The existing tail at `0x08808314` then uses `0x08B91590`
to reach `0x08869644`. Its call at `0x0886966C` reads
`0x08B915C8`, which contains `0x0886AE60`. Each admitted slot is
file-backed and each live target matched the checked slot. Later indirect
sites in these CFGs retain terminal guards.

**VERIFIED bounded stop:** the 749-root / 44,814-word host-native slice
stops at `0x0886AEC4`, observing target `0x0886B45C` from a writable
object field. The code at `0x0886AE80` reads the field at object offset
`0x68`; at `0x0886AEC4` it calls the loaded value. The field writer and
lifetime have not yet been established, so this target is not authorized.
The run completed fifteen GE lists, consumed one controller read, and
captured sixteen uniformly black 480×272 frames. This is no title-screen
or physical ARM64 evidence.

## Next experiment

**PLANNED at the 749-root checkpoint; completed below:** watch writes to
the observed object field and trace its constructor/source instructions.
Inspect `0x0886B45C` and its direct
callees with later indirect sites trapped. Admit only a proven target
source and rerun the bounded exact-input slice. Milestones 003 and 004
remain open.

## Watched constructor callback continuation

**VERIFIED, 2026-11-18:** a write watch on guest address `0x09103EA8`
recorded one main-thread 32-bit store, changing zero to `0x0886B45C`.
The watch logs dispatch PC `0x0886B3E8`; exact source instructions form
the target at `0x0886B3E8/0x0886B3F0` and store it at `0x0886B3F4`
to object offset `0x68`. The call path reads that field at
`0x0886AE80` and reaches it through `0x0886AEC4`. The profile's
immediate-source guard admits only the formed target; the live runtime
guard rejects any other value. This witness applies to the observed
object and run, not arbitrary future object lifetimes.

**VERIFIED execution:** `0x0886B45C` and its inspected direct callees
execute. The two `0x08869694` calls then use checked file-backed slots
`0x08B915D0` and `0x08B90E80`. Later methods use checked slots
`0x08B915BC` at `0x08868A0C`, `0x08B915AC` at `0x08868DD0`, and
`0x08B915C4` at `0x08868950`. Other indirect branches remain trapped.
The 762-root / 45,442-word bounded run next stops at the existing
guard `0x0885FCF4`, observing a new object's target `0x0888A1CC`
and vtable `0x08B93C68`. Fifteen GE lists, one controller read and
sixteen uniformly black frames remain. Title-screen boot is not verified.

**NEXT:** inspect `0x0888A1CC` and the observed vtable slot at
`0x08B93C7C`, then rerun with later transfers guarded.

## Object chain after volatile-memory acquisition

**VERIFIED, 2026-11-18:** after the checked volatile-memory try-lock,
the existing tail at `0x08808314` uses file-backed slot `0x08B93CC8`
to reach `0x088891A0`. Its direct helper `0x0885EC24` is covered with
two possible indirect tails trapped; that helper was not observed in
the latest run. The next object call at `0x0885FCF4` uses checked slot
`0x08B93FDC` to reach `0x0888A788`. Its observed call at
`0x0888A7E4` uses `0x08B93E50` to reach `0x08893F44`.

The same bounded path executes direct helper `0x0888A6BC`, then its
checked call at `0x0888A844` uses `0x08B93EA0` to reach
`0x08893F84`. Later indirect sites remain terminal guards. The
781-root / 46,379-word slice stops at `0x0885FCF4` with a different
object vtable `0x08B93D58` and live target `0x0888893C`. The observed
slot would be `0x08B93D6C`; it has not been admitted. One volatile lock
remains held. Fifteen GE lists and sixteen uniformly black frames do
not establish title-screen boot.

**NEXT:** inspect `0x0888893C` and the exact slot, then continue the
bounded path without widening other object dispatches.
