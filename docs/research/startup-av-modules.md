# Startup AV module requests

**VERIFIED, 2026-11-19 exact USA input:** the 55,415-word
bounded slice reaches `sceUtility::0x2A2B3DE0` from
`0x08A56240` with ID `0x301`. The same direct function
requests `0x300` and `0x302` next. The previous run stopped
at the missing import, with sixteen black frames.

**DOCUMENTED:** [PSPSDK's module header](https://pspdev.github.io/pspsdk/psputility__modules_8h_source.html)
names the NID `sceUtilityLoadModule`, identifies `0x300` as
AV codec, `0x301` as SAS core, and `0x302` as ATRAC3+, and
defines duplicate-load result `0x80111102`. [PPSSPP's utility
implementation](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceUtility.cpp)
also tracks loaded modules and documents that repeated AV
loads occur in Tekken 6.

**VERIFIED profile contract:** the `sceUtilityLoadModule` HLE
accepts only IDs `0x300` through `0x302`. It records loaded
IDs per runtime, returns zero for a first request and the
documented duplicate result thereafter. All other IDs stop
without modifying state. Synthetic native tests cover each
accepted ID, duplicate requests, invalid IDs, return ABI and
runtime isolation.

**LIMITATION:** this contract records the three startup
requests; it does not load firmware PRXs or implement their
audio exports. Further audio imports remain rejecting. The
bounded host run records all three requests, then reaches
guarded indirect call `0x08976C7C`, observing `0x089780BC`.
The 953-root / 55,986-word slice still produces zero nonblack
pixels. Neither title-screen boot nor physical ARM64 execution
is verified.

**PLANNED:** inspect the callsite's exact pointer slot and
target, then continue bounded execution.
