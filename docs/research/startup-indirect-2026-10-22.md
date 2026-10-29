# Guarded graphics startup continuation

This research record follows the exact-input Windows native probe through the
matrix-object and graphics-memory setup boundaries. The source checkpoint
`8787384` and subsequent verified increments are now incorporated into `main`.
See [current progress](../project/progress.md) and
[reproduction commands](../development/building.md). Milestones 003 and 004
remain in progress; neither title boot nor gameplay is verified.

## Initial coverage boundary (2026-10-22)

**VERIFIED:** the 301-root / 24,728-word local slice stopped at uncovered
helper `0x08AC2B54`, with return address `0x08AC61F0`, after the previously
uncovered `0x08ACD010` had been added. That earlier helper is a two-instruction
return including its delay slot. The Python suite then passed 64 cases.
The baseline source was `94ccffc2e720`; the additional local startup changes
were preserved in `8787384` before the experiments below.

Every inspection and execution validates USA ULUS10466 SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
Inputs, disassembly, generated translations, logs and captures stay under
ignored `local/`. Source-only research reports do not distribute those artifacts.

The initial bounded scan of `0x08AC2B54` rejected an unknown transfer at
`0x08AC2C00`. Its address-level data flow was:

| Guest site | Meaning |
| --- | --- |
| `0x08AC2BF4` | Load object pointer from `[s0 + 0x3F8]` into a0 |
| `0x08AC2BF8` | Load first object word into v1 |
| `0x08AC2BFC` | Load function pointer from `[v1 + 0x18]` into v0 |
| `0x08AC2C00` | Call through v0, with delay slot at `0x08AC2C04` |

The object and target were initially unverified. No nearby address was assumed
to be a valid virtual method. Four direct callees were already covered:

| Call site | Destination | Existing bounded coverage |
| --- | --- | --- |
| `0x08AC2BD4` | `0x08AC2A64` | 36 words; no new callees |
| `0x08AC2BDC` | `0x08AC295C` | 29 words; no new callees |
| `0x08AC2BE4` | `0x08AC29D0` | 37 words; calls covered `0x08AC46DC` |
| `0x08AC2BEC` | `0x08AC2AFC` | 22 words; no new callees |

The guarded diagnostic procedure is maintained in
[testing and debugging](../development/testing.md). Static CFG discovery alone
never authorizes a target or proves that a path executed.

## Executed terminal diagnostic

**VERIFIED, 2026-10-22:** commit `8787384` contains the previously local source
checkpoint. The working tree was clean and existing local work was preserved.
An ignored backup retains the profile, starting commit and working diff before
this experiment.

The experiment added only root `0x08AC2B54` and the empty-target register-2
trap at `0x08AC2C00`. The bounded CFG contains 45 additional instruction words,
with all four direct callees already covered. The complete slice has 302 roots
and 24,773 words. Generated guard placement was inspected: it precedes target
dispatch, the JALR link update and the delay slot.

The exact-hash Windows probe stops with:

- **VERIFIED:** `Unverified indirect target 0x08AC39A8 at 0x08AC2C00`.
- `a0 = 0x08C44E00`, `v0 = 0x08AC39A8`, `v1 = 0x08BCF510`, and
  the checked first word of the object is `0x08BCF510`.
- `ra = 0x08AC2BF4` remains the prior direct-call return; JALR has not replaced it.
  The runtime reports block-entry PC `0x08AC2BF4`; the stop reason identifies
  the exact trapped instruction. Runner status is `failed`, native exit code 1,
  as expected for this deliberate guard. This is not a successful boot.
- Prior results remain: three texture-state writebacks (six total cache ranges),
  one GE initialization list, 209 GE state writes and two idle VBLANK returns.

**VERIFIED static follow-up:** the observed vtable's slot `0x08BCF528`
(`+0x18`) contains `0x08AC39A8` in the exact executable. Its complete bounded
five-instruction method sets the object's words at offsets `0x24` and `0x1C`
to one and returns zero, including the store in the return delay slot. Both
observed destinations are within the ELF's static zero-initialized memory
extent, ending at `0x08C63044`. It has no additional calls or indirect targets.
This static review alone does not authorize execution; the diagnostic commit
retains the terminal trap. Raw instructions and execution logs remain local.

