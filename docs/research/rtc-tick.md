# RTC tick continuation

**VERIFIED input and execution:** the exact ULUS10466 EBOOT builds callback
`0x0889D4CC` at `0x0889D284/288`. A bounded write watch recorded the
executed constructor `0x0889D1E8` storing it at object `0x090A99C0 + 0x68`.
The guarded call at `0x0889D128` then enters that callback. Its first call
reaches game thunk `0x088C3D3C`, which jumps to import stub `0x08B892C0`.
The validated-media run stopped on `sceRtc::0x3F7AD767` before the HLE was
registered.

**DOCUMENTED contract:** pinned PSPSDK `psprtc.h` names that NID
`sceRtcGetCurrentTick` and specifies a guest `u64` output and zero on success.
Pinned PSP autotests compare two ticks across a thread delay. The pinned
PSPRecomp VCS RTC reference uses microseconds since year 1, with Unix epoch
offset 62,135,596,800,000,000 microseconds.

**VERIFIED Azazel6 contract:** the probe samples host UTC once to anchor a
calendar date. Each tick query adds only elapsed microseconds produced by the
existing explicit guest event clock. Runnable guest instructions, import
reads and host elapsed time do not advance that clock. The HLE validates the
entire eight-byte guest destination and arithmetic before writing, and stops
on invalid buffers or overflow. Deterministic synthetic contracts cover
the epoch conversion, repeated reads, an explicit clock advance, return ABI,
partial-write rejection and overflow. Unknown RTC NIDs remain unsupported.

The bounded validated-media run executes one tick query and next stops at
missing game thunk `0x088C3D8C`. That thunk points to `sceRtcTickAddSeconds`
(`0xF2A4AFE5`); its argument contract is the next investigation. The 16-frame
capture remains black. **HYPOTHESIS:** broader runnable-time behavior may
become necessary after this pure tick arithmetic. No such timing model is
authorized by this checkpoint.

**VERIFIED timezone continuation:** the game calls `sceRtcTickAddSeconds`
with zero seconds, then queries `sceUtilityGetSystemParamInt` ID 6 into an
object inside a live partition allocation. Pinned PSPSDK identifies ID 6 as
the timezone offset in minutes east of UTC. Azazel6 samples the host zone
once with the calendar anchor, accepts only ID 6 and the existing button ID
9, and checks the live caller stack or partition lease before writing. The
game receives 480 minutes in the current local environment and passes that
value to `sceRtcTickAddMinutes`. The shared checked arithmetic handles
positive and negative 64-bit seconds/minutes, source/destination aliasing,
invalid buffers and overflow. The next bounded run stops at `sceRtcSetTick`
(`0x7ED29E40`). Coverage is 641 roots / 39,893 words; one RTC tick read,
one seconds add, one minutes add and a black 16-frame capture.

**VERIFIED, 2026-11-17 calendar and libc continuation:** pinned PSPSDK
`psprtc.h` and `psptypes.h` define `sceRtcSetTick` as conversion from a
microsecond tick to the 16-byte `pspTime` calendar layout. The exact game
called it three times after two current-tick reads, two seconds adds and
three minutes adds. Azazel6 converts through a checked Gregorian date,
supports years 1 through 9999, and rejects truncated or unaligned buffers
and out-of-range ticks without a partial date write. Synthetic tests cover
the year-1 and Unix epochs, leap day and the last supported day.

**DOCUMENTED libc contract:** pinned PSPSDK `psputils.h` and the PSP
autotest identify `UtilsForUser::0x27CC57F0` as `sceKernelLibcTime(time_t*)`.
The firmware reference returns Unix seconds, copies them to a valid optional
output, and returns zero for an invalid output pointer. **VERIFIED Azazel6
contract:** the game called it with a null output pointer. The profile
derives signed 32-bit Unix seconds from the same sampled RTC anchor and
explicit guest-event clock; it does not advance time on a read. Synthetic
tests cover null, valid and invalid outputs, return ABI, overflow and the
2038 boundary.

