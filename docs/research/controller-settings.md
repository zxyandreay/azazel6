# Controller sampling settings

**VERIFIED, 2026-11-12:** the game calls `sceCtrlSetSamplingCycle` with zero
from `0x08ADFEBC`, then selects analog mode through `sceCtrlSetSamplingMode`
at `0x08ADFED0`. The native trace records one successful call to each service.
The next stop is uncovered direct callee `0x08887C70`, return `0x088ABACC`.
The 15-frame capture remains black. Guest pad sampling is not yet implemented.

**DOCUMENTED:** pinned PSPSDK declares NIDs `0x6A2774F3` and `0x1F4011E6`
and previous-setting return values. Pinned uOFW `src/kd/ctrl/ctrl.c` initializes
both settings to zero. Cycle zero selects VBLANK updates; nonzero cycles in
5555..20000 use a timer. Digital/analog modes are 0/1. Invalid cycle and mode
errors are `0x800001FE` and `0x80000107`. See [source revisions](sources.md).

The profile supports the observed VBLANK configuration and both sampling modes.
Mode changes return the previous runtime-local value. Invalid values return
the documented errors without changing state. Valid nonzero cycles stop because
timer-driven sampling requires a scheduling contract. Configuration creates no
sample, timestamp or elapsed time; unsupported input reads remain terminal.

**VERIFIED synthetic contracts:** repeated configuration, mode transitions,
previous values, register preservation, invalid boundaries, runtime isolation
and rejection of valid timer intervals. All 94 Python cases, 25 integrated
CTest suites and the Android ARM64 cross-build pass at this checkpoint.
These results do not establish physical input or title boot.