The local Python suite passes 65 cases, including a synthetic object-load/JALR-v0
fixture verifying terminal CFG reachability and guard ordering before link/delay
effects. The Windows Release build, all 18 integrated CTest suites and the local
Android ARM64 cross-build passed. This native probe was executed on Windows;
cross-compilation is not Android execution, and the Android host diagnostic
remains a separate application.

## Executed checked-slot continuation

**VERIFIED, 2026-10-22:** after the diagnostic commit `32b6e1f`, the profile
replaces only the `0x08AC2C00` terminal trap with a register-2 pointer call
derived from file-backed slot `0x08BCF528`, and adds method root `0x08AC39A8`.
The exact-input validator checks the call instruction, pointer and executable
target. Runtime dispatch still rejects every other target before link or delay
effects. No object table is patched and no success stub is introduced.

The 303-root / 24,782-word slice executes `0x08AC39A8`, returns through
`0x08AC2C08`, and continues graphics setup. The next actual stop is
`Unverified indirect target 0x08ACF96C at 0x08AC7358`, with block-entry PC and
RA `0x08AC7350`, SP `0x09FFE600`, a0 `0x00000200`, v0 `0x08ACF96C`, and
v1 `0x08BCFC88`. This later trap remains terminal. The previous cache, GE and
two-idle-VBLANK results remain unchanged; both workers remain waiting. The
runner reports exit 1 / `failed` for the explicit guard, not successful boot.

Independent synthetic regressions cover a JALR-v0 checked slot and retained
continuation, plus actual runtime dispatch/link/delay/return behavior. A
registered but unreviewed method must still reject without executing either
the target or delay side effect. All 66 Python tests and 18 integrated CTest
suites pass (40.28 seconds for CTest). Windows Release and Android ARM64
cross-builds pass. The exact-hash Windows run above was executed locally;
no Android game execution or new Android host APK behavior is claimed.

## Graphics memory continuation (2026-10-23)

**VERIFIED:** at `0x08AC7358`, the observed v1 value `0x08BCFC88` selects
file-backed slot `0x08BCFCD8` (`+0x50`), containing target `0x08ACF96C`.
The caller supplies its object from s1 in the call delay slot; pre-call a0 is
not that object. Only this checked slot is permitted. Six bounded method/helper
roots are added. The already covered shared entry `0x08AC3414` gets an explicit
root so the two callers do not duplicate its translation. Generation still
rejects overlapping ranges.

The 310-root / 24,944-word slice initially exposed a host registration stack
overflow. The independently tested [registration fix](aot-registration.md)
permits the exact-hash Windows probe to execute the method and its helpers,
including EDRAM-backed allocator state, and return through `0x08AC7360`.
The next stop is `Unverified indirect target 0x08AC7A24 at 0x08AC757C`.
The block-entry PC and RA are `0x08AC7560`; SP is `0x09FFE630`.
The observed object is `0x08C52884`, whose checked first word and v1 are
`0x08BCF5D8`. The terminal guard has not executed the new method.

EDRAM base reads increase from four to five. Three texture-state writebacks,
one initialization list, 209 GE state writes, two idle VBLANK returns and the
two waiting workers remain. There is no draw submission or title-screen result.
All 69 Python tests, 18 CTest suites (57.03 seconds), Windows Release and
Android ARM64 builds pass. The native guarded stop remains exit 1 / `failed`,
with logs retained locally. Static inspection of the next target stops at
unverified indirect call `0x08AC7A58`; its target must be diagnosed before use.

## Nested graphics-object diagnostic (2026-10-23)

**VERIFIED:** the observed object at `0x08C52884` uses vtable `0x08BCF5D8`;
its file-backed slot `0x08BCF5F4` (`+0x1C`) selects `0x08AC7A24`.
That single target is now guarded at `0x08AC757C`. Its bounded prefix calls
the already covered object accessor and stops at a new empty-target guard at
`0x08AC7A58`. The later caller site `0x08AC75B8` also remains terminal.