**VERIFIED exact-input continuation:** the game constructs callback
`0x088675EC` at `0x088677BC/0x088677C0` and callback `0x08865CAC` at
`0x088655C0/0x088655C4`. Both enter through guarded indirect calls. A
subsequent virtual call reads file-backed slot `0x08B908F0`, which contains
`0x08866D34`; the four-word target clears three object fields. The next
indirect call at `0x08865DEC` remains trapped because its target has not
executed in this bounded run. The direct call at `0x08865DA4` reaches the
next missing AOT function `0x0889C958`, whose CFG contains an unreviewed
indirect transfer at `0x0889C98C`. Current coverage is 648 roots / 40,165
words. The run reports failure at that exact boundary. Its 16-frame,
480×272 capture is uniformly black; title boot is unverified.

**VERIFIED, 2026-11-17 virtual-call continuation:** the object at
`0x090A9764` carries file-backed vtable `0x08B8C2D0`; slot
`0x08B8C2D8` points to existing root `0x0882C5E0`. The guarded call at
`0x0889C98C` executes that target and reaches direct helper
`0x08889BD4`. The next two calls in `0x08865CAC`, at `0x08865DEC` and
`0x08865E5C`, each observe file-backed slot `0x08B908F0` and target
`0x08866D34`. The subsequent direct helper `0x0886CC28` executes.
The fourth call at `0x08865ECC` is still trapped; execution first reaches
missing direct helper `0x088AD6C4`. Its static CFG has uncovered direct
callees `0x0882D62C` and `0x0885F17C`. The latest bounded native
slice covers 651 roots / 40,468 words, fails at that helper, and retains
the uniformly black 16-frame capture. The prior 648-root record above
remains a historical checkpoint.

**VERIFIED, 2026-11-17 object initialization continuation:** the exact
`0x088AD6C4` helper and its two reviewed direct callees execute before
the fourth virtual call. That call, at `0x08865ECC`, observes vtable
slot `0x08B908F0` and target `0x08866D34`. The reviewed helper
`0x088B99A8` executes before the fifth call at `0x08865F3C`, which
observes the same slot and target. The sixth call at `0x08865FAC`
remains trapped: execution first stops at direct helper `0x0892FEF4`,
whose CFG exposes uncovered callee `0x08825ED4`. Current coverage is
655 roots / 40,754 words. The bounded run reports failure there, with
the same black 16-frame scanout. No title-screen claim follows.

**VERIFIED, 2026-11-17 tail-path continuation:** the sixth object's
direct helper `0x0892FEF4` enters `0x08825ED4`. The latter's exact
EBOOT code branches into a distant direct tail path; its two indirect
sites `0x08ACE4F8` and `0x08ACE580` were first held as terminal traps.
The executed path passes reviewed helpers `0x08ACB214`, `0x08ACB0B0`
and `0x08ACB0B8`. At `0x08ACE4F8`, the live object carries file-backed
vtable `0x08BCFBD0`; slot `0x08BCFC20` contains `0x08ACE978`. The
guarded dispatch executes that 19-word target. It then enters direct
helper `0x08ACB68C`, whose two indirect sites remain trapped, and
stops first at missing 3-word callee `0x08ACB1BC`. Coverage is 664
roots / 41,111 words. The bounded run fails at that direct callee;
sixteen captured frames remain uniformly black.

**VERIFIED, 2026-11-17 checked object/cache continuation:** the sixth
object's tail path passes direct helper `0x08ACB1BC` and follows the
file-backed vtable at `0x08BCFC88`. Live guarded calls observe slots
`0x08BCFCC0`, `0x08BCFCA0` and `0x08BCFB84`, targeting
`0x08ACF924`, `0x08ACF8D0` and `0x08ACD304` respectively. Inner
dispatches use checked slots `0x08BCF9D0`, `0x08BCF9FC`,
`0x08BCFA00`, `0x08BCFA04` and `0x08BCF9D4`. The exact path
performs a null `0x80`-byte data-cache writeback, then a file-backed
writeback from `0x08B9C610`; both are covered by narrow profile
contracts and synthetic rejection tests. The following direct
five-instruction helper at `0x08ACB2D4` executes. A later guarded
call reads `0x08BCFCC8` and targets `0x08ACF93C`. Further exposed
indirect sites remain trapped pending live evidence. This is native
host execution with validated media, not a title-screen boot.

**VERIFIED bounded checkpoint:** the `0x08ACF93C` path enters direct
`0x08AEBAF8` and its shared tail target `0x08AC1E5C`. The next live
vtable call at `0x08ACB87C` reads file-backed slot `0x08BCFCAC` and
targets `0x08ACF8F4`. The resulting path stops at missing direct
helper `0x08ACB2A8` before reaching the still-trapped later indirect
sites. Coverage is 680 roots / 41,491 words. The ISO-backed,
15-second guest run reports failure at that boundary; its sixteen
480×272 frames have zero nonblack pixels. The 240-second media
validation bound is separate from the guest execution bound.

