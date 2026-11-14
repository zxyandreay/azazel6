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
partial-write rejection and overflow. Other RTC NIDs remain unsupported.

The bounded validated-media run executes one tick query and next stops at
missing game thunk `0x088C3D8C`. That thunk points to `sceRtcTickAddSeconds`
(`0xF2A4AFE5`); its argument contract is the next investigation. The 16-frame
capture remains black. **HYPOTHESIS:** broader runnable-time behavior may
become necessary after this pure tick arithmetic. No such timing model is
authorized by this checkpoint.