The 311-root / 24,974-word Windows probe actually reaches the nested guard:
`Unverified indirect target 0x08ACF924 at 0x08AC7A58`. PC and RA are
`0x08AC7A40`; SP is `0x09FFE610`; a0 is `0x08C44E44`. Both the checked
object word and v1 are `0x08BCFC88`, with v0 `0x08ACF924`. The call's stack
store delay slot has not executed. Exact-input static slot `0x08BCFCC0`
(`+0x38`) matches the observed target; this diagnostic does not authorize it.
The earlier GE/display/cache counts remain unchanged. All 69 Python tests,
18 CTest suites and Windows/ARM64 builds pass locally. This remains a deliberate
guarded stop, not a successful boot or Android game execution.

## Graphics resource-query continuation (2026-10-23)

**VERIFIED:** the observed slot `0x08BCFCC0` now guards query sites
`0x08AC7A58`, `0x08AC7AC8` and `0x08AC75B8`. The first two share the same
object retained in s1; the third was separately observed by a terminal probe
with a0 `0x08C44E44`, checked object word/v1 `0x08BCFC88` and target
`0x08ACF924`. Other targets still reject. Bounded coverage adds that method,
its direct resource lookup `0x08AEB824`, and layout helper `0x08AE7B18`.

The final 314-root / 25,159-word Windows probe executes both nested query
returns twice, returns through `0x08AC75C0`, and executes the layout helper.
It then reaches the next direct coverage boundary `0x0885ECE4`, RA
`0x08861050`, SP `0x09FFE680`. Five writebacks (eight total cache ranges)
and nine EDRAM base reads are observed; there is still only one GE list,
209 GE state writes and two idle VBLANK returns. Both workers remain waiting.
Prepared graphics words do not prove submitted draws or visible output.

All 69 Python tests, 18 CTest suites (29.38 seconds), Windows Release and
Android ARM64 builds pass locally. The uncovered-code stop is exit 1 / `failed`.
Static inspection of `0x0885ECE4` requires a terminal diagnostic at indirect
call `0x0885ED34`; no target for that new subsystem has been authorized.

## State transition after graphics initialization (2026-10-23)

**VERIFIED:** adding root `0x0885ECE4` with an empty-target register-2 guard
at `0x0885ED34` produces 315 roots / 25,198 words. The actual startup takes
the empty-list path and returns; the guarded cleanup callback does not execute.
The next stop is the existing state-callback guard:
`Unverified indirect target 0x08862200 at 0x08860FBC`. PC is `0x08860F88`,
RA `0x0885FBAC`, SP `0x09FFE680`. This call uses a1 (register 5), not v0;
the observed a0 is `0x090B17C0`, whose checked first word is `0x08B8FE80`.

The established initial callback remains authorized. Static review traces the
new address from the exact LUI/ADDIU pair `0x08861444` / `0x08861448`, through
the store at `0x08861464` into object offset `0x60`, to the existing dispatcher.
That provenance needs a separate reviewed immediate-source mapping before the
new callback may execute. The alternate virtual-member path stays trapped.
All 69 Python tests, 18 CTest suites (28.30 seconds) and Windows/ARM64 builds
pass. The prior graphics/cache/display observations remain unchanged.


## Checked next-state callback (2026-10-23)

**VERIFIED:** the immediate-source mapping at `0x08860FBC` now preserves both
state callbacks, including the observed `0x08862200` producer described above.
A synthetic two-source test checks both addresses, guard ordering and malformed
second-source rejection. Eight bounded roots cover the callback's first helper
chain; the new virtual call at `0x08AC3DF0` remains terminal.

The 323-root / 25,428-word native probe executes this callback and reaches
`sceKernelDcacheWritebackRange` at `0x08B88FC8`, returning to `0x08AEDE80`.
The request covers `[0x09BF4280, 0x09C74280)` (512 KiB). The current less-than-
64-KiB cache policy rejects it before recording another operation. Eight cache
operations, five writebacks, one GE list, 209 GE state writes and two idle
VBLANK returns remain observed; both workers remain waiting. This exit-1
research boundary is not a successful boot.

All 70 Python cases, 18 CTest suites (32.46 seconds), Windows Release and
Android ARM64 builds pass locally. **PLANNED:** review the large-range firmware
contract and live buffer ownership before extending writeback support. No game
code was executed on Android in this increment.

