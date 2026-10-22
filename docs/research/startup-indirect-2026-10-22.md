# Local native boot-probe handoff: indirect call at 0x08AC2C00 (2026-10-22)

**Scope / provenance:** This is a research checkpoint assembled from the
maintainer's Windows **local** terminal output and address-only inspection reports.
The 12 locally edited tracked files described below were **not** available in the
remote GitHub checkout and are **not** included in this commit. Do not call this a
verified remote build, a completed Milestone 003, or a playable game.

## Baselines and tests

- The GitHub `main` source before this memo was `94ccffc2e720`.
- The maintainer's local checkout passed **64/64 Python unit tests** after pulling
  that commit. That covers the inspector's new existing-root and import-stub checks,
  but does not test every uncommitted local change or verify game boot.
- The user's exact local decrypted USA ULUS10466 executable passed fingerprint
  validation (SHA-256 `e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`).
  The executable itself must stay in ignored `local/` and never be committed.
- With **uncommitted** local startup/host/tool changes, generation reported **24,728
  startup instruction words**. Native Windows Release `azazel6_entry_probe.exe`
  rebuilt and linked using MSBuild 17.13.19. No GitHub Actions run was needed.
- The rebuilt native probe advanced past the previously uncovered
  `0x08ACD010`, but then stopped at **unregistered guest address
  `0x08AC2B54`**, returning exit code **1** and runner status `failed`.
  This is an AOT coverage boundary, not a title-screen/gameplay milestone.
- The local `startup.json` (not in GitHub's `main` yet) already contains
  `0x08ACD010`. A strict-hash local static audit showed the entry immediately
  returns through an ordinary delay slot: 2 instructions, no additional calls.
  Do not confuse the adjacent preview words with reachable function body.

## Current address-level findings

The maintainer ran `tools/inspect_root.py` against `0x08AC2B54`.
It is **not** in the local startup roots. The strict CFG scan returned
`BLOCKED_NEEDS_REVIEW` at **`0x08AC2C00`**:
`Indirect control transfer ... requires a verified target map`.

Relevant instruction decoding from a verified local, address-only dump:

| Guest site | Meaning |
| --- | --- |
| `0x08AC2BF4` | Load object pointer from `[s0 + 0x3F8]` into `a0` |
| `0x08AC2BF8` | Load first object word into `v1` (possible vtable pointer) |
| `0x08AC2BFC` | Load function pointer from `[v1 + 0x18]` into `v0` |
| `0x08AC2C00` | `jalr v0`, indirect call; delay slot at `0x08AC2C04` |

**Interpretation, not yet proven:** This looks like C++-style virtual dispatch
via a pointer embedded in the current graphics-related object. The actual
runtime object, vtable identity, candidate method and the safety of the target
are **not yet verified**. A target must never be invented, inferred from a nearby
address, or treated as executable merely because a method slot was observed.

The caller also contains four direct `jal` sites, all separately inspected:

| Direct-call site | Direct destination | Local CFG findings |
| --- | --- | --- |
| `0x08AC2BD4` | `0x08AC2A64` | Existing root; 36 instructions; no new callees |
| `0x08AC2BDC` | `0x08AC295C` | Existing root; 29 instructions; no new callees |
| `0x08AC2BE4` | `0x08AC29D0` | Existing root; 37 instructions; direct call to `0x08AC46DC`, no new candidate |
| `0x08AC2BEC` | `0x08AC2AFC` | Existing root; 22 instructions; no new callees |

All four reports were `CFG_DISCOVERED_EXISTING_ROOT` using the user's current
local startup profile. They should **not** be redundantly added as new roots.
Static CFG success is not proof of runtime semantics or executed call paths.

## Proposed next experiment (NOT yet performed in this handoff)

1. Preserve local work and capture a fresh backup/diff before modifications.
2. On a new local working branch, add only `0x08AC2B54` to the **existing**
   startup `roots` array if not already present.
3. Add a **terminal guarded trap**, not an allowlist or arbitrary target:
   `{"site": "0x08AC2C00", "register": 2}` to `trapped_calls`, if no
   conflicting declaration exists. The local code must be audited because it is
   ahead of remote `main`. Ensure generator guard insertion and stopping
   semantics still match the current implementation.
4. Regenerate the bounded native probe, build Release locally, run against the
   strictly verified user-owned EBOOT under existing execution/timeout bounds,
   and record the **actual** indirect target and relevant object-pointer evidence.
   `run_probe.py` may return 1 for a correctly guarded stop; inspect the reason.
5. Do not execute the unknown indirect call, expand an allowlist or remove the
   trap until target provenance and exact runtime safety are established; add
   synthetic unit/integration regressions for any verified mapping.
6. Only after the local probe and tests pass, update milestone/progress files
   and make a reviewed source commit. Never treat a synthetic host APK/AVD test
   as actual Tekken 6 graphics/gameplay verification.

See [Codex continuation guide](../development/codex-handoff-2026-10-22.md).

## Worktrees, local artifacts and safety

At this checkpoint, `git status --short` on the user's **Windows working copy**
showed edits to these **12 tracked files**, none part of this remote memo commit:

- `docs/milestones/004-graphics-input.md`
- `docs/research/data-cache.md`
- `docs/research/display-foundation.md`
- `docs/research/ge-lists.md`
- `profiles/tekken6/config/startup.json`
- `profiles/tekken6/host/entry_probe.cpp`
- `profiles/tekken6/host/memory.cpp`
- `profiles/tekken6/include/azazel6/memory.hpp`
- `profiles/tekken6/tests/memory_test.cpp`
- `tests/test_ge_spans.py`
- `tools/android_runtime_smoke.py`
- `tools/entry_probe.py`

Three separate documentation edits are in the user's existing stash named
`Local Azazel6 documentation before sync`. **Do not auto-pop or discard it**.

Local ignored artifacts include `local/entry-probe/`, build logs,
`local/root-inspection/`, and a backup `local/azazel6-uncommitted-backup.patch`.
Treat these as private user-owned research outputs. Never commit decrypted
EBOOT/ISO, disassembly/raw instruction dumps, generated game code, extracted
assets, secret files, or unreviewed diagnostic logs.

CI workflows must remain manually dispatched; prefer local tests and builds.

## Executed terminal diagnostic on the checkpoint branch

**VERIFIED, 2026-10-22:** `wip/local-startup-2026-10-22` at `8787384`
contains the previously local source checkpoint. The working tree was clean;
the existing documentation stash was left untouched. A fresh ignored backup
retains the profile, starting commit and working diff before this experiment.

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
