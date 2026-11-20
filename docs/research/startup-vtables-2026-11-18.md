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

## Next object method and helper chain

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reads file-backed slot `0x08B97174`, reaching `0x088C0358`.
Its direct helpers `0x088C058C` and `0x088BFC18` are reached.
The latter's direct callees are covered; its `0x088AC558` jump
table has fifteen checked file-backed entries within the
`0x088AC510` CFG. Table coverage alone is not evidence that the
branch executed.

**VERIFIED bounded stop:** the 860-root / 51,408-word exact-input
slice stops at indirect tail `0x088C03FC`, observing target
`0x088C0298` from checked file-backed slot `0x08B97178`. One
volatile-memory lock remains held. Fifteen GE lists, one controller
read, sixteen black display frames and zero nonblack pixels in the
captured 480×272 scanout remain. Title-screen boot is not verified.

**PLANNED:** cover the inspected tail target `0x088C0298` and its
reached direct callees, then continue the bounded run.

## Following object after the helper chain

**VERIFIED, 2026-11-19:** indirect tail `0x088C03FC` reaches
`0x088C0298` through checked file-backed slot `0x08B97178`.
Its direct callees are covered. The indirect calls in the
`0x08A68770` CFG are guarded pending an observed target and source.

**VERIFIED bounded stop:** the 865-root / 51,665-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x088B0D34` from checked file-backed slot `0x08B9657C`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x088B0D34` with its indirect
transfers guarded, then continue the bounded run.

## Further object tail continuation

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reaches `0x088B0D34` through checked file-backed slot
`0x08B9657C`. Its tail at `0x088B0D5C` observes `0x088B0F68`
from checked slot `0x08B96580`; the tail target and its direct
callees are covered.

**VERIFIED bounded stop:** the 870-root / 52,040-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x088C183C` from checked file-backed slot `0x08B97294`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** cover the inspected `0x088C183C` CFG and its reached
direct callees, then continue the bounded run.

## Nested callback and next shared dispatch

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reads checked file-backed slot `0x08B97294`, reaching
`0x088C183C` and its direct helper `0x088C1AF4`. A later call
through slot `0x08B9590C` reaches `0x088A392C`, which executes
direct helper `0x08A67118`. Its indirect call at `0x088A39CC`
reads checked slot `0x08B95990` and reaches four-word target
`0x088A3B90`.

**VERIFIED bounded stop:** the 875-root / 52,316-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x088C0FE8` from checked file-backed slot `0x08B97234`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** cover the inspected `0x088C0FE8` CFG with its
indirect transfers guarded, then continue the bounded run.

## Cleanup callback and system-language query

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reaches `0x088C0FE8` through checked slot `0x08B97234`.
Its nested call reaches previously covered `0x088A53DC` from
slot `0x08B95AA0`; tail `0x088C1100` reaches `0x088C0F90`
through slot `0x08B97238`. Cleanup tail `0x08808314` then
observes `0x0892F99C` from slot `0x08B9C580` and later
`0x0892D290` from slot `0x08B9C308`.

**VERIFIED target constraint, HYPOTHESIS source:** the writable
callback at `0x0892D2F4` observes `0x0892F208`, matching the
unique file-backed descriptor word at `0x08B9C4D8`. The generated
dispatch is limited to that exact target. The path that writes
the callback into the object at runtime is not fully traced.

**VERIFIED bounded stop:** the 881-root / 52,631-word exact-input
slice passes `sceUtilityGetSystemParamInt` ID 8 for the fixed
profile language and stops at missing direct function
`0x08968E48` (return address `0x0892F26C`). One volatile-memory
lock remains held. Fifteen GE lists, one controller read,
sixteen black display frames and zero nonblack pixels in the
captured 480×272 scanout remain. Title-screen boot is not verified.

**PLANNED:** cover the inspected `0x08968E48` CFG with its
indirect transfers guarded, then continue the bounded run.

## Language continuation and object call

**VERIFIED, 2026-11-19:** direct `0x08968E48` and its helper
`0x0896A10C` are covered. Its calls at `0x08968EA8` and
`0x08968EC0` resolve through checked file-backed slot
`0x08B8F860` to already covered `0x0885E644`. The later
`0x0892F208` call at `0x0892F27C` reaches `0x088873FC`
through checked slot `0x08B93478`. The next direct call reaches
`0x089699EC`; its three stack-descriptor calls use the same
`0x08B8F860` slot. Its nine-word helper `0x0896A140` is covered.

**VERIFIED target constraint, HYPOTHESIS source:** the
object-field call at `0x08969CD0` observes `0x0885E644`,
matching file-backed slot `0x08B8F860`. Generated dispatch is
limited to that exact target. The write path for the object's
descriptor field has not been fully traced.

**VERIFIED, 2026-11-19:** tail `0x08808314` reaches the inspected
four-word `0x08968B98` through checked vtable slot
`0x08BA39D0`.

**VERIFIED bounded stop:** the 887-root / 53,025-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x089469A0` from checked file-backed slot `0x08B9E39C`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x089469A0` with its indirect
transfers guarded, then continue the bounded run.

