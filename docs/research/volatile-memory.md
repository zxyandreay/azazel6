# Volatile memory startup lock

## Question and pinned input

**VERIFIED, 2026-11-18:** the 774-root bounded native slice reached
`sceSuspendForUser::sceKernelVolatileMemTryLock` at import stub
`0x08B893F0`. The exact USA decrypted executable SHA-256 is
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`;
the separately validated read-only ISO SHA-256 is
`45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8`.
The observed call used mode zero and two output words in a live user
partition block. It came from the inspected helper chain rooted at
`0x08A694D8`.

## Contract and evidence

**DOCUMENTED reference:** the read-only PSP autotest
`local/references/pspautotests/tests/power/volatile/trylock.expected`
reports mode-zero success with base `0x08400000` and size `0x00400000`.
Other modes return `0x80000107`; a second try while held returns
`0x802B0200` without changing outputs. Its unlock expectations report
`0x800201AE` when not held.

**VERIFIED synthetic:** the profile now owns one lock state per runtime.
The nonblocking mode-zero import checks both optional output words for
alignment and ownership in live partition blocks before writing either.
Equal, stale, code, alias, unaligned and overflowing output pointers
fail closed, as does a foreign partition state at registration. Busy,
invalid-mode and unlock paths preserve state as tested.
The blocking `sceKernelVolatileMemLock` and suspend/resume behavior remain
unimplemented.

**VERIFIED execution:** the exact-input, ISO-backed 774-root /
46,118-word slice returns from the observed `TryLock` with one successful
lock. It next stops at the guarded tail `0x08808314`, observing target
`0x088891A0` from vtable `0x08B93CB0`. The lock remains held at that
stop. Fifteen GE lists and sixteen uniformly black 480×272 frames
remain. This is not title-screen or physical ARM64 evidence.

## Next experiment

**PLANNED:** inspect `0x088891A0` and the observed file-backed tail slot,
then rerun with later transfers guarded. Verify eventual unlock and
volatile-region use before broadening the HLE contract.