## Ghidra-assisted matrix continuation (2026-10-23)

**VERIFIED:** the subsequent large-range ownership change `dc92503` completes
the 512 KiB writeback and reaches `0x08AC3DF0` with target `0x08AC3CD8`.
The observed object `0x08C44E00` retains table `0x08BCF510`. The new
[Allegrex workflow](reverse-engineering.md) confirms the caller and four setters
against 45 instruction words from the exact ELF. Pseudocode is supporting
evidence only; its unresolved tail warning remains recorded locally.

The caller reloads the same table after each setter. Slots `+0x0C`, `+0x10`,
`+0x14`, `+0x18` select `0x08AC3CD8`, `0x08AC3CB0`, `0x08AC3CC4`, and
`0x08AC39A8`. The methods only set words at object offsets `0x0C`, `0x14`,
`0x1C`, `0x24` and return zero; none changes the table or calls another function.
All destinations lie within the observed static object's ELF memory extent.
The three JALR sites and final JR tail receive separate exact-slot target guards.
Link and delay-slot effects occur only after the guard; the tail retains RA.
Two new roots supply the previously uncovered setters.

The 325-root / 25,453-word Windows probe executes all four, returns through
`0x08AC5C24` and reaches direct helper `0x08ACE044` from `0x08862268`.
Two additional diagnostic roots then reach `0x08ACF848` with observed target
`0x08ACE2D8`, object `0x08C44E40`, table/v1 `0x08BCFBD0`, RA `0x08ACE064`
and SP `0x09FFE640`. The register-2 guard at that site remains terminal in
this increment. Its file-backed slot at `0x08BCFC7C` (`+0xAC`) matches the
observed target, but executing it requires reviewing its resource helper chain.
Ghidra's second grouped report recovers the caller and wrapper without resolving
this target automatically. The diagnostic slice is 327 roots / 25,478 words.

Six writebacks, nine cache ranges, one GE list, 209 state writes, two idle
VBLANK events and both waiting workers remain unchanged. Every run is bounded;
the new guarded stop is exit 1 / `failed`, not title boot or rendering.
All 76 Python tests pass, including grouped call/tail-slot and refusal regressions.
All 18 CTest suites pass using a local temporary directory (the sandbox's default
temporary path initially caused two upstream rename failures). Windows Release
and Android ARM64 cross-compilation pass. No Android game execution is claimed.

## Resource layout and clock conversion (2026-10-24)

**VERIFIED:** the `0x08ACF848` guard now admits only `0x08ACE2D8` from exact
slot `0x08BCFC7C`. Seventeen bounded roots cover the resource sizing,
subobject initialization and linked-list helpers. Ghidra byte checks and advisory
decompilation of `0x08ACE2D8`, `0x08ACF660` and `0x08ACBE40` supplement CFG,
delay-slot and memory-layout review. The 344-root / 25,783-word native run
returns through `0x08ACE064` to the state callback, stopping at `0x08AAC414`.
Five further allocation helpers yield 349 roots / 25,952 words and reach
`0x08AE032C`. Local grouped evidence retains the intermediate boundaries.

That 34-word routine calls `ThreadManForUser` NID `0xE1619D7C` through stub
`0x08B88E38`: `sceKernelSysClock2USecWide`. Its arguments are 1,000,000 ticks,
null seconds output and a word on the caller's stack. The new original HLE
performs unsigned conversion without advancing any clock. Both output ranges
are checked before either write; only aligned live-caller-stack words are
accepted. Aliases, null pairs, interrupt context and unsupported ownership stop.
The null-first form must return the full low word, not the fractional remainder:
this prevents the observed call from incorrectly receiving zero. PSP autotests
and pinned PPSSPP behavior support this distinction; Yakumo's implementation
does not supply it. No upstream implementation was copied.

The resulting **350-root / 25,986-word** Windows run returns through
`0x08AE034C` and reaches `0x08AE0404` from call site `0x08862294`,
RA `0x0886229C`, SP `0x09FFE670`. Six writebacks, nine cache ranges,
one GE list, 209 state writes, two idle VBLANK events and two waiting workers
remain observed. The exit-1 uncovered-root stop is expected research evidence,
not successful boot, drawing or gameplay.