## Allocated object and paired descriptor calls

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reaches `0x089469A0` through checked file-backed slot
`0x08B9E39C`. Its direct allocator/helper chain
`0x08955FD0` and `0x08955ADC` is covered.

**VERIFIED target constraint, HYPOTHESIS source:** the paired
object-field calls at `0x089469E0` and `0x08946A18` each
observe `0x08948818`, matching checked file-backed slot
`0x08B9E3D8`. The four-word target and both exact call targets
are covered. The runtime write path for the two object fields
has not been fully traced.

**VERIFIED bounded stop:** the 891-root / 53,184-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x089553C0` from checked file-backed slot `0x08B9F244`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x089553C0` with its indirect
transfers guarded, then continue the bounded run.

## Descriptor array initialization and cleanup

**VERIFIED, 2026-11-19:** shared virtual call `0x0885FCF4`
reaches `0x089553C0` through checked slot `0x08B9F244`.
Its call at `0x089553E0` reads checked slot `0x08B9F25C`
and reaches `0x08954B28`; the latter's tail reads checked
slot `0x08B9F260` and reaches `0x089560A4`. Direct helper
`0x08947E5C` is covered.

**VERIFIED source and bounded targets:** constructor helper
`0x08955ADC` initializes eleven object descriptors, spaced
`0x18` bytes from offset `0x80`, with file-backed vtable
`0x08B9F3A0`. Each call at offsets `0x80` through `0x170`
is constrained to `0x08956A48` through checked slot
`0x08B9F3A8`. The bounded run observed this target at
`0x08956158` and `0x08956214`; the remaining mapped call
sites are source-backed coverage, not a claim they all ran.
The direct cleanup helpers `0x08954820`, `0x08956A58`,
`0x08956A68`, `0x08956A78`, `0x08954964`, and
`0x08958274` are covered.

**VERIFIED bounded stop:** the 902-root / 53,975-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x08948B24` from checked file-backed slot `0x08B9E5E4`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x08948B24` with its indirect
transfers guarded, then continue the bounded run.

## Further shared virtual dispatches

**VERIFIED, 2026-11-19:** shared call `0x0885FCF4` reaches
two-word `0x08948B24` through checked file-backed slot
`0x08B9E5E4`. Subsequent checked slots `0x08B9EACC`,
`0x08B9E62C`, `0x08B9ED7C`, and `0x08B9ED34` reach
`0x0894DBF0`, `0x08948CC0`, `0x089523A0`, and
`0x08952374` respectively. The `0x0894DBF0` direct helper
chain through `0x0894DA2C` and `0x08945E6C` is covered.

**VERIFIED bounded stop:** the 909-root / 54,053-word exact-input
slice stops at shared virtual call `0x0885FCF4`, observing target
`0x0882B788` from checked file-backed slot `0x08B8C23C`.
One volatile-memory lock remains held. Fifteen GE lists, one
controller read, sixteen black display frames and zero nonblack
pixels in the captured 480×272 scanout remain. Title-screen boot
is not verified.

**PLANNED:** inspect and cover `0x0882B788`, guarding its
indirect transfer at `0x0882B7E8` until its target is verified.

## New object call and VFPU conversion

**VERIFIED, 2026-11-19:** shared call `0x0885FCF4` reaches
`0x0882B788` through checked slot `0x08B8C23C`. Its direct
helper `0x08817AD8` is covered. The guarded object call at
`0x0882B7E8` observes `0x0882C5F0` from checked slot
`0x08B8C278`; the exact target is covered. The later direct
`0x08ADD7A4` and its ten inspected callees are covered.

**VERIFIED bounded stop and continuation:** the first run stops
at unsupported `vi2uc.q` word `0xD03C80E0` at `0x08ADE234`.
After [exact-site profile lowering](startup-vi2uc.md), the
924-root / 54,600-word slice executes through both conversion
sites and `0x08ADD964`, then stops at missing direct
`0x08ADD8A0`. The captured scanout remains black with zero
nonblack pixels. Title-screen boot is not verified.

**PLANNED:** inspect and cover `0x08ADD8A0`, then continue the
bounded run.

## AV module and object continuation

**VERIFIED, 2026-11-19:** direct wrappers `0x08ADD8A0`,
`0x08ADD754`, `0x08ADD9F0`, and `0x08ADD8F0` execute through
the `0x0882B788` object loop. Tail `0x08808314` uses checked
slot `0x08B8AF18` to reach `0x08817424`. Later shared calls
use checked slots `0x08BC8FE4`, `0x08BC90DC`, and
`0x08BB0A84` to reach `0x08A54974`, `0x08A56230`, and
`0x089769AC`. Nested calls at `0x08A54904`, `0x08976A0C`,
`0x08976B60`, and `0x08976BC4` use the inspected exact slots.
The `0x08976A80` jump table has eight file-backed entries and
an explicit `<8` index bound.

**VERIFIED bounded stop:** the 953-root / 55,986-word slice
passes three scoped AV module requests and stops at guarded
`0x08976C7C`, observing `0x089780BC`. The captured 480×272
scanout has zero nonblack pixels. Title-screen boot is open.
See [module contract](startup-av-modules.md).

