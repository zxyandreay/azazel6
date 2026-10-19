# Open questions and next evidence

| Question | Current evidence | Next step |
| --- | --- | --- |
| First PSP import behavior | VERIFIED two modeled SDK/compiler services return; public source contracts inspected | Validate against PSP observations when available; no firmware-equivalence claim |
| Thread lifecycle and scheduling | VERIFIED owned stacks, copied StartThread arguments, full context handoff and strict priority changes | Waiting queues, timeouts, equal-priority scheduling and interrupt delivery |
| Module-start arguments/stack | VERIFIED module export specifies priority 32 and 1024-byte stack; zero start arguments remain experimental | Compare complete bootstrap state with PSP observations |
| Executable/code boundaries | RWX segment mixes code/data; heuristic false positives | Feed verified import stubs and code ranges to generation |
| Dynamic code | Two PRX resources and module imports | Trace module loads and writes followed by execution |
| Complete HLE service set | 268 statically enumerated imports | Trace order and behavior; dynamic modules may add services |
| Vulkan/GE compatibility | No renderer implemented | Stock GE trace and reusable renderer extraction after boot progresses |
| Gameplay memory/host stack | VERIFIED shared ownership of image-excluding arenas, stacks, partition blocks and one-block fixed pools | Measure stack high-water marks and validate broader pool/overlay lifetimes |
| Jin8 freezes | DOCUMENTED multiple failing moves, unproven cause | Multi-move paused CPU/state comparison, no speculative patch |
| PPSSPP performance advantage | Unmeasured | Matched-device A/B after stock combat works |
| Android application | Native compile succeeds, no APK/device run | Build lifecycle-aware host once boot dependencies are implemented |
