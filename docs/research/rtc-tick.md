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