All 76 Python tests and 18 CTest suites pass locally. Added native tests cover
64-bit extremes, rollover, each optional-output form, register/return behavior,
atomic refusal, stack bounds/alignment and another thread's stack. Windows
Release and Android ARM64 builds pass. **PLANNED:** inspect `0x08AE0404` and
its indirect method before authorizing more coverage. Title boot remains open.

## Setup callback completion (2026-10-24)

**VERIFIED:** diagnostic guards observe `0x08AC7610` at `0x08AE0430`.
The pre-delay registers hold object/s0 `0x08C52884` and table/a1 `0x08BCF5D8`;
a0 has not yet received s0. Guard diagnostics now retain every GPR to avoid
misidentifying stale argument registers as the callee's arguments. Exact slot
`0x08BCF5E8` (`+0x10`) supplies the target. Its three instructions return the
file-backed float at `0x08BCF620` (approximately 59.94) in f0, including the
return delay slot. No host timing constant is changed.

Both `0x08AE0404` and `0x08AE04AC` read this same global object and method;
separate slot guards cover `0x08AE0430` and `0x08AE04CC`. The method and shared
`0x08AC71A0` field accessor do not mutate the table. Ghidra confirms both
callers' scalar calculations and exact instruction bytes. Three subsequent
direct setters cover static state words, packed scalar color conversion and a
scalar float store. Their conditional/likely branches and return delay slots
were reviewed against instructions; no VFPU prefix inference is involved.

The **357-root / 26,220-word** Windows run returns through `0x088622F4`,
completes this state callback and stops at the existing `0x08860FBC` guard with
new target `0x088610C4`, object `0x090B17C0`, RA `0x0885FBAC`,
SP `0x09FFE680`. The target is produced by `0x088622FC` / `0x08862300` and
stored by the already covered state-update path. It remains unauthorized in
this increment pending review of its direct dependencies. GE/display/cache
counts and waiting workers remain unchanged; no title or drawing is claimed.

All 76 Python cases and 18 CTest suites pass (30.83 seconds). Windows Release
and Android ARM64 builds pass. Local Ghidra reports preserve the grouped
instructions/pseudocode and uncertainty warnings. **PLANNED:** review the
31-word next callback and its `0x08AC5B78` / `0x08AAB7C8` helper chain.

## Clear-command production and DrawSync (2026-10-24)

**VERIFIED:** the callback source `0x088622FC` / `0x08862300` reaches the
existing store at `0x08862248`, installing `0x088610C4` at object offset `0x60`.
The immediate guard preserves the prior two targets and admits this third
verified source. Six bounded roots cover the callback, clear generator and
submission wrapper; unknown callback loops at `0x08AC6E7C` / `0x08AC6EAC`
remain terminal. Ghidra's nine-function report supplies byte-checked advisory
evidence, including its unreachable-code and PIC warnings; conservative CFG
coverage retains branches the decompiler discards.

The clear generator's VFPU matrix loads, scalar one, pair dot product and
nearest-integer conversion were checked independently in the pinned decoder,
emitter and context helpers. New synthetic tests exercise matrix-lane selection,
poisoned unused lanes, signed affine values and positive/negative half-way
rounding. The actual pre-callback stop has neutral source/target prefixes
`0xE4` and destination prefix zero; the generator introduces no new prefixes.
No pseudocode translation or timing adjustment was made.

The **363-root / 26,589-word** native slice executes the clear/sprite command
producer at `0x08AC67E8`, then reaches `sceGeDrawSync(0)` at `0x08B890F0`.
The new original HLE returns DONE only when every owned list is complete (or
the queue is empty). It checks mode/context and refuses queued, stalled,
callback-held or failed work without mutation. Polling incomplete work and
asynchronous waits remain unimplemented. This uses the documented PSPSDK
`pspge.h` contract; PortableKit's unconditional DrawSync success is not reused.

