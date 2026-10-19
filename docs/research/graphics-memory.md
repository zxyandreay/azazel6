# Graphics memory startup boundary

**VERIFIED, 2026-10-19:** the main thread follows the linked-object vtable at
`0x08B8FE80`, through slots `+0x14` and `+0x18`, into its initial state callback.
The table is installed by the executed constructor at `0x08861694`. Guards at
`0x0885FCF4`, `0x0885FBA4` and tail dispatcher `0x08808314` derive their targets
from these file-backed slots. The initial state passes 480 by 272 dimensions to
`0x08AAB4A0`, then enters the lower-level setup helper `0x08AC5C60`.

The exact-hash bounded run reaches `sceGeEdramGetSize` from `0x08AC1BF8`.
**DOCUMENTED:** pinned PSPSDK `pspge.h` and `sceGe_user.S` identify size query
`0x1F6752AD` and base-address query `0xE47E40E4`; see [sources](sources.md).
The profile implements only these immutable geometry queries against the pinned
runtime's allocated VRAM. Registration checks that its canonical EDRAM allocation
is present and has the expected size. Size returns `0x200000`; address returns
guest address `0x04000000`, never a host pointer. The runtime owns the memory;
queries neither allocate nor resize it and do not alter RAM, VRAM or other CPU
registers. Query accounting is runtime-local.

**VERIFIED execution:** size query completes once, address query has not yet been
called. Startup stops at uncovered helper `0x08AC6A78` after 16788 dispatches.
Coverage is 255 roots/23130 words. All constructors, the 2048-record loop, fixed
pool, language settings and event creation still complete, with both workers Ready.
The alternate member call `0x08861008` and graphics-object call `0x08AC757C` remain
terminal guards, with no inferred destinations.

Synthetic tests verify exact guest geometry, returning ABI, register and memory
preservation at both ends of VRAM, isolation and rejection of unknown GE services.
All 47 Python tests and 13 CTest suites pass. Windows execution and Android ARM64
compilation pass. GE command execution, framebuffer presentation, display timing,
and title-screen boot remain **PLANNED**, not implemented by these queries.
