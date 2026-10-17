# Shared partition memory

**VERIFIED 2026-10-18:** a common guest allocator now owns both thread stacks and
user-partition blocks. Its arena excludes the loaded image and bootstrap stack.
Allocations use 256-byte granularity, widened arithmetic, power-of-two alignment,
non-overlapping free intervals and monotonic host tickets. Released adjacent ranges
coalesce implicitly. Thread and partition UIDs occupy disjoint ranges; passing a
thread UID to a partition free cannot release its stack.

The supported SysMemUserForUser subset comprises AllocPartitionMemory, FreePartitionMemory,
GetBlockHeadAddr, MaxFreeMemSize and TotalFreeMemSize. Partition 2 supports low/high,
fixed-address and aligned low/high modes. Fixed byte requests include the containing
256-byte pages. Unsupported partitions stop; invalid size/type/alignment and stale
handles return explicit errors. Guest name pointers are checked before reads.
Failed allocations have no guest-memory writes or lasting allocation ownership.

Source evidence: pinned uOFW `src/kd/sysmem/partition.c` and `memory.c` provide
allocation modes, alignment validation, fixed-address rounding, return codes and
block-head behavior. Pinned PSP autotests' `tests/sysmem/memblock.expected` supports
stale partition UID/head behavior; its newer allocation API's mode restrictions
are not used for the older partition API. No reference source or test body copied.

## Native checkpoint

Adding the reached root `0x08B787CC` produces 293 AOT instruction words in three
bounded CFGs. The native child calls AllocPartitionMemory with partition 2, type 3,
size `0x011C0000` and alignment `0x1000`. It receives UID `0x10000000`; GetBlockHeadAddr
returns `0x08C64000`, yielding owned interval `[0x08C64000, 0x09E24000)` without
overlapping the child stack. Translated initialization then returns to the child
and reaches uncovered direct call `0x08B7B1B4` (RA `0x088042E8`). Logs are local under
`local/entry-probe-memory`; the runner records a coverage failure, not a boot success.

Seven native suites and 21 Python tests pass, including mixed stack/heap exhaustion,
alignment, fixed overlap, failed allocation, stale and cross-type UID tests.
Android ARM64 compilation passes. Allocation addresses, total usable memory and
UID numbering remain host policy rather than a reproduced firmware partition map.
The full OS/kernel reservation layout is not implemented. Continue from the next
observed direct call without replacing guest initialization with a success stub.
