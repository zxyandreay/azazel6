# Startup language and confirm-button settings

**VERIFIED, 2026-10-19:** after its allocator and startup objects initialize, the
game invokes `sceImposeGetLanguageMode` (`0x24FD7BCF`) at `0x08860AB4`, using two
adjacent words on the current thread's stack. The following code checks the
language against a local mask, then calls `sceImposeSetLanguageMode`
(`0x36AA6E91`) before further initialization.

**DOCUMENTED:** pinned PSPSDK supplies the API/NID declarations, language values
0–11, and Circle/Cross values 0/1. The separately pinned PPSSPP language-mode
implementation stores the requested pair, writes the current pair to the output
addresses in language/button order and returns zero. Its behavior is reference
evidence, not a PSP hardware result. See [sources](sources.md); no emulator source
was copied into this implementation.

The exact game profile explicitly selects English (1) and Cross (1) in
`startup.json`. Generation rejects missing, extra, boolean, non-integer or
out-of-range settings before emitting the probe configuration. No host locale or
persistent console setting is read. Each runtime owns its own current pair and
get/set diagnostic counters.

The getter validates both aligned four-byte output intervals before either
write. They must lie in canonical startup data beyond the file-backed image and
outside registered AOT entry points. An invalid second output cannot partially
change the first. Equal output pointers follow the documented write order.
The setter accepts the enumerated language/button domain; other values stop
without changing the pair. Unsupported pointer/value paths do not invent a PSP
error code or report success. Other impose services remain unimplemented.

Synthetic contracts exercise every enumerated pair, isolation, return/register
preservation, overlapping outputs, range ends, alignment, aliases, code overlap,
invalid configuration and mutation-free rejection. These tests establish this
profile contract, not complete firmware behavior or Android locale integration.

**VERIFIED execution:** one getter and one setter complete with language 1 and
confirm button 1. Startup creates a fourth mutex, then reaches
`sceKernelCreateEventFlag` at stub `0x08B88F08`, return `0x089B56E0`, after 15765
dispatches. All constructor returns, the 2048-record initialization loop and the
single-block pool allocation remain intact. Coverage is 238 roots/21593 words;
Windows and Android ARM64 compilation pass. Title-screen boot is still pending.

## System button preference, 2026-11-12

**VERIFIED:** the later object constructor calls `sceUtilityGetSystemParamInt`
(`0xA5DA2406`) with ID 9 and an output word on its live stack. The profile now
returns its explicit Circle/Cross preference for this ID only. This immutable
system preference is separate from mutable `sceImpose` language-mode state.
Unknown IDs, foreign runtime ownership, detached CPU contexts, interrupt-disabled
callers and outputs outside an aligned live caller-stack word reject before writing.

**DOCUMENTED:** pinned PSPSDK names ID 9 `BUTTON_SWAP`. PPSSPP's
[`sceUtility.cpp` at `cf17b0ac7966ba4b172a32340333d209a2d747e7`](https://github.com/hrydgard/ppsspp/blob/cf17b0ac7966ba4b172a32340333d209a2d747e7/Core/HLE/sceUtility.cpp)
returns the configured button preference (with its own compatibility override).
This reference establishes the setting's meaning, not hardware equivalence.

**VERIFIED:** synthetic contracts cover both values, stack endpoints, invalid
configuration, identity, alignment, overflow and mutation-free rejection. The
real-media probe passes the query and reaches `sceCtrlSetSamplingCycle` at
`0x08B89098`, returning to `0x08ADFEC4`. It records 15 frames, 14 GE lists and
28,196 disc bytes; captured scanout remains black. Title boot remains open.
