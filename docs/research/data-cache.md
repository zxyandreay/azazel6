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

## Texture-state writeback (2026-10-22)

**VERIFIED observation:** after the guarded graphics interface calls, method
`0x08ACD030` reaches `sceKernelDcacheWritebackRange` (`0x3EE30821`) through
`0x08B88FC8`. Its first request is address `0x08BEF374`, size `0x20`, returning
to `0x08ACF200`. The exact-hash producer indexes eight 32-byte command slots,
using a bounded ring index. The reviewed cache-line envelope is
`[0x08BEF340, 0x08BEF480)`; this is a data-coherency lease, not permission to
submit or execute a GE list.

**DOCUMENTED:** the same pinned PSPSDK declares the writeback service void.
uOFW's small-range L1 path visits 64-byte lines, synchronizes and leaves v0 zero.
No firmware implementation was copied. Neither inspection nor coherent host
execution is a measurement of PSP hardware cache timing.

The profile's optional `cache_writeback_spans` must be complete, file-backed,
aligned cache lines. Generation rejects overlap with any discovered instruction
range. Runtime registration rejects overlap, excess count, oversized spans and
spans outside canonical memory preceding BSS. Only writeback can use these
additional leases; writeback/invalidate retains its original BSS/RAM policy.
Both services check sizes, 64-bit end arithmetic and registered code in every
intersecting line before recording success. Shared AOT/HLE RAM is unchanged.
The counters distinguish writebacks from the total completed cache operations.

**VERIFIED execution:** the bounded Windows probe completes three 32-byte
writebacks at `0x08BEF374`, `0x08BEF394` and `0x08BEF3B4`, then continues into
graphics matrix/state setup. The cache counter reaches six total operations,
including the three earlier constructor writeback/invalidate requests. This does
not submit the texture-state ring to a GPU.

Synthetic tests cover coherent data/return ABI, invalidation-policy isolation,
lease boundaries, invalid registration, aliases, overflow and code-line overlap.
General cache emulation, asynchronous device coherency and instruction-cache
invalidation remain unimplemented.