**VERIFIED bounded continuation:** direct helpers `0x08ACB2A8`,
`0x08AEB9D8`, `0x08AECA28` and the `0x08AEBB00` tail thunk execute.
The subsequent guarded call at `0x08ACB8A0` observes object vtable
`0x08BD0790`, whose file-backed slot `0x08BD07B8` contains
`0x08AEC8CC`. Its shared tail code at `0x08ACD838` was separated
into a nonoverlapping AOT root. The 686-root / 41,585-word run next
stops at missing direct helper `0x08ACB200`, called from
`0x08AEC8F0`. Later indirect calls remain trapped. The validated ISO
run has fifteen GE lists, sixteen black display frames and no title.

**VERIFIED, 2026-11-17 second object continuation:** exact direct helper
`0x08ACB200` executes. The object at `0x09CD6300` changes the live
target at `0x08ACD868` from the first checked vtable family to
file-backed slot `0x08BCFA90` (`0x08ACCEF0`). Its guarded methods
use slots `0x08BCFABC`, `0x08BCFAC0`, `0x08BCFAC4`,
`0x08BCFA94` and `0x08BCFAB0`, with observed targets
`0x08ACC67C`, `0x08ACCE50`, `0x08ACCE18`, `0x08ACCF74`
and `0x08ACCFC8`/`0x08ACCFD0` through the shared accessors.
The second null and extended file-backed cache requests complete
under narrow contracts. A later tail call at `0x08ACD65C` observes
file-backed slot `0x08BD0794` and executes `0x08ACD6F0`.
The 694-root / 41,706-word bounded ISO run next stops at missing
direct helper `0x08ACBCF8`. It reports 43 cache operations and 34
writebacks; fifteen GE lists and sixteen uniformly black frames are
unchanged. This is host-native game startup research, not title boot.

**VERIFIED continued object construction:** direct helper
`0x08ACBCF8` executes, then the `0x08AD8A24` thunk enters a reviewed
tail path. The sixth virtual call at `0x08865FAC` observes file-backed
slot `0x08B908F0`, containing existing target `0x08866D34`.
Direct constructor `0x0892F114` then executes, followed by another
checked call through the same slot at `0x0886601C`. The 697-root /
41,933-word bounded run next stops at missing direct helper
`0x089484A0`, before the still-trapped call at `0x0886608C`.
There are still fifteen GE lists, sixteen black frames and zero
nonblack pixels; no title-screen inference is warranted.

**VERIFIED further constructor sequence:** exact constructors
`0x089484A0`, `0x0882BBCC`, `0x08A54D6C` and `0x089773E4`
execute on the continuing startup path. The virtual calls at
`0x0886608C`, `0x088660FC`, `0x0886616C` and `0x088661DC`
each observe file-backed slot `0x08B908F0` with target
`0x08866D34`. Every next indirect site was first held as a
terminal trap until its live target could be checked. The latest
701-root / 42,368-word bounded ISO run stops at missing direct
helper `0x089872C8`, before the trapped call at `0x0886624C`.
The capture still contains sixteen black 480×272 frames; there
are fifteen GE lists and no title-screen evidence.

**VERIFIED, 2026-11-18 next constructor path:** direct constructor
`0x089872C8` executes and the virtual call at `0x0886624C`
observes file-backed slot `0x08B908F0`, targeting existing root
`0x08866D34`. A direct thunk at `0x08A9F1B8` leads to constructor
`0x08A9F020`. The 704-root / 42,619-word bounded run stops at
missing direct helper `0x08AA0BF8` before the still-trapped call
at `0x088662A0`. Fifteen GE lists and sixteen black frames are
unchanged. No title-screen boot has been observed.

**VERIFIED next checked call:** the exact four-instruction helper
`0x08AA0BF8` clears object fields, then the virtual call at
`0x088662A0` observes file-backed slot `0x08B908F0` and target
`0x08866D34`. The 705-root / 42,658-word bounded ISO run next
stops at missing direct constructor `0x08A53A08`, before the
still-trapped indirect call at `0x08866310`. Fifteen GE lists and
sixteen black frames remain unchanged.
