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

## Later object dispatch and writable callback

**VERIFIED source and execution, 2026-11-18:** the existing object sites
`0x0885FCF4` and `0x0885FBA4` used further checked file-backed slots
`0x08B93D6C`, `0x08B93DA4`, `0x08B93D70`, and `0x08B93DA8`.
The call sites `0x0888A88C` and `0x0888A8CC` used checked slots
`0x08B93EA0` and `0x08B93E50`. The tail at `0x08808314` used
`0x08B91700` to enter `0x0886C78C`. Each admitted target was observed
under the bounded exact-input run and inspected before adding its root.

**VERIFIED watched write:** `0x0886C814` loaded its target from a
writable object field at `0x090A9460`. One main-thread store changed
that field from zero to `0x0886CDD0`. The exact instructions at
`0x0886CC40/0x0886CC48` form this address and `0x0886CC4C` stores
it at object offset `0x60`. The immediate-source guard admits this
observed callback only; the field's behavior across other lifetimes
remains unverified. Its direct callees were inspected. The later calls
at `0x0888AAF4`, `0x0886CEC0`, `0x0888D1D4`, and `0x0888CB84`
each reached `0x0885E644` through the checked file-backed slot
`0x08B8F860`. Other indirect transfers retain terminal guards.

**VERIFIED bounded stop:** the 801-root / 47,409-word slice passes those
calls and stops at missing direct function `0x0888AEFC` (return address
`0x0888CC9C`). One volatile-memory lock is still held. The run records
fifteen GE lists, one controller read, sixteen display frames, and a
captured 480×272 scanout with zero nonblack pixels. It does not verify
a title screen. Generated code, watched logs, and captures remain in
ignored `local/`.

**PLANNED:** inspect `0x0888AEFC` and its direct callees. Keep the
unreached calls at `0x0886CF8C` and `0x0888CCE8` trapped until their
targets and sources are observed. Continue bounded exact-input runs.

## Repeated checked helper after the callback

**VERIFIED, 2026-11-18:** `0x0888AEFC` has a 54-word inspected CFG
and calls the already covered `0x08878CD4`. The bounded run then
observed `0x0888CCE8` and `0x0888CD48` reaching `0x0885E644`
through the checked file-backed slot `0x08B8F860`. After an inspected
root at `0x0888C7E4`, its call at `0x0888C860` used that same slot
and target. The guard at each call admits only this source and value.

**VERIFIED bounded stop:** the 803-root / 47,657-word slice next stops
at missing direct function `0x0888C56C` (return address `0x0888C944`).
One volatile-memory lock remains held; fifteen GE lists and sixteen
black display frames remain, and the captured 480×272 scanout has zero
nonblack pixels. The title-screen gate remains open.

**PLANNED:** inspect `0x0888C56C` and its indirect call at
`0x0888C5FC` before admitting either. `0x0886CF8C` remains trapped
and unobserved.

## Next object and initializer chain

**VERIFIED, 2026-11-18:** `0x0888C56C` executes and its call at
`0x0888C5FC` reads the checked file-backed slot `0x08B8F860`,
reaching `0x0885E644`. The direct `0x0896A214` initializer executes;
the previously trapped calls at `0x0886CF8C` and `0x0886D050`
also use that same checked slot and target. The next object call at
`0x0885FCF4` observes vtable `0x08B96000` and file-backed slot
`0x08B96014`, containing `0x088AD1D4`.

**VERIFIED source and execution:** the new method calls inspected direct
helpers `0x088B5AEC` and `0x088AD0CC`. Its call at `0x088AD2A4`
reads file-backed slot `0x08B960D8` to reach `0x088ADCD0`;
`0x088AD2FC` reads `0x08B96088` to reach `0x088ADCE0`.
Both targets have four-word inspected CFGs. The later call at
`0x088AD354` remains terminally trapped and has not executed.

**VERIFIED bounded stop:** the 810-root / 48,153-word exact-input slice
stops first at missing direct function `0x088AFDA0` (return address
`0x088AD344`). One volatile-memory lock remains held, with fifteen GE
lists, one controller read, sixteen black display frames, and zero
nonblack pixels in the captured 480×272 scanout. Title-screen boot is
not verified. The generated slice, media and capture remain ignored in
`local/`.

**PLANNED:** inspect and cover `0x088AFDA0`, then observe the target
and source at the guarded `0x088AD354` call.

## Continuation through another initializer and helper