**PLANNED:** inspect the observed target and exact slot for
`0x08976C7C`, then continue the bounded run.

## Nested object continuation

**VERIFIED, 2026-11-20:** `0x08976C7C` reads checked slot
`0x08BB0FB8`, whose exact-input word is `0x089780BC`.
Direct helpers `0x0897668C`, `0x08975894`, `0x0888E168`
and `0x0888D2C0` then execute. Their calls at
`0x089766EC`, `0x08975928`, `0x08975A30`, `0x0888E1F8`
and `0x0888D354` observe the already covered `0x0885E644`
through checked file-backed slot `0x08B8F860`.

**VERIFIED bounded stop:** the 958-root / 56,730-word slice
stops at guarded call `0x0888D4A8`, observing the same
`0x0885E644` target. The captured scanout still has zero
nonblack pixels. This is bounded host execution, not a title
screen or physical ARM64 run.

**PLANNED:** check the slot at `0x0888D4A8` and continue.

## Nested callbacks and constructed list node

**VERIFIED, 2026-11-20:** the calls at `0x0888D4A8` and
`0x0888D508` use checked file-backed slot `0x08B8F860`, whose
exact-input word is `0x0885E644`. Shared dispatch
`0x0885FCF4` then reaches `0x08974BD4`, `0x08986888`,
`0x08A9C104`, `0x08A535EC` and `0x089985D4` through checked
slots `0x08BB0ACC`, `0x08BB271C`, `0x08BCE188`,
`0x08BC8E7C` and `0x08BB36F4`, respectively. Nested calls at
`0x08986930`, `0x089869D8` and `0x08986A7C` also use
`0x08B8F860`. Direct helper `0x089B6C54` is covered.

**VERIFIED callback provenance:** `0x08A9C104` takes a call
target from offset 4 of a runtime list node, then invokes it at
`0x08A9C168`. This pointer is guest-written, not a file-backed
vtable slot. A bounded write watch of node address `0x090A8180`
observed the main thread store `0x08A9CB44` during dispatch
`0x08A9F034`, before the guarded call observed the same target.
Exact-input instructions at `0x08A9F070` and `0x08A9F074`
construct `0x08A9CB44`; `0x08A9F078` stores that value at node
offset 4. The guard for `0x08A9C168` names only this source pair
and target. The watch identifies the dispatch and address, while
the disassembly identifies the store instruction; it does not
claim an instruction-precise watch PC.

**VERIFIED bounded stop:** the 965-root / 57,245-word exact-input
slice passes these callbacks and `0x089B6C54`, then stops at
missing direct function `0x08A3C724`, called from
`0x08998658`. One volatile-memory lock remains held. It records
15 GE lists, one controller read, 16 display frames and zero
nonblack pixels in the captured 480×272 scanout. This is bounded
host execution; title-screen boot and physical ARM64 behavior
remain unverified.

**PLANNED:** inspect and cover `0x08A3C724` and its direct
callees. The candidate inspector reaches an indirect call at
`0x08A3C610` in a called function, so that site needs a reviewed
target map before generation.

## Constructed object and shared helper continuation

**VERIFIED, 2026-11-20:** direct constructor `0x08A3C724`
initializes an object and enters its shared initializer. The
inspected direct chain covers `0x08B793E0`, `0x08B56380`,
`0x08943300` and their checked callees. Two callers share the
tail at `0x08A56C68`; declaring that entry separately avoids
overlapping generated code. This is AOT coverage, not evidence
that every source-backed branch executed.

**VERIFIED object call:** the initializer's call at
`0x08A3C610` observed `0x08A3C7C4`. Constructor
`0x08A3C724` stores subobject vtable `0x08BC8178`; exact
file-backed slot `0x08BC8180` contains the observed target.
The guarded call names only that slot.

**VERIFIED later object call:** a bounded write watch at
guest address `0x090A7E90` observed a main-thread write of
`0x08BB3798` during dispatch `0x089983F4`. Exact-input
instructions at `0x0899853C`/`0x08998540` construct this
vtable, and `0x08998558` stores it at object offset `0x150`.
The call at `0x089986C4` observed `0x08998968`; its exact
file-backed slot `0x08BB37A0` contains that target. The same
constructor stores vtable `0x08BB3738` at offset `0x168`,
whose slot `0x08BB3740` supplies inspected target
`0x08998978` for call `0x089987BC`. That latter site is
source-backed coverage, not a claim it executed in this run.
The write watch reports a dispatch PC, not the exact store PC.

**VERIFIED bounded stop:** the 1,010-root / 59,948-word
exact-input slice passes the observed virtual target and stops
at missing direct `0x08A8CB04`, called from `0x08998760`.
One volatile-memory lock remains held, with 15 GE lists, one
controller read, 16 display frames and zero nonblack pixels in
the captured 480×272 scanout. Title-screen boot and physical
ARM64 behavior remain unverified.

**PLANNED:** cover inspected four-word `0x08A8CB04` and
continue bounded exact-input execution.
