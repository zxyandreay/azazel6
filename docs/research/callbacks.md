# Callback ownership and exit registration

**VERIFIED, 2026-10-19:** post-constructor initialization invokes
`ThreadManForUser::sceKernelCreateCallback` (`0xE81CAF8F`) from `0x08860A54`,
with name at `0x08B8FF70`, function `0x08860A24`, common argument zero. The next
call registers that UID through `LoadExecForUser::sceKernelRegisterExitCallback`
(`0x4AC57943`). Both complete in the native probe; one callback belongs to thread
1 and is selected as the exit callback. No exit event has been delivered.

**DOCUMENTED:** pinned PSPSDK defines the callback function/common argument and
owner-thread status fields. Pinned pspautotests callback create/delete expectations
report copied names up to 31 bytes, duplicate names, null functions, opaque common
arguments, cross-thread deletion and invalid/stale UID error `0x800201A1`. Deleted
owner threads invalidate their callbacks. The reference tests were inspected, not
copied or executed. Pinned uOFW's loadexec source validates callback UID type;
SDKs newer than 3.09 return `0x800200D2` for invalid exit registrations. This exact
game profile uses SDK 5.05. See [source revisions](sources.md).

The profile owns callback metadata in a separate monotonically increasing UID
namespace, with a 4096-object bound and canonical checked name reads. A null
function is stored; nonzero functions must be aligned in the verified executable
layout. The common argument is copied as a 32-bit value, never dereferenced as a
host pointer. Unsupported function/name layouts stop before mutation rather than
claiming arbitrary PSP validation behavior. Deleting a callback or its owner
thread also clears a matching exit registration. Registration replaces the
selected UID without executing guest code.

Callback operations require a running thread and enabled interrupts in this
profile. Owner exit without deletion stops before mutation pending independent
lifetime evidence; exit-delete removes owned callbacks. Notification, callback
waits, pending-exit delivery and callback ABI execution are unimplemented and
retain rejecting import behavior. No callback is treated as a host function or a
normal guest thread.

Synthetic tests cover names, ownership/common values, duplicates, null functions,
stale/cross-type IDs, replacement/deletion, owner cleanup, unsupported pointers,
interrupt state, capacity and runtime isolation. These tests do not establish
callback delivery semantics or PSP exception handling.

**VERIFIED checkpoint:** 199 roots/17122 words compile for Windows and Android
ARM64. All 39 Python cases and nine CTest suites pass. Native execution creates
and registers the exit callback, completes floating-point control setup, and
reaches uncovered helper `0x0889DB64`, return `0x0889DD6C`. The unresolved virtual
call at `0x08860DBC` has a rejecting guard with no allowed targets; it has not
been executed. No callback-delivery or title-screen claim follows from this run.
