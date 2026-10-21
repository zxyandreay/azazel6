# Roadmap

These are dependency-ordered research milestones, not delivery promises.

| Milestone | Outcome | Exit evidence |
| --- | --- | --- |
| 001 Foundation | Implemented | Pinned framework, validation, analysis, native entry slice, Android cross-build |
| 002 Native bring-up | Implemented: bounded native bootstrap and child execution | First import semantics, wider verified AOT coverage, bootstrap ABI, explicit failure boundaries |
| 003 Boot sequence | In progress: startup completes GE initialization and display mode under bounded idle events; title gate remains open | Kernel threads, scheduler, I/O, module loading and title-screen progression |
| 004 Graphics/input | In progress: [interrupt/display/GE foundation and synthetic Android host APK](../milestones/004-graphics-input.md); game drawing and device verification remain open | GE/Vulkan correctness, menus, gamepads and multitouch |
| 005 Stock combat | Planned | Character selection, training, advancing simulation, audio and stable stock matches |
| 006 Ultrawide | Planned | Verified 3D projection and semantic HUD ownership, round-win matrix |
| 007 Textures/mod platform | Planned | Compatible texture keys, safe resource packages, conflict management |
| 008 Native movesets | Planned | Lifetime-safe table/data ownership, one validated move before Jin8 expansion |
| 009 Android optimization | Planned | Device profiling, frame pacing, bounded memory and long-session stability |
| 010 Public readiness | Planned | Import/install UX, saves, compatibility matrix and distribution review |

Do not migrate gameplay modifications before unmodified combat works. Dynamic code
discoveries can change the split between milestones 002 and 003. Record those
changes in progress and architecture notes rather than claiming static analysis
proved there are no overlays.

The observed VBLANK/GE startup dependency means Milestone 004 integration must
begin before Milestone 003's title-screen gate can close. Neither gate is waived;
the current handoff is documented with exact execution evidence.
