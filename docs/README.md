# Azazel6 technical knowledge base

Reviewed 2026-10-20. Evidence labels apply to individual claims:

- **VERIFIED:** inspected source/bytes or reproduced execution/test in this project.
- **DOCUMENTED:** reported by a reference project, not reproduced here.
- **HYPOTHESIS:** an explanation or expectation requiring an experiment.
- **PLANNED:** a design not implemented.
- **BLOCKED:** progress requires a named prerequisite.

## Project

- [Vision and scope](project/vision.md)
- [Progress and compatibility](project/progress.md)
- [Roadmap](project/roadmap.md)
- [Milestone 001](milestones/001-project-foundation.md)
- [Milestone 002](milestones/002-native-bring-up.md)
- [Milestone 003](milestones/003-boot-sequence.md)
- [Milestone 004](milestones/004-graphics-input.md)

## Architecture

- [Overview, framework and runtime](architecture/overview.md)
- [Recompilation and guest memory](architecture/recompilation.md)
- [Android](architecture/android.md)
- [Renderer and performance](architecture/renderer.md)
- [Native ultrawide migration](architecture/ultrawide.md)
- [Moveset and mod design](architecture/mod-system.md)
- [Decision records](decisions/README.md)

## Research

- [Source inventory and revisions](research/sources.md)
- [Executable and PSP imports](research/executable.md)
- [Startup imports and thread boundary](research/startup-imports.md)
- [Dormant threads and StartThread boundary](research/dormant-threads.md)
- [Thread startup and bootstrap handoff](research/thread-startup.md)
- [Shared partition memory](research/partition-memory.md)
- [Resident module identity](research/module-identity.md)
- [Startup interrupt state](research/interrupt-state.md)
- [Startup coverage and reachability](research/startup-coverage.md)
- [Guarded constructor calls](research/constructor-calls.md)
- [Startup mutex ownership](research/mutexes.md)
- [Guarded formatter jump table](research/jump-tables.md)
- [Formatter callback boundaries](research/formatter-callbacks.md)
- [Current thread priority](research/thread-priority.md)
- [Current-thread VFPU attributes](research/thread-attributes.md)
- [Inactive virtual timers](research/virtual-timers.md)
- [Constructor coverage and FPU branches](research/constructor-coverage.md)
- [Guarded object dispatch](research/virtual-dispatch.md)
- [Coherent startup data-cache ranges](research/data-cache.md)
- [Callback ownership and exit registration](research/callbacks.md)
- [Post-constructor objects and exception boundaries](research/startup-objects.md)
- [Startup fixed memory pools](research/fixed-pools.md)
- [Startup language and confirm-button settings](research/language-mode.md)
- [Startup event flags](research/event-flags.md)
- [Worker entry and immediate address guards](research/worker-dispatch.md)
- [Graphics memory startup boundary](research/graphics-memory.md)
- [GE callback ownership and VBLANK handoff](research/ge-callbacks.md)
- [Dynamic code](research/dynamic-code.md)
- [Moveset findings](research/moveset-findings.md)
- [Open questions](research/open-questions.md)

## Development

- [Setup and builds](development/building.md)
- [Testing and debugging](development/testing.md)
- [Coding standards and workflow](development/workflow.md)

Record guest addresses, file offsets and host pointers distinctly. Each new
investigation should include its question, pinned source, commands, observed
results, limitations and next experiment. Keep raw game-derived evidence local;
publish only reviewed summaries and independently authored tooling.