**VERIFIED, 2026-11-19:** inspected direct helper `0x088AFDA0`
executes. The call at `0x088AD354` observes target `0x088ADCC0`
through file-backed slot `0x08B96128`; the target has a four-word
CFG. The path then executes `0x0882D6C8`, whose exact two-word CFG
returns immediately, and `0x088ABF60`. Within the latter helper,
`0x088B5C40` and `0x088B5E90` each observe `0x0885E644` through
the checked file-backed slot `0x08B8F860`. Later indirect calls at
`0x088B5CCC` and `0x088B5DCC`, and tails at `0x088AD3C8` and
`0x088AD438`, retain terminal guards.

**VERIFIED bounded stop:** the 814-root / 48,573-word slice stops at
missing direct function `0x0888E9EC` (return address `0x088B5EE4`).
One volatile-memory lock remains held; fifteen GE lists and sixteen
black display frames remain. The captured 480×272 scanout has zero
nonblack pixels. No title screen or physical ARM64 execution is
verified.

**PLANNED:** inspect `0x0888E9EC` and its indirect site at
`0x0888EA78`, then continue under the bounded exact-input guard.

## Return to the new object's tail

**VERIFIED, 2026-11-19:** `0x0888E9EC` and its inspected direct callee
`0x0888E2D4` execute. Their calls at `0x0888EA78`, `0x0888E364`,
`0x0888E4B8`, and `0x0888E518` each observe `0x0885E644` through
the checked file-backed slot `0x08B8F860`. Inspected helper
`0x0888B0C4` also executes. The object method then reaches its tail
at `0x088AD438`: `0x088AD418` reads vtable slot `0x08B96018`,
whose checked file-backed value is the observed target `0x088AD808`.
That target's later computed transfer at `0x088AD970` remains trapped.

**VERIFIED bounded stop:** the 818-root / 49,060-word exact-input slice
stops at missing direct function `0x0882421C` (return address
`0x088AD830`). One volatile-memory lock remains held. The run still
records fifteen GE lists, one controller read, sixteen black display
frames, and zero nonblack pixels in the captured 480×272 scanout.
This does not establish title-screen boot.

**PLANNED:** cover the inspected `0x0882421C` helper and continue the
bounded run. Investigate the computed transfer only if observed.

## Post-tail helpers and state transition

**VERIFIED, 2026-11-19:** the inspected `0x0882421C`, `0x088BF718`,
`0x088BF8AC`, and `0x088BEF10` roots are covered on the bounded path.
The latter roots include a thunk into a 201-word CFG; other direct
callees in that CFG remain unadmitted until their paths execute.
The existing tail at `0x0885F16C` then observes `0x088ADA70` from
checked file-backed vtable slot `0x08B9605C`. Its indirect call at
`0x088ADB30` remains terminally trapped and has not executed.

**VERIFIED bounded stop:** the 823-root / 49,379-word slice stops at
missing direct function `0x088B8764` (return address `0x088ADAE0`).
One volatile-memory lock remains held; fifteen GE lists, one
controller read, sixteen black display frames, and a captured
480×272 scanout with zero nonblack pixels remain. Title-screen boot
is not verified.

**PLANNED:** inspect the observed direct helper's callees and
continue the exact-input run under the same bound.

## Nested object callback after the post-tail state

**VERIFIED, 2026-11-19:** direct helper `0x088B8764` executes through
the observed branch to five-word helper `0x08A66D1C`. The call at
`0x088ADB30` then reads checked file-backed slot `0x08B95AA0`,
containing target `0x088A53DC`. The latter has a four-word inspected
CFG. The path next executes seven-word direct helper `0x088B1654`.
Potential tails at `0x088ADC40` and `0x088ADC88` remain terminally
trapped and unobserved.

**VERIFIED bounded stop:** the 827-root / 49,652-word exact-input
slice stops at missing direct function `0x088B8A10` (return address
`0x088ADB98`). One volatile-memory lock remains held. Fifteen GE
lists, one controller read, sixteen black display frames, and zero
nonblack pixels in the captured 480×272 scanout remain. Title-screen
boot is not verified.

**PLANNED:** cover the inspected `0x088B8A10` CFG and continue the
bounded path.

## Nested tail and next object

