# Exact startup `vi2uc.q` lowering

**VERIFIED, 2026-11-19:** the exact USA executable SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`
has word `0xD03C80E0` at `0x08ADE234` and `0x08ADE25C`.
Both encode `vi2uc.q` with source vector 0 and destination
scalar 96. Yakumo's pinned generator emits terminal
`vfpu4 not lowered yet` stops at those sites. The earlier
bounded run reached the first stop at `0x08ADE234` after
entering `0x08ADE21C`.

**DOCUMENTED instruction behavior:** `vi2uc.q` packs four
signed integer lanes into unsigned bytes. Negative lanes become
zero; positive lanes use bits 30..23. The PSP autotest
[`convert.expected`](https://github.com/hrydgard/pspautotests/blob/071a2a6f14477b8b186bd53201e2f2c0adc80679/tests/cpu/vfpu/convert.expected)
records `0x00020002` for alternating `0x01234567` and negative
lanes, and `0xFE002200` for its third pattern. The profile's
synthetic native test covers all four autotest patterns plus
source swizzle, destination mask, and prefix consumption.

**VERIFIED implementation:** `tools/vfpu_overrides.py` admits
only the two configured file-backed sites when their instruction
word and generated unsupported statement match exactly. It
replaces each terminal statement with a call to the profile
helper, which reads and writes through Yakumo's existing VFPU
views and prefix operations. A changed word or generated form
fails generation. Yakumo remains external and pinned; no generic
fallback or unrelated VFPU word is enabled.

**VERIFIED bounded continuation:** the 924-root / 54,600-word
Windows slice executes past both sites, passes the inspected
`0x08ADD7A4` direct-call chain and `0x08ADD964`, then stops at
missing direct function `0x08ADD8A0` (return address
`0x0882B898`). One volatile-memory lock remains held. Fifteen
GE lists, one controller read, sixteen black display frames and
zero nonblack pixels in the captured 480×272 scanout remain.
This is host execution of a bounded exact-input slice, not title
boot or physical Android execution.

**PLANNED:** inspect `0x08ADD8A0` and continue the bounded run.
