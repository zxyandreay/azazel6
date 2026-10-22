# Codex continuation handoff for Azazel6 (2026-10-22)

Use this document with the [native probe research checkpoint](../research/startup-indirect-2026-10-22.md)
and the existing [roadmap](../project/roadmap.md).
This is **not** permission to bypass verified-input or safe-execution boundaries.

## Where to start

Repository: `https://github.com/zxyandreay/azazel6`.

**Important branch distinction:** GitHub `main` initially contains the
published research tools but **not** the maintainer's 12 locally modified source
and documentation files described in the research checkpoint. A cloud Codex
session cloning `main` cannot see those local changes. Before implementing
changes against the newest work, ensure the user has pushed a reviewed
checkpoint branch from the Windows working copy and explicitly select that
branch. If unavailable, limit work to auditable remote research and report
the missing prerequisite. Never recreate unseen source files from prose.

Milestone 002 is implemented; Milestones 003 boot and 004 graphics/input are
**in progress**. Not playable. A synthetic SDL/Vulkan APK/AVD diagnostic does
not include full guest rendering or actual stock Tekken 6 gameplay. Do not
conflate framework compilation, AOT word count, dispatch count, frames in a
diagnostic HLE clock, and user-visible game boot.

## Prioritized task

**First:** review current branch state, `git status`, working diff, history,
existing test/build configurations, safety checks, and the handoff evidence.
Identify what was changed locally (especially startup, memory, data cache,
graphics/GE, test updates) and do not overwrite or duplicate it.

**Second:** investigate verified new native stop at `0x08AC2B54`. The local
exact-hash inspector blocked on `jalr v0` at `0x08AC2C00`; the call target
is loaded from `[[s0+0x3F8]+0x0]+0x18`. Four direct callees are already local
startup roots. Investigate using narrow, bounded instrumentation/terminal guard,
never a guessed static target. The initial conservative diagnostic proposal is
one new root `0x08AC2B54` and one `trapped_calls` entry for site
`0x08AC2C00`, register `2`, only after checking no conflicting local work.
Regenerate the probe; keep runtime guard and no-interpreter policy intact.
Investigate the actual runtime object/vtable and plausible target provenance
before considering any nonempty allowlist or additional root. Be careful to
preserve behavior of the four direct helpers, any VFPU operations, and return
semantics. Add targeted synthetic regressions; isolate local experiments.

**Third:** run validation locally wherever the environment supports it:
`python -m unittest discover -s tests -v`,
`python tools/repository_check.py`,
`python tools/entry_probe.py local/game/EBOOT.BIN --tools-dir local/build/framework/Release`,
CMake Release native build, then the bounded `tools/run_probe.py` invocation.
A full game-specific run requires the user's locally owned exact-decrypted
EBOOT; if cloud Codex lacks it, honestly distinguish tested synthetic tooling
from unavailable game execution and leave exact local commands for the user.
Do not upload or copy game binaries into Codex workspaces or GitHub.

**Fourth:** if new native behavior is evidenced, update the research note and
milestone/progress docs with the measured result and remaining blocker. Preserve
the existing startup/graphics milestones and prior established observations.
Keep changes minimal, defensible, and comprehensively documented.

## Non-negotiable constraints

- Do not trigger or re-enable automated GitHub Actions runs; both workflows
  remain `workflow_dispatch` only. Use local builds/tests.
- Respect `local/` ignore scope and strict ULUS10466 executable hash.
- Keep all unverified indirect branches guarded and unknown imports rejecting.
- Maintain bounded analysis, host dispatch budget and external timeout; no
  interpreter or emulator fallback disguised as native success.
- Do not fabricate actual game title-screen, 3D rendering, controller,
  performance or Android physical-device claims.
- No force push, destructive reset, stash pop, or surprise overwrite.
- Commit only reviewed redistributable source, tests and documentation, in
  focused commits with clear statuses. Include test results and limitations.

## Local validation commands (Windows CMD)

```bat
git status --short
python -m unittest discover -s tests -v
python tools\repository_check.py
python tools\entry_probe.py local\game\EBOOT.BIN --tools-dir local\build\framework\Release
cmake -S . -B local\build\probe -DAZAZEL6_FRAMEWORK=ON -DAZAZEL6_ENTRY_DIR=local/entry-probe -DBUILD_TESTING=OFF
cmake --build local\build\probe --config Release --target azazel6_entry_probe --parallel 2
python tools\run_probe.py local\game\EBOOT.BIN --binary local\build\probe\profiles\tekken6\Release\azazel6_entry_probe.exe
```

The exact-hash game-specific commands above are **not runnable** unless that
local EBOOT is present. The user already ran 64/64 Python tests and a successful
native Release link before the handoff, but the next virtual-call experiment
has not yet been performed.

## Completion report expected from Codex

Report the exact branch/commit IDs; changed paths; validation commands and
pass/fail evidence; observed native stop reason and addresses (if executed);
updated implementation status; and next unverified dependency. Explicitly
state whether locally edited work was incorporated or inaccessible.
