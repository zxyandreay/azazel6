# ADR-0001: Pin Yakumo's framework; own the Tekken profile

**Status:** Accepted. **Date:** 2026-10-17.

## Context

Azazel6 needs an Android-capable Allegrex AOT foundation, not Monster Hunter game
behavior. Yakumo evolved from PSPRecomp and exposes a framework-only CMake build,
ELF loader, decoder, runtime dispatch, interpreter fallback, analyzer and generator.
Its successful game-specific HLE/GE host is mostly under `profiles/mhp3rd`, outside
the generic core. See [inspected sources](../research/sources.md).

The fetched Yakumo `main` was exactly
`39ac05f1a166703430d18826bc5877f4feb6b511`, matching the supplied research baseline.
This is the actual selected commit, not a moving branch. PSPRecomp's separate
`main` was also inspected at `f6e7d415c7f447b934cc3865a31eb725f353d659`.

## Decision

Use an external, sparse Git checkout pinned in `dependencies.json`. The fetch tool
selects only framework directories and root files; no Monster Hunter profile is
needed. The Azazel6 CMake project adds this dependency with
`PSPRECOMP_PROFILE=""`, then adds its own `profiles/tekken6` directory. Fetching is
explicit, never a hidden network side effect of configuration.

Retain the upstream framework unmodified. Implement the sectionless Tekken module
reader in the profile tooling. Android ARM64 is primary because a dedicated Android
game application is the requested product; Windows and Linux are bring-up hosts.

## Alternatives and tradeoffs

- Vendoring all Yakumo would import substantial unrelated game logic and licensing
  surface. Rejected.
- Copying selected core files would make upstream fixes harder to reconcile.
  Rejected for now.
- Using original PSPRecomp alone would lose Yakumo's tested fallback/overlay hooks
  and Android-informed evolution. Retained as a reference, not a second dependency.
- A maintained fork may become appropriate for demonstrated generic corrections.
  None is necessary for this milestone.

The network fetch is an initial prerequisite, and upstream build/API changes can
break integration. The current core still contains legacy title-specific optional
diagnostics (for example MHP-prefixed trace controls in `src/runtime.cpp`); the
advertised separation is not perfect. Azazel6 neither enables nor extends these.
Import enumeration from sectionless ELF and code/data separation need profile work.

## Updates, licensing and evidence

Update the lock in a reviewable commit, inspect upstream changes and notices, run
synthetic tests plus framework regressions, regenerate local output, compare import
and code coverage reports, and repeat the bounded native/Android compile probes.
Do not overwrite a dirty dependency checkout. Keep any future necessary generic
patches in a documented fork or explicit patch series, never hidden local edits.

The MIT license and both upstream copyright notices remain in the dependency and
are retained in the root third-party notice. Original game-derived translations
remain excluded. Framework regression tests and Android ARM64 compilation passed;
these results do not establish Tekken graphics, compatibility or performance.
