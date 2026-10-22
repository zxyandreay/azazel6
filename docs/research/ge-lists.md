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

## Guarded graphics object continuation (2026-10-21)

**VERIFIED:** exact-hash ELF pointer slots `0x08BCFC24`, `0x08BCFB6C` and
`0x08BCFB88` select `0x08ACE9C4`, `0x08ACD7F0` and `0x08ACD60C` respectively.
The corresponding call sites `0x08ACF154`, `0x08ACF1B0` and `0x08ACF1E0` now use
the existing checked pointer-call mechanism. Unknown targets still stop; the
continuation includes explicit stops at the newly reachable interface calls.

The 280-root, 23,880-word probe executes the first two guarded methods and their
direct helpers, preserving the prior two VBLANK returns and 209 initialization
state writes. It stops at the direct coverage boundary `0x08ACD010` with return
address `0x08ACC3E4`. The third guarded method is covered but has not executed at
this checkpoint. Windows bounded execution and Android ARM64 compilation pass;
there is still no drawing stream, guest input or menu result. The next startup
step is bounded discovery of that direct helper, followed by its observed
interface ownership. Raw disassembly, generated source and execution logs remain
under `local/`.

## Graphics interface and texture-state continuation (2026-10-22)

**VERIFIED:** the graphics object's interface query at `0x08ACC134` reads
vtable slot `0x08BCFAE4`, selecting `0x08ACD008`. The observed default object has
vtable `0x08BD0708`; its slots `0x08BD070C` and `0x08BD0710` select
`0x08AEBCC4` and `0x08AEBCCC`. Checked calls at `0x08ACD634`/`0x08ACD678` and
tails at `0x08ACD65C`/`0x08ACD6A0` admit only those verified slots. The observed
zero-argument branch executes the latter call/tail pair. The nonzero branch is
covered, not claimed executed.

The graphics setup call at `0x08ACF1F8` reads slot `0x08BCFAF0`, selecting
`0x08ACD030`. The bounded probe executes it and reaches a 32-byte texture-state
[cache writeback](data-cache.md). These are translated game methods, including
the executable's own constant-return methods, not host success stubs. Unknown
object types and targets still stop. No new drawing stream has been submitted.

The state query at `0x08AE7F78` and its related site `0x08AC2F94` use the
reviewed slot `0x08BCFB64` selecting `0x08ACD768`; the former executes on the
observed path. Subsequent matrix setup copies VFPU values and calls direct
helpers before tail-dispatching at `0x08AC2C8C`. Its object's vtable is
`0x08BCF510`; the verified slot `0x08BCF524` selects `0x08AC3CC4`, which sets
two state flags in the guest object. The bounded probe executes this method.
Only the observed interface target is admitted; other object types still stop.

The current 301-root, 24,728-word slice reaches uncovered helper `0x08AC2B54`,
return address `0x08AC61F0`. It retains the two VBLANK returns, one completed
initialization list and 209 state writes. There are three new texture-state
writebacks, but no second list submission or drawing evidence. The next coverage
step needs a bounded CFG and terminal guards for that helper's indirect calls.
