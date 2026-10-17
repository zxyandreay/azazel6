# Testing and debugging

**Executed 2026-10-17 on Windows x64:** 13 Python test cases, the C++ aspect contract
test, and three upstream CTest suites all passed. Python cases include every
truncation of a synthetic ELF, invalid format/headers/segments, BSS/offset conversion,
overflow/overlap, hash mismatch, missing input, profile validation, output confinement,
module/import parsing and malformed descriptor/name/table rejection.

The C++ test checks 16:9 identity, 20:9 results, symmetry and anchors over five
ratios, and invalid values. It throws on failure rather than relying on `assert`,
so Release builds still execute checks. Upstream suites cover framework instructions,
generation/runtime contracts and memory-watch contracts. Original game data is not
used in any CI test. Test fixtures are constructed at runtime and removed locally.

Use [build commands](building.md) to reproduce. The workflow in
`.github/workflows/ci.yml` fetches the pinned framework, builds root integration,
runs native/Python tests and audits repository content on Ubuntu. It does not
download commercial input, run the real-game probe or upload generated output.

## Evidence boundaries

| Check | What it proves | What it does not prove |
| --- | --- | --- |
| Synthetic parser tests | Validation/mapping failure contracts | All PSP ELF/PRX formats |
| Framework CTest | Upstream regression expectations | Tekken gameplay correctness |
| Real EBOOT validation | Exact supported input and static structures | Complete function boundaries |
| Auto generation | Generator accepts the input | Complete/usable compiled corpus |
| Entry probe | Actual translated entry instructions reach a known import | Successful kernel bootstrap or game boot |
| Android build | ARM64 native compilation/linking | APK, surface lifecycle or device execution |

## Debugging procedure

Inspect source and the smallest failed log first. For local analysis look at
`report.json` and then `analyzer.log`/`generation.log`. For entry bring-up capture
guest PC, RA, SP, arguments and import identity; compare with the generated slice
and original guest instruction bytes. Keep all translated code/logs local.

Use unit/static checks before broader builds. Bound native execution with
`tools/run_probe.py`; generated intra-unit loops are not necessarily bounded by an
outer dispatch count, so retain the external timeout as coverage expands. Add a
regression for each parser/runtime boundary change. Browser tests are not relevant
to this milestone. No Android device, sanitizer run, full-corpus compilation,
gameplay benchmark or renderer validation was performed locally.
