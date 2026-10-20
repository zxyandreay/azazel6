# VBLANK and display foundation

**VERIFIED, 2026-10-20:** exact-hash startup registers and enables interrupt
30/subinterrupt 0. Its handler at `0x08AC6C74` has four instruction words, now
included in AOT coverage. Registration checks executable bounds and an existing
AOT entry. Duplicate registration and missing release use documented errors;
unimplemented controllers and unsupported pointers stop the experiment.
Startup now accepts the observed display-off framebuffer request and enters a
Waiting state at `sceDisplayWaitVblank`. The scheduler transfers to a priority-48
worker while the priority-40 graphics thread remains waiting. No frame event has
been produced in the real run. The clock does not advance on import calls.
The 267-root/23338-word slice enters worker `0x089B5A70`, validates its observed
vtable slot `0x08BB6E4C`, calls `0x089B5CBC` and acquires mutex `0x30000003`.
Its next member call at `0x089B5CFC` remains guarded; the observed target is
`0x089B5AFC`. Task-object callback `0x089B5AA8` is also a terminal guard. None
of these guards authorizes a guessed callback target.

**VERIFIED synthetic contracts:** owned interrupt delivery uses a separate 4096-byte
guest allocation, bounded pending events, registration generations, the module GP,
and a guarded return gate. It restores all integer, FPU and VFPU state, including
FPR bit patterns. Removed AOT coverage rejects delivery. Release/re-registration
invalidates old events. New VBLANK edges while CPU interrupts are masked are
dropped; already queued events wait for resumption. Nested delivery and interrupt
handler thread mutations remain rejecting. This is not full PSP interrupt HLE.

The display clock accepts explicit half-ticks of the 9 MHz LCD clock and emits ordered
edges every 300300 half-ticks (60000 frames in 1001 seconds). This integer timebase
also represents the reference's 731.5-microsecond VBLANK interval exactly (13167
half-ticks). Overflow and a per-advance
1024-event budget reject without advancing time. Its epoch is an explicit test
origin, with active scan before the first edge and VBLANK following each edge. No host elapsed time,
dispatch count, or guessed guest instruction cost is converted into LCD time.

Display waits save the entire caller context and only become Ready at the requested
edge. Cancellation removes the deadline and preserves its cancellation result
against stale edges. An idle gate stops when no explicit guest-clock producer is
connected. The observed WaitVblank import returns immediately only while the clock
is already in VBLANK, otherwise it queues the caller for the next edge. A synthetic
integration test executes an AOT interrupt, restores its context, wakes the waiting
thread and observes the latched buffer. The event driver yields at every frame
boundary and refuses to advance through an unfinished callback.

Framebuffer descriptors own 32-bit addresses, never host pointers. The stock
480x272 span must fit canonical RAM or the physical 2 MiB VRAM window. Format,
stride, alignment and synchronization are validated before mutation. Next-frame
base changes latch on VBLANK; format/stride changes and the intermediate current
descriptor are checked too. Immediate updates replace pending state. Display-off
descriptors retain no memory span. Aliases, narrow strides and unsupported spans
stop rather than silently wrapping memory. No framebuffer is rendered.

**DOCUMENTED:** pinned PSPSDK headers/export tables and uOFW interrupt registration
provide ABI and error references. PPSSPP's `sceKernelInterrupt.cpp` and
`sceDisplay.cpp` at the [recorded revision](sources.md) additionally describe
enable-before-register, interrupt arguments, mask behavior, framebuffer latch
rules and the LCD clock ratio. These are behavioral references, not reproduced
PSP hardware evidence. No reference implementation was copied or linked.

**BLOCKED visual verification:** `adb devices` reports no attached device. Android
cross-compilation cannot establish title rendering, menu input or resource
lifetime on hardware. **PLANNED:** an accounted guest-time producer, additional observed
display services, game GE list execution, Android surface/Vulkan and input integration.
