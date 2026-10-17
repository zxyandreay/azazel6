# Architecture and runtime boundaries

**VERIFIED implementation:** the root project contains an independently testable
HUD math library, Python input/analysis tools and optional integration of a pinned
PSPRecomp core. The Tekken profile owns executable identity and the entry probe.

```text
User's decrypted ELF -> strict profile validation -> PSPRecomp analysis/generation
                                       |                         |
                              module/import report         local C++ only
                                                                 |
Android/desktop host <- Tekken bootstrap + HLE policy <- native compiler
                              |
                  generic PSP memory and dispatch
```

The framework supplies Allegrex state, ELF loading, memory access, NID registration,
native function registration, AOT dispatch and fallback interpretation. It does
not supply a ready-to-use generic PSP OS or a Tekken renderer. Yakumo's actual
kernel, Vulkan GE, audio, SDL input and mod implementations live in its game profile;
they require an extraction/design review before reuse.

**PLANNED runtime:** keep guest CPU state and deterministic PSP scheduling on an
owned execution thread. HLE models guest-visible objects and returns scheduling
events. Platform code owns Android surfaces, file descriptors and input queues.
Graphics/audio consume explicit submissions. Do not expose host pointers in guest
registers or let a Java/UI callback mutate guest memory during execution.

Use runtime registrations for verified native overrides. Overlay loading must
invalidate registrations for the replaced guest range, validate the new bytes,
then install matching functions. A fallback has to record coverage and maintain
scheduler boundaries. The entry probe disables interpretation and models settings,
thread lifecycle, priority scheduling, shared stack/heap ownership and resident
module identity. Other imports stop. No default-success syscall behavior is
installed. See [native bring-up outcomes](../milestones/002-native-bring-up.md).

See [the decision](../decisions/ADR-0001-framework-strategy.md),
[memory/recompilation](recompilation.md) and [Android plan](android.md).
