# Development workflow and coding standards

Keep original source C++20/Python standard-library-only unless a dependency solves
a demonstrated requirement. Use explicit unsigned guest addresses, checked integer
arithmetic and named ownership/lifetime boundaries. Never cast a guest VA to a host
pointer. Keep C++ headers narrow, platform glue out of ELF tooling and gameplay
constants out of the external framework.

Use four-space code indentation, LF endings and UTF-8. Treat tools as local
developer programs, not network services processing arbitrary uploaded binaries.
Validate untrusted file sizes and metadata before allocating, indexing or invoking
downstream tools. Catch expected input errors with useful diagnostics; do not turn
unexpected failures into successful output. Log evidence of actual executed work.

The user owns local input. Ignore rules and index audits are safeguards, not
licenses. Do not commit executable bytes, game-derived C++, copyrighted assets,
private cheat records or machine-specific paths. Recheck third-party provenance
whenever reusing code. Changes to the upstream research repositories are outside
this project's authorization.

Make scoped Conventional Commits, review staged diffs and run relevant checks
before pushing. Update progress and the milestone evidence when behavior changes.
Record unresolved limitations plainly. Avoid force pushes and unrelated cleanup.
Current development continues directly on `main`, pushing reviewed increments
to `origin/main`. Preserve local edits and existing stashes. Only prune a branch
after verifying its complete history is incorporated in the pushed main branch.
