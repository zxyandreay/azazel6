# Testing and debugging

**Executed 2026-10-23 on Windows x64:** 69 Python test cases and 18 integrated
CTest suites passed. These cover profile tooling, math/input, startup HLE,
thread/memory ownership, display/GE contracts and three upstream suites.
Python cases include every
truncation of a synthetic ELF, invalid format/headers/segments, BSS/offset conversion,
overflow/overlap, hash mismatch, missing input, profile validation, output confinement,
module/import parsing and malformed descriptor/name/table rejection. Thirteen startup
CFG cases cover calls, returns, both branch paths, loops, instruction budgets,
invalid targets, unsupported transfers and malformed delay slots.

The C++ test checks 16:9 identity, 20:9 results, symmetry and anchors over five
ratios, and invalid values. It throws on failure rather than relying on `assert`,
so Release builds still execute checks. Upstream suites cover framework instructions,
generation/runtime contracts and memory-watch contracts. Original game data is not
used in any CI test. Test fixtures are constructed at runtime and removed locally.
Bootstrap contracts cover SDK-family acceptance/rejection, compiler state, register
and memory preservation, per-runtime state ownership, unknown-library rejection,
and preservation of HLE-directed PC/context switches. Thread tests also cover
nested interrupt suspend/restore and fail-closed mutations in critical sections. These are synthetic tests,
not execution of the game or a complete PSP scheduler.

Dormant-thread tests cover pointer boundaries, name truncation, stack rounding and
overflow, pool exhaustion, reusable holes, stale UIDs, per-runtime ownership,
stack fill/clear behavior and unsupported-option stops. CreateThread is also
reached by the bounded real-input experiment; deletion is synthetic-only evidence.

Use [build commands](building.md) to reproduce locally. Local Python tests,
CMake/CTest, repository audit and official Android AVD execution are the default;
commit and push their reviewed results without starting hosted test runs. Both
GitHub workflows accept manual dispatch only, as an explicit fallback.
`.github/workflows/ci.yml` fetches the pinned framework, builds root integration,
runs native/Python tests and audits repository content on Ubuntu when dispatched.
It does not download commercial input, run the real-game probe or upload generated
output. See the [Android host procedure](android-host.md) for actual local APK
installation, Vulkan pixels, input, lifecycle and runtime evidence.

## Evidence boundaries

| Check | What it proves | What it does not prove |
| --- | --- | --- |
| Synthetic parser tests | Validation/mapping failure contracts | All PSP ELF/PRX formats |
| Framework CTest | Upstream regression expectations | Tekken gameplay correctness |
| Real EBOOT validation | Exact supported input and static structures | Complete function boundaries |
| Auto generation | Generator accepts the input | Complete/usable compiled corpus |
| Entry probe | Actual translated entry instructions reach a known import | Successful kernel bootstrap or game boot |
| Android build | ARM64 native compilation/linking | APK, surface lifecycle or device execution |
| Android host packaging | Synthetic probe APK, signature and ELF/ZIP 16 KiB alignment | Real-device presentation, lifecycle, page-size compatibility or game execution |
| Host input | Stable touch identity, stock-coordinate bounds, cancellation/capacity and analog conversion | Physical controller/touch behavior or guest `sceCtrl` semantics |

## Debugging procedure

Inspect source and the smallest failed log first. For local analysis look at
`report.json` and then `analyzer.log`/`generation.log`. For entry bring-up capture
guest PC, RA, SP, arguments and import identity; compare with the generated slice
and original guest instruction bytes. Keep all translated code/logs local.

Use unit/static checks before broader builds. Bound native execution with
`tools/run_probe.py`; generated intra-unit loops are not necessarily bounded by an
outer dispatch count, so retain the external timeout as coverage expands. Add a
regression for each parser/runtime boundary change. Browser tests are not relevant
to this milestone. The synthetic host has executed on an official local Android
AVD; this does not verify physical ARM64 hardware or game rendering. No sanitizer
run, full-corpus compilation or gameplay benchmark has been performed locally.

For a new indirect boundary, first inspect the current profile and preserve local
work. Add only the bounded caller and an empty-target terminal guard after
checking for conflicting descriptors. Run the exact-hash probe and record the
actual target, object/register evidence and call-delay semantics. Authorize a
nonempty mapping only after reviewing target provenance, memory ownership and
bounded callee coverage; retain guards against every other target. Add synthetic
boundary/failure regressions, rebuild locally and report the actual next stop.
The [graphics startup record](../research/startup-indirect-2026-10-22.md) shows
this procedure in use. If the exact local executable is unavailable, report that
limitation and distinguish synthetic validation from guest execution.
