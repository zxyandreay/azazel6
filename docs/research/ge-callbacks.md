# GE callback ownership and VBLANK handoff

**VERIFIED, 2026-10-20:** the bounded native run registers one GE finish callback
through `sceGeSetCallback` (`0xA4FC06A4`). The caller constructs a stack record with
null signal function/argument, finish function `0x08AC6CE4` and opaque argument
`0x08C2C400`. The host copies the record and returns slot 0. No callback executes.

**DOCUMENTED:** pinned PSPSDK defines the four-word record and set/unset NIDs.
Pinned uOFW `src/kd/ge/ge.c` describes sixteen slots, lowest-free allocation,
exhaustion error `0x80000022`, and unset error `0x80000100` for IDs outside 0..15.
Unsetting an empty in-range slot succeeds. These references were inspected for
behavior; no firmware implementation was copied. See [revisions](sources.md).

The registry owns sixteen optional records independently in each runtime. Function
addresses are null or aligned words inside the profile's verified executable
range. Arguments remain opaque 32-bit values. The input record must fit entirely
in aligned canonical RAM. Both function pointers are checked before committing
the registration. Unsupported addresses stop without consuming a slot or returning
success. The record can be overwritten immediately after registration without
changing the owned callback. Freed indices can be reused, as documented by the
firmware slot model. These IDs are not ThreadMan callback UIDs.

GE list submission/completion and interrupt delivery remain unimplemented. No
host callback calls a guessed guest function; no list is marked complete to
advance boot. Future queue ownership must handle unregister/reuse explicitly.

**VERIFIED next boundary:** after 16801 dispatches, startup calls
`InterruptManager::sceKernelRegisterSubIntrHandler` (`0xCA04A2B9`) at `0x08B88C68`,
returning to `0x08B55694`. Arguments are interrupt 30, subinterrupt 0, function
`0x08AC6C74`, common argument `0x08C2C408`. Pinned PSPSDK/uOFW identify interrupt
30 as VBLANK. The import remains rejecting; no display interrupt is fabricated.

Coverage is 257 roots/23187 words. Synthetic tests cover all sixteen slots,
exhaustion, copied records, null functions, opaque arguments, reuse, empty unset,
invalid IDs, runtime isolation, executable bounds and mutation-free rejection of
bad records or second callbacks. All 47 Python tests and 14 CTest suites pass;
Windows execution and Android ARM64 compilation pass. This establishes the
[Milestone 004 entry point](../milestones/004-graphics-input.md), not title boot.