After that call the real run stops at **`0x08AC713C`**, RA `0x08AC6DD0`,
SP `0x09FFE640`. The newly generated command block has not been submitted;
the same single completed initialization list, 209 state writes, two display
events and two waiting workers remain. This is not real rendering or title boot.
All 76 Python tests and 19 CTest suites pass, as do Windows Release and Android
ARM64 builds. **PLANNED:** inspect the next frame/list helper chain and retain
strict draw/producer refusal until ownership and rendering are implemented.


## Frame flip and next-edge wait (2026-10-25)

**VERIFIED:** ten reviewed roots extend the slice to **373 roots / 26,800 words**.
The guarded frame-flip method at `0x08AC7154` reads slot `0x08BCF5F0` and
enters `0x08AC7898`. Its two resource queries reuse the already verified
`0x08ACF924` adjustment thunk. Four descriptor calls use slots `0x08BCFAF8`
(getter `0x08ACD028`) and `0x08BCFAF4` (setter `0x08ACD018`). The getter's
return delay slot loads object+0x0C; the setter's branch-likely delay slot stores
only for nonzero input. Exact instructions, observed objects/tables and Ghidra
pseudocode were reviewed independently. Seven slot guards preserve fail-closed
indirect dispatch. Related direct helpers cover framebuffer submission,
writeback and subsequent buffer reset; coverage does not imply all ran.

**DOCUMENTED:** pinned PSPSDK display imports identify NID `0x46F186C3` as
sceDisplayWaitVblankStartCB. **VERIFIED:** the profile now waits for the next
VBLANK edge even if already inside VBLANK, using the existing owned scheduler,
interrupt delivery and framebuffer latch. Only inactive PSP callbacks exist;
notification and general callback delivery remain unimplemented. Synthetic
contracts check both phases, inactive callback retention, full saved context,
disabled interrupts and interrupt-context rejection.

The exact-input Windows run returns from that wait, executes the real VBLANK
handler a third time and latches framebuffer `0x04044000`, stride 512, format 1.
A seventh writeback covers the 256 KiB command allocation at `0x09BF4280`.
The new boundary is **GE enqueue at `0x08B890E8`**, RA `0x08AC6E24`,
SP `0x09FFE640`, with a0=`0x09BF4280`, a1=0, a2=0, a3=0. The current HLE
rejects this dynamic span and owned callback ID rather than pretending to draw.
One initialization list/209 state writes remain; both workers remain Waiting.

**VERIFIED validation:** all 20 native CTest suites and 79 Python cases pass;
Windows Release and Android ARM64 compile/link. The bounded game probe exits 1
at the explicit GE rejection, not success. Local evidence is under
`local/re-frame-sync-ghidra`, `local/re-flip-complete-execution` and
`local/re-cbwait-execution`; generated translations and raw evidence stay local.

**PLANNED:** validate the dynamic command-buffer extent and callback lifetime,
inspect its first drawing commands, and implement the required GE behavior with
independent ownership/renderer tests. No title screen or game rendering is verified.


## First submitted primitive (2026-10-25)

