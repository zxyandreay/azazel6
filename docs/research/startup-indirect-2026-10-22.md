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
