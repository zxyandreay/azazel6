# Open questions and next evidence

| Question | Current evidence | Next step |
| --- | --- | --- |
| First PSP import behavior | VERIFIED two modeled SDK/compiler services return; public source contracts inspected | Validate against PSP observations when available; no firmware-equivalence claim |
| First thread lifecycle | VERIFIED owned dormant thread and StartThread arguments | Establish caller priority, start-argument ownership, initial CPU/stack ABI and scheduling |
| Module-start arguments/stack | Experimental zero args and 64 KiB guest stack | Inspect startup/thread metadata and compare PSP bootstrap state |
| Executable/code boundaries | RWX segment mixes code/data; heuristic false positives | Feed verified import stubs and code ranges to generation |
| Dynamic code | Two PRX resources and module imports | Trace module loads and writes followed by execution |
| Complete HLE service set | 268 statically enumerated imports | Trace order and behavior; dynamic modules may add services |
| Vulkan/GE compatibility | No renderer implemented | Stock GE trace and reusable renderer extraction after boot progresses |
| Gameplay memory/host stack | ELF fits 32 MiB only | Measure allocations, stack high-water marks and code chaining |
| Jin8 freezes | DOCUMENTED multiple failing moves, unproven cause | Multi-move paused CPU/state comparison, no speculative patch |
| PPSSPP performance advantage | Unmeasured | Matched-device A/B after stock combat works |
| Android application | Native compile succeeds, no APK/device run | Build lifecycle-aware host once boot dependencies are implemented |
