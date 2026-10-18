# Current-thread VFPU attributes

**VERIFIED, 2026-10-18:** constructor 23 invokes
`ThreadManForUser::sceKernelChangeCurrentThreadAttr` (`0xEA748E31`) with remove
mask zero and add mask `0x4000`. The exact-hash executable reaches this call at
`0x08B88E50`; the first 23 constructors return after implementing the service.

**DOCUMENTED:** pinned PSPSDK declares `0x4000` as the VFPU thread attribute.
Its VFPU context utility enables this attribute to request register preservation.
Pinned pspautotests `threads/threads/change.expected` reports successful zero
masks and VFPU addition/removal, and `0x80020191` for every other individual bit
in either mask. These are inspected reference expectations, not hardware tests
run by Azazel6. No reference test code was copied. Revisions and licensing are
listed in [sources](sources.md). uOFW has only an unimplemented declaration here.

The profile changes only the running thread's owned attribute metadata. The
scheduler already copies the full CPU context, including all VFPU lanes/control
registers. Changing attributes does not reset these registers, alter priority,
allocate host resources or switch threads. Repeating an add/remove is idempotent;
zero masks leave attributes unchanged. Invalid bits return the documented error
without mutation. Overlapping remove/add masks stop pending precedence evidence.
Missing running context or suspended interrupts also stop before mutation; those
are profile limitations, not claims about firmware errors. VFPU attributes at
thread creation remain outside the existing create-thread contract.

**VERIFIED:** synthetic contracts check transitions, idempotence, all 31 other
bits in both masks, register preservation, runtime isolation and rejection before
mutation. Existing scheduling tests check full-context save/restore. Windows
native tests and Android ARM64 compilation pass. No VFPU exception/trap policy
or full instruction conformance is established by this service.