**VERIFIED:** the next increment adds the guarded FINISH callback root and
live written GE leases. The **374-root / 26,826-word** slice reaches command
`0x0406001E` at `0x09BF42B0`, the first clear-mode sprite draw. The GE HLE rejects
that primitive; guest PC remains `0x08B890E8` and no FINISH callback executes.
See [submission ownership and exact state](ge-lists.md#live-written-submissions-2026-10-25).
This supersedes the preceding span-admission blocker. Twenty native suites,
79 Python cases and Android ARM64 compilation pass. One diagnostic relink hit a
Windows executable lock while CTest was running; a sequential retry passed.


## Clear execution and FINISH callback (2026-10-25)

**VERIFIED:** the bounded clear operation now consumes the first primitive and
writes 130,560 pixel positions in guest VRAM. FINISH dispatch enters
`0x08AC6CE4`, then stops at missing `sceKernelGetSystemTimeLow`, stub
`0x08B88EC0`, RA `0x08AC6D08`. The callback has not returned and the queue does
not report completion. See [clear ownership and validation](ge-lists.md#native-bounded-clear-execution-2026-10-25).
The next task is a clock query tied to the existing explicit guest timeline,
without inventing time on reads or converting host execution time to PSP time.


## Two clear/flip cycles (2026-10-25)

**VERIFIED:** read-only system-time HLE lets FINISH return on its owned interrupt
stack. The real run completes two clear/flip cycles: three Complete GE lists,
227 retained state writes, two clears/261,120 pixel positions, four VBLANK events
and four time queries at a final 66,733 us. The next state callback is
**`0x08862338` at guarded site `0x08860FBC`** (a0=`0x090B17C0`). The previous
clear callback has installed a later startup state; its provenance and direct
callees require review before expanding the guard. No title screen is claimed.
See [clock policy and tests](display-foundation.md#finish-system-time-reads-2026-10-25).


## Post-clear timing and state callback (2026-10-27)

**VERIFIED:** the exact input still hashes to
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The reviewed startup profile now contains 423 roots and generates 28,992
instruction words. It follows the post-clear object graph through callback
`0x08862338`, five file-backed vtable/list callback families, graphics-state
setup, and later state callback `0x08862578`. Guards combine independently
verified pointer and immediate provenance only when the call register agrees.
Immediate address construction may cross an ordinary branch only when the
lower instruction is its explicit always-executed delay slot; branch-likely,
link and unknown forms remain rejected.

Three newly observed read-only services use existing owned state. NID
`0x82BC5777` returns the full 64-bit system time from the explicit LCD/event
timeline, NID `0x773DD3A3` returns the current 0..285 Hcount, and
`sceKernelGetThreadId` returns the running scheduler UID. Pointer-form
`sceKernelSysClock2USec` converts a checked eight-byte clock value and writes
only checked words in the live caller stack. None of these queries advances
time or reads a host clock. Synthetic tests cover return registers, wrap,
range/alias rejection, interrupt boundaries and preservation of unrelated
state.

The bounded native run advances from four to five VBLANK events, from three to
four Complete GE lists, from four to nine system-time reads, and from 66,733 to
83,416 guest microseconds. The fourth list contains eight commands. Retained GE
state reaches 230 writes; two clears still account for 261,120 pixel positions.
The current boundary is missing `scePowerGetPllClockFrequencyInt`, NID
`0x34F9C463`, at stub `0x08B89218`, RA `0x089CFBA0`. The pinned PSPSDK export
table and pinned PPSSPP service table identify the ABI as a no-argument integer
query, but no power-clock policy is implemented in this slice. No title, menu,
scene presentation or measured guest CPU timing is claimed.

## Nominal clocks and source-only callback setup (2026-10-27)

**VERIFIED:** the exact input still hashes to
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The reviewed profile now contains 438 roots and generates 29,719 instruction
words. The clock-selection object reads PLL, CPU and bus frequencies once each.
Its selected file-backed profile row is 222/222/111 MHz, matching the immutable
startup state, so it does not call the unimplemented clock setter. These queries
do not advance or rescale the guest timeline.

The continued object chain creates a callback and invokes `sceIoDevctl` for
device `fatms0:`, command `0x02415821`, with a four-byte callback UID in the live
caller stack and null output arguments. The implemented contract validates the
exact device and command, current-thread stack ownership, callback ownership and
duplicates. It records callback ownership only: it opens no host path, reports no
media insertion or capacity, and delivers no callback. Synthetic contracts cover
the successful ABI and atomic rejection of unsupported devices, commands, ranges
and duplicates.

The bounded native counters remain five VBLANK events, four Complete GE lists,
230 GE state writes, two clears/261,120 pixel positions, nine system-time reads
and 83,416 guest microseconds. One `fatms0:` callback is registered and three
guest callbacks exist in total. The current boundary is missing
`scePowerRegisterCallback`, NID `0x04B7766E`, at stub `0x08B89210`, RA
`0x089CF4A8`, with auto-slot `-1` and callback UID `0x50000002`. Power-state
notification and general callback delivery remain unimplemented. No title, menu,
scene presentation, filesystem access or measured guest CPU timing is claimed.

## Power registration and controller idle policy (2026-10-28)

**VERIFIED:** `scePowerRegisterCallback` now owns sixteen runtime-local slots,
implements automatic and explicit allocation with PSP-compatible error returns,
and records the initial `0x10E4` notification for the registered callback. The
notification remains pending because general guest callback delivery is not yet
implemented. Synthetic contracts cover invalid UIDs and slots, occupied and full
tables, disabled-interrupt context, state isolation and atomic failures.

The exact-hash bounded startup run registers callback `0x50000002` in automatic
slot 0, then follows reviewed direct helpers `0x08AAB810`, `0x08B610F4` and
`0x08AAB870`. NID `0xA7144800` is `sceCtrlSetIdleCancelThreshold`; its narrow HLE
accepts signed `-1` or 0..128 and records the observed 64/-1 policy. This setting
does not sample, synthesize or deliver controller input.

The reviewed profile now contains 443 roots and generates 29,840 instruction
words. It follows vtable `0x08BCB1A0` through `0x08A76224` and its file-backed
default no-op callback `0x08A762B8`. The next object uses vtable `0x08B954F8` and
reaches guarded target `0x0889E1DC` at tail site `0x08808314`; that target contains
an unresolved indirect transfer at `0x0889E254`, so its provenance is the next
research boundary. Counters remain five VBLANK events, four Complete GE lists,
230 GE state writes, two clears/261,120 pixel positions and 83,416 guest
microseconds. This bounded native startup evidence does not establish title boot,
scene presentation, controller input or gameplay.

## Semaphore worker and later object passes (2026-10-28)

**VERIFIED:** the exact input still hashes to
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The reviewed profile now contains 472 roots and generates 30,841 instruction
words. The prior object boundary expands through file-backed vtables and a
callback installed from the immediate pair at `0x0889E2BC`/`0x0889E2C0`.
Guards admit each observed target only through its reviewed pointer slot or
immediate construction; unresolved paths retain traps.

The continued initialization creates two semaphores: one empty semaphore with
maximum `0x7fffffff`, and one binary semaphore initially holding one count. The
owned `sceKernelCreateSema` service validates names, attributes, counts and
options. `sceKernelWaitSema` consumes an available count or records an untimed
FIFO wait in the running thread before rescheduling. The newly created priority
19 worker enters `0x08AE1268`, waits for one count on the empty semaphore and
becomes Waiting; the bootstrap thread resumes. Signaling, deletion, priority
wait ordering and timeouts remain unimplemented and fail closed.

The bounded run then completes two additional idle/display cycles. It reaches
seven VBLANK events, six Complete GE lists, 236 retained GE state writes, 19
system-time reads and 116,783 guest microseconds. The two verified clears still
cover 261,120 pixel positions. The next direct helper is `0x08B60854`, called
from `0x0889B5F8`; static inspection stops at its indirect transfer
`0x08B608C0`, which has no reviewed target map yet. This is bounded native
startup evidence, not title boot, scene presentation, controller input or
gameplay.

## Callback dispatch, source-only identity and media boundary (2026-10-28)

**VERIFIED:** the exact input still hashes to
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
The reviewed profile now contains 501 roots and generates 32,210 instruction
words. The `0x08B608C0` target is proven by file-backed vtable slot
`0x08BD6B1C`; later event callbacks are admitted through reviewed immediate
constructions or file-backed callback slots. The event dispatcher at
`0x088995A4` retains terminal traps for dynamic paths without a verified source.

NID `0x0C622081` is handled as a narrow source-only
`sceWlanGetEtherAddr` contract. It writes the fixed locally administered
unicast address `02:00:00:00:00:01` only to a complete six-byte range in the
running caller's live stack. It never examines a host interface, opens a
socket, or establishes connectivity. The exact bounded path reads it once.

The continued callback chain initializes one lightweight mutex workarea and
creates a priority-32 worker with the observed VFPU thread attribute. Both
contracts retain runtime-local ownership and checked guest-memory writes. The
worker reaches `sceUmdActivate` at stub `0x08B89420`, return address
`0x08AA2A5C`. That import remains intentionally unsupported: reporting success
would fabricate media availability, while real media access is outside this
source-only run.

The bounded counters reach eight display frames, seven Complete GE lists, 239
retained GE state writes, 24 system-time reads and 133,466 guest microseconds.
The two verified clears still cover 261,120 pixel positions. These observations
are bounded native startup execution; they do not establish title boot, scene
presentation, media access, networking or gameplay.
