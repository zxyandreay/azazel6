# VBLANK and display foundation

**VERIFIED, 2026-10-20:** exact-hash startup registers and enables interrupt
30/subinterrupt 0. Its handler at `0x08AC6C74` has four instruction words, now
included in AOT coverage. Registration checks executable bounds and an existing
AOT entry. Duplicate registration and missing release use documented errors;
unimplemented controllers and unsupported pointers stop the experiment.
The expanded slice has 262 roots and 23253 words, including an explicitly shared
graphics helper. Startup next reaches uncovered `0x08AC7464`; this is an unfinished
coverage boundary, not a successful display initialization. All 47 Python tests and
16 integrated CTest suites pass.

**VERIFIED synthetic contracts:** owned interrupt delivery uses a separate 4096-byte
guest allocation, bounded pending events, registration generations, the module GP,
and a guarded return gate. It restores all integer, FPU and VFPU state, including
FPR bit patterns. Removed AOT coverage rejects delivery. Release/re-registration
invalidates old events. New VBLANK edges while CPU interrupts are masked are
dropped; already queued events wait for resumption. Nested delivery and interrupt
handler thread mutations remain rejecting. This is not full PSP interrupt HLE.

The display clock accepts explicit elapsed guest ticks at 9 MHz and emits ordered
edges every 150150 ticks (60000 frames in 1001 seconds). Overflow and a per-advance
1024-event budget reject without advancing time. Its epoch is an explicit test
origin; active-scan phase and VBLANK duration are not modeled. No host elapsed time,
dispatch count, or guessed guest instruction cost is converted into LCD time.

Display waits save the entire caller context and only become Ready at the requested
edge. Cancellation removes the deadline and preserves its cancellation result
against stale edges. An idle gate stops when no explicit guest-clock producer is
connected. These scheduler APIs are tested synthetically; display wait imports
are not yet installed in the real-input probe.

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
lifetime on hardware. **PLANNED:** an accounted guest-time producer, display HLE
integration, GE list execution, Android surface/Vulkan and input integration.
