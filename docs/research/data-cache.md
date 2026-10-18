# Coherent startup data-cache ranges

**VERIFIED, 2026-10-19:** constructor 32 reaches
`UtilsForUser::sceKernelDcacheWritebackInvalidateRange` (`0x34B9FA9E`) at
`0x08B88FC0`, initially with address `0x08C1AF40`, size `0x5A0`. This interval
is outside the exact-hash ELF's file-backed image, in its zero-initialized data.
After three supported range calls, constructor 32 returns. Constructor 33 reaches
uncovered `0x089B6DB8`, return `0x089C6924`.

**DOCUMENTED:** pinned PSPSDK declares this service as void. Pinned uOFW
`src/kd/sysmem/start.S` implements its small-range L1 path by visiting 64-byte
cache lines and synchronizing; the inspected path leaves v0 zero for sizes below
64 KiB, including zero. These source observations are not a hardware test here.
See [source revisions and licensing](sources.md).

**VERIFIED implementation:** this probe has one CPU context executing at a time.
Yakumo AOT stores and HLE accesses share the same RAM backing; there is no separate
CPU data cache or asynchronous device copy to flush. The handler validates the
entire byte interval using 64-bit arithmetic, permits unaligned data intervals,
and checks every intersecting cache line for registered guest code. It records
completed operations, preserves data and registers except v0, then returns through
the normal import wrapper. It does not clear RAM or unregister translations.

The supported data region begins at the first whole cache line after the verified
file-backed image and ends at canonical RAM's exclusive end. File-backed regions,
aliases, overflow, sizes of 64 KiB or more and registered-code intersections stop
before mutation. Zero-length operations still require an address in this region
or at its exclusive end. These are deliberate profile boundaries, not fabricated
firmware error codes or allocation-lifetime guarantees.

Synthetic tests exercise AOT-to-HLE visibility, unchanged bytes, return ABI,
unaligned/zero/upper-bound intervals, invalid sizes and addresses, and code overlap
in the same cache line. The profile has 169 roots/15957 words. Direct-call batches
remain bounded to 16 roots/2048 additional words, with only one callee level from
executed functions or the newly observed missing root; targets are not recursively
followed within a batch. All 39 Python cases, nine CTest suites and Android ARM64
compilation pass. Native execution retains its 4096-dispatch/15-second bounds.

**PLANNED:** graphics/DMA, additional processors and dynamic-code execution need
explicit cache/coherency integration. This startup handler does not establish
those contracts, instruction-cache behavior or general PSP cache emulation.
