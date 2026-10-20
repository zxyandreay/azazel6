# Bounded GE control streams

**VERIFIED synthetic behavior, 2026-10-20:** a profile-owned GE queue models FIFO
list leases, stalls, bounded command execution, control flow and callback return.
The initial control-flow tests are synthetic; the native initialization-list
checkpoint below is separate. Neither path is connected to rendering.

Each list has an explicit aligned canonical-RAM interval. Every command read and
branch target is checked against that interval. Active overlapping intervals reject.
The queue retains at most 64 records, uses non-reused monotonically increasing
host IDs, and allows explicit reclamation. A stalled head cannot be bypassed by
later lists. Updating a stall validates before mutation. Per-call work is bounded,
and a separate 65536-command lifetime budget terminates looping streams even if
the host keeps submitting small work slices. Call depth is capped at eight as a
research resource limit, not a claim about hardware stack capacity.

The independently authored decoder supports NOP, BASE, JUMP, CALL, RET, FINISH,
handler-suspend SIGNAL and END. FINISH/SIGNAL require a following END, including
across a stall. Unsupported drawing commands, state operations, SIGNAL modes, invalid returns,
unowned targets and exhausted budgets produce terminal failures with diagnostics.
Each list owns its control stack. The queue retains shared GE registers, including
BASE, across lists. Nonzero-offset control flow, ORIGIN, VRAM lists and context
save/restore remain rejecting. The restricted initialization HLE is described below.

Callback registrations now have per-slot generations. A queue captures a generation
at submission and rechecks it before dispatch and completion. Unregister/reuse
cannot redirect an old event. Callback arguments remain opaque 32-bit values.
Events have separate monotonically increasing completion tokens. SIGNAL waits for
its callback return; FINISH does not mark the list complete until the callback
returns. An in-flight event prevents list deletion. Duplicate/wrong acknowledgments
reject. Null callbacks need no guest execution.

The host callback bridge uses the owned interrupt stack and guarded return gate.
It checks runtime ownership, interrupt availability and AOT coverage, delivers the
16-bit event value and callback argument, then restores the complete interrupted
context before acknowledging the event. Synthetic AOT functions exercise this
whole route under a finite dispatch bound. No guest renderer or successful GE
completion is fabricated to advance the real-input probe.

**DOCUMENTED references:** pinned PSPSDK `guInternal.h`, `sceGuFinishId.c`,
`sceGuSignal.c`, `sceGuCallList.c`, `pspgu.h` and `pspge.h` describe command numbers,
terminal pairs, SIGNAL modes and callback arguments. Existing GE callback
[ownership evidence](ge-callbacks.md) establishes the registry. Reference source
was inspected only; no implementation was copied. The finite host leases, IDs and
budgets above are explicit Azazel6 policies.

## Native initialization checkpoint (2026-10-21)

**VERIFIED:** the hash-validated native probe submits its first GE list at
`0x08BEFBE8`, with exclusive end `0x08BEFF34`, after the first idle VBLANK. The
investigated 844-byte file-backed span is configured in the Tekken profile,
validated during generation and rechecked at HLE registration. It is not inferred
by scanning arbitrary RAM for END. Startup populates initially empty slots before
submission. Execution retains 209 state updates and completes FINISH/END: 211
commands, with no primitive, transfer or texture load. ListSync then returns only
because that queue entry is Complete.

The independently authored state model retains 24-bit register payloads for
viewport, lighting, material, texture setup, framebuffer, tests, blending and
transfer setup. Vertex/index commands resolve BASE/offset into checked 32-bit
physical addresses; they do not read vertices. View-matrix commands keep 12 exact
float32 bit patterns expanded from GE float24, with bounded sequential indices.
Other matrix streams, wrapping, actual draws, transfers, CLUT loads and cache
operations remain explicit failures. Retained registers do not imply those
operations have been rendered or executed.

The installed EnQueue HLE admits only configured list starts, null stall/options
and callback ID -1, from an interruptible running thread. It executes bounded
initialization work and returns an owned list ID only on completion. Unknown
producers and failed work stop without a success return. ListSync supports the
observed completed-list case, with modes 0/1; incomplete waits and unowned IDs
remain research stops. Synthetic callback delivery remains independently tested;
asynchronous guest enqueue/callback integration is not claimed.

The next observed display service is SetMode(0, 480, 272). It validates mode/size
and waits for the next VBLANK even when called during an existing VBLANK. Under
the explicit bounded idle clock, the native probe completes this wait after a
second handler execution. This is initialization/control-flow evidence only.

**DOCUMENTED:** pinned PSPSDK command declarations and float-emission helpers
establish register numbers and float24 packing. The pinned PPSSPP `sceGe.cpp`
provides enqueue/sync ABI behavior; `sceDisplay.cpp` documents SetMode's wait.
No implementation was copied. Host list IDs and lease limits remain profile policy.

**VERIFIED validation:** 50 Python tests cover generation and file-backed span
bounds, BSS rejection, overlap, capacity and shape. C++ contracts cover retained
state, float bits, address overflow, matrix bounds, unsupported operations, inherited
BASE, enqueue/sync ABI and failure returns. Android ARM64 compilation passes.

**PLANNED:** establish dynamic list producer ownership and asynchronous GE waits;
observe actual draw commands before connecting Vulkan. A Vulkan clear screen or
synthetic triangle would not satisfy the menu-rendering gate.

The bounded slice now contains 272 roots / 23503 instruction words. After SetMode,
four graphics-object method sites are guarded by verified vtable slot
`0x08BCFCBC`, which contains `0x08ACF9F8`. The next allocator method at
`0x08ACF154` remains terminal: its observed target is `0x08ACE9C4`, from an
object with vtable `0x08BCFBD0`. Another unverified method at `0x08AC7358`
also remains guarded. No guessed target is admitted.
