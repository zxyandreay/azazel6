# Azazel6 development rules

Azazel6 researches a standalone native Android ARM64 port of Tekken 6 USA
(ULUS10466). Milestone 001 is tooling and feasibility, not a playable port.

- Read [docs/README.md](docs/README.md), the progress report and applicable ADRs first.
- Only this repository may be changed. Reference checkouts are read-only.
- Keep Yakumo's generic runtime external and pinned. Game addresses, overrides,
  HLE policy and generated-code configuration belong in `profiles/tekken6/`.
- Do not commit game inputs, generated translations, dumps, private cheats,
  assets, captures, credentials or personal paths. All experiments go in `local/`.
- Validate the executable SHA-256 before analysis, generation or execution.
  Never silently substitute a regional variant or guessed GP/overlay layout.
- Guest virtual addresses are 32-bit values, never host pointers. Check ranges,
  overflow, lifetime and ownership before memory access. Mods must fail closed.
- Diagnose source, logs, static checks, focused tests, build output, then runtime.
  Browser automation is unnecessary unless browser behavior is at issue.
- Test new behavior with synthetic fixtures and boundary/failure cases. Run
  `python -m unittest discover -s tests -v`, the CMake tests and repository audit.
  Never label synthetic execution as game execution. Bound every guest run.
- Record VERIFIED, DOCUMENTED, HYPOTHESIS, PLANNED and BLOCKED evidence explicitly.
  Update relevant docs, progress and milestone outcomes when behavior changes.
- No global HUD transform, blanket import-success stub, speculative freeze fix,
  or timing/FPS change. Keep future native modifications reversible and scoped.
- Prefer simple, deterministic tools. Avoid subagents unless explicitly requested.
- Review staged content and `python tools/repository_check.py` before pushing.
  Use meaningful Conventional Commits; never force-push or rewrite shared history.
