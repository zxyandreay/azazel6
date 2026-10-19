# Startup fixed memory pools

**VERIFIED, 2026-10-19:** the object initializer reaches
`ThreadManForUser::sceKernelCreateFpl` (`0xC07BB470`) from `0x08879724`. It requests
partition 2, attribute `0x100`, one `0x120000`-byte block and an eight-byte options
structure containing alignment 16. Its next call is `sceKernelTryAllocateFpl`
(`0x623AE665`), with an output slot in the owning guest object.

**DOCUMENTED:** pinned PSPSDK declares the create/delete/allocate/try/free ABI.
Pinned uOFW exports establish the NIDs and error constants. Pinned pspautotests
FPL expected results describe copied 31-byte names, duplicate names, null-name
error `0x80020190`, attributes, alignment options, invalid sizes, stale IDs,
exhaustion and double-free errors. Those references were inspected, not copied
or executed. See [revisions](sources.md).

The profile implements **single-block pools** with low/high allocation and
FIFO/priority attributes. Memory comes from the same allocator used by stacks and
partition blocks; a pool owns an allocation ticket until deletion. UIDs occupy
`0x60000000` through `0x6FFFFFFF`, never reuse a deleted ID and have a 4096-object
limit. Host metadata holds copied names and 32-bit guest addresses. Pool deletion
releases its ticket even if its block remains allocated, without touching other
objects. Pools are kernel objects independent of their creating thread's lifetime.

Creation accepts null, four-byte or eight-byte options; the latter supports power
of two alignment through 4096, with a minimum of eight. Positive block size is
checked before the allocator performs 64-bit rounding and interval validation.
Backing reservations use the profile's 256-byte granularity. This is not a claim
of byte-identical firmware metadata placement or memory accounting.

Immediate allocation writes the block address only after validating the entire
aligned output slot in canonical startup data. Exhausted try-allocation returns
`0x80020190` without changing the output. Free requires the exact currently
allocated block base. Double/interior/foreign frees return `0x800201B6`; stale or
cross-type IDs return `0x8002019D`. Unsupported address layouts stop rather than
fabricating firmware errors. All operations require a running thread and enabled
interrupts in this research profile.

**PLANNED:** multiple-block allocation order, waiting allocation, timeouts,
callback-aware allocation, cancellation/status services and other partitions.
These remain explicit stopping boundaries; there is no waiting queue or callback
delivery. No successful placeholder is registered for them.

**VERIFIED execution:** create and try-allocation complete with UID `0x60000000`
and backing base `0x09E24000`. The existing partition block remains at
`0x08C64000` with size `0x11C0000`; the pool does not overlap it or the thread
stack. Execution then reaches uncovered helper `0x0887929C`. All 59 constructors,
the 2048-record loop and exit registration still complete.

Coverage is 220 roots/19443 words. Added virtual guards at `0x08878C34` and
`0x08878BF4` read slots `0x08B920F4` and `0x08B920F8`: their vtable is installed
at object offset `0x184` by the already investigated initializer. The one-level
bounded coverage batches can now include validated pointer-call destinations
from observed roots, while retaining the 16-root/2048-word batch bounds.

Synthetic pool contracts exercise allocation/free/delete, copied names, alignment,
shared exhaustion, error codes, stale/cross-type IDs, runtime isolation, pointer
bounds, unsupported options, IRQ context and mutation-free stopping paths.
Windows execution and Android ARM64 compilation pass. Title-screen boot remains
unimplemented.