**VERIFIED, 2026-11-19:** inspected direct helper `0x088B8A10`
executes. Its caller's tail at `0x088ADC40` reads checked file-backed
slot `0x08B96060` and reaches `0x088AC7C4`. That target's 215-word
CFG is covered with its unobserved direct callees still unadmitted.
The next object call at `0x0885FCF4` observes vtable `0x08B969F8`;
checked slot `0x08B96A0C` contains target `0x088B94B8`. The latter
executes inspected direct helper `0x08A6677C`; its indirect call at
`0x088B9570` remains terminally trapped and unobserved.

**VERIFIED bounded stop:** the 831-root / 50,117-word exact-input
slice stops at missing direct function `0x08A6691C` (return address
`0x088B9504`). One volatile-memory lock remains held. Fifteen GE
lists, one controller read, sixteen black display frames and zero
nonblack pixels in the captured 480×272 scanout remain. Title-screen
boot is not verified.

**PLANNED:** inspect the observed direct helper's callees and
continue the bounded exact-input run.

## Nested initializer under the next object

**VERIFIED, 2026-11-19:** the inspected `0x08A6691C` and its direct
callee `0x08A66C74` are covered. The object method also executes
direct helper `0x088A4D8C`. Its indirect call at `0x088B9570`
observes target `0x088B9B9C` from checked file-backed slot
`0x08B96B28`. That target has a four-word inspected CFG. The later
call at `0x088B9644` remains terminally trapped and unobserved.

**VERIFIED bounded stop:** the 835-root / 50,420-word exact-input
slice stops first at missing direct function `0x088C04C8` (return
address `0x088B95B4`). One volatile-memory lock remains held;
fifteen GE lists, one controller read, sixteen black display frames
and zero nonblack pixels in the captured 480×272 scanout remain.
Title-screen boot is not verified.

**PLANNED:** cover the inspected `0x088C04C8` CFG, then observe the
later indirect call's target and source if reached.

## Further nested initializer continuation

**VERIFIED, 2026-11-19:** direct helpers `0x088C04C8` and
`0x088C1A24` execute. The later call at `0x088B9644` reads checked
file-backed slot `0x08B96A90`, containing the observed four-word
target `0x088B9BBC`. The next call at `0x088B969C` remains terminally
trapped and unobserved.

**VERIFIED bounded stop:** the 838-root / 50,552-word exact-input
slice stops first at missing direct function `0x088A3738` (return
address `0x088B968C`). One volatile-memory lock remains held.
Fifteen GE lists, one controller read, sixteen black display frames
and zero nonblack pixels in the captured 480×272 scanout remain.
Title-screen boot is not verified.

**PLANNED:** cover the inspected `0x088A3738` CFG and its reached
direct callees, then continue the bounded run.

## Second nested object and tail continuation

**VERIFIED, 2026-11-19:** direct helper `0x088A3738` and its
`0x08AB2F9C` callee execute. The indirect call at `0x088B969C`
observes `0x088B9BCC` from checked file-backed slot `0x08B96A48`.
The next indirect call at `0x088B9740` observes `0x088B9BAC`
from checked file-backed slot `0x08B96AE0`. Both targets have
four-word inspected CFGs.

**VERIFIED bounded stop:** the 842-root / 50,748-word exact-input
slice stops at indirect tail `0x088B9798`, with observed target
`0x088B9378` from checked file-backed slot `0x08B96A10`. One
volatile-memory lock remains held. Fifteen GE lists, one controller
read, sixteen black display frames and zero nonblack pixels in the
captured 480×272 scanout remain. Title-screen boot is not verified.

**PLANNED:** cover the inspected tail target `0x088B9378` and its
reached direct callees, then continue the bounded run.

## Next object after the nested tail

**VERIFIED, 2026-11-19:** checked tail `0x088B9798` reaches
`0x088B9378`; its inspected direct callees are covered. A later shared
virtual call at `0x0885FCF4` observes `0x088A45C4` from file-backed
slot `0x08B95A6C`. Within that method, call `0x088A4634` observes
previously covered `0x088A53DC` from slot `0x08B95AA0`. Tail
`0x088A46B4` then reaches `0x088A500C` through slot `0x08B95A70`.
Two indirect calls inside that target are still terminally trapped
and unobserved.

**VERIFIED bounded stop:** the 848-root / 51,061-word exact-input
slice stops at shared virtual call `0x0885FCF4`, now observing
target `0x088C0358` from checked file-backed slot `0x08B97174`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x088C0358` with its indirect
transfers guarded, then continue the bounded run.
