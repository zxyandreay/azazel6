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

**PLANNED:** watch writes to the observed object field and trace its
constructor/source instructions. Inspect `0x0886B45C` and its direct
callees with later indirect sites trapped. Admit only a proven target
source and rerun the bounded exact-input slice. Milestones 003 and 004
remain open.
