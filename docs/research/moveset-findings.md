# Jin8 migration evidence and unresolved freezes

**DOCUMENTED from pinned Jin8 source; not reproduced in Azazel6.** See
[source revision](sources.md). The existing plugins remain unchanged.

The original master contains 511,607 records and runtime stream 2,458. Static data
staging and event-driven behavior are plausible replacements for repeated CWCheat
execution, but lexical opcode counts do not prove execution semantics. Pointer
writes use live guest structures, and source address words encode operation bits;
they are not host pointers or direct guest addresses without decoding.

Observed moveset-base variants include guest `0x08754400` and `0x086A8800`. A
character ID or a nonzero pointer did not reliably prove battle readiness. Later
KiwiYu telemetry found an inactive timer gate and then actual activation under a
different gate. A high write at guest `0x0A1F0AB0` exceeds classic 32 MiB RAM.
Native recompilation does not fix stale lifetimes, insufficient RAM or bad pointers.

The KiwiYu source/incident history identifies 16 conditional 8.5 MiB snapshots,
sentinel pointer `0x12345678`, broad JIT invalidation and reinstallation hazards.
The v0.4 guards reduce selected risks but are not proof all modified moves work.
Camera instruction guest `0x0895350C` is shared with optional ultrawide presets;
cinematic/speed state needs explicit ownership and restoration.

[Incident 016](https://github.com/zxyandreay/tekken6-jin8/blob/dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0/research/jin-lifecycle/incident-016-multiple-moves-allmove-cpu-probe.md)
corrects an uppercut-only interpretation: several modified moves freeze. A hint
near guest `0x088C6160` and slowed guest ticks are not a captured paused register
proof of the root cause. One later CPU probe did not capture a freeze at all.
No animation deletion or speculative loop patch is justified.

## Reproducible next experiments

1. Establish stock, ultrawide-only, original mod and combined scenarios on the same
   game/emulator/device, with camera presets and expanded-memory setting recorded.
2. Trigger multiple failing move IDs and capture paused guest PC, operands, thread
   backtrace and a bounded surrounding memory range; always resume after capture.
3. Compare pointers, copy sizes and destination ownership before/after activation,
   across character selection, rounds and matches. Trace writes to transition tables.
4. Compare a tiny isolated move's memory effects with the source behavior before
   converting static tables or conditional callbacks to a native package.
5. Measure copy volume, operation count and guest tick progress, distinguishing
   a CPU loop from rendering stall or state-machine wait.

Private cheat/animation resources are not imported or distributed. Freezes remain
unresolved. See [the native package design](../architecture/mod-system.md).
