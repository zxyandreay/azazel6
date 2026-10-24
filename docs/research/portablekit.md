# PortableKit compatibility investigation

**2026-10-24: optional research integration; primary AOT architecture unchanged.**
The user suggested [PortableKit](https://github.com/TeamGDB/PortableKit).
Revision `33800c4060c53f3233e08f8ab2cc4590a0872063` is pinned separately in
`dependencies.json`. Its README describes a generic system layer extracted from
Yakumo; Yakumo's existing copy had not migrated to it. Neither another game's
playable status nor an upstream interpreter run establishes Tekken compatibility.

## Verified local results

- The unchanged Windows host builds with renderer and FFmpeg disabled, without
  downloading media dependencies. Sectionless module discovery recognizes the
  exact input, entry `0x08804124`, GP `0x08C022B0` and 268 imports.
- With strict imports, unadapted execution stops after 13 interpreted instructions
  at SDK-version import `SysMemUserForUser::0x91DE343C`. It registers zero fallback
  logging/success stubs. That mode does not prove every explicitly named upstream
  handler implements its full PSP contract.
- An exploratory local harness compiling Azazel6's existing SDK/compiler metadata
  handlers unchanged progressed to a 1,000,000-dispatch limit at `0x089FD4B8`,
  with file-worker threads and repeated GE/display calls. No ISO, renderer or
  audio was supplied. The trace is **diagnostic only**, not verified title boot:
  known semantic gaps below prevent promoting that run to milestone evidence.
- The reproducible probe initially reproduced that wide-clock refusal. It now
  shares Azazel6's checked conversion helper, using PortableKit's live thread
  stack owner. Metadata and clock compatibility tests pass against the external
  core. With this adapter, the bounded no-disc run reaches its 100,000-dispatch
  limit at `0x08AC6C64`, with two waiting event workers and file-worker threads.
  This remains upstream diagnostic evidence, not title boot or native AOT progress.

## Admission and reproduction

Clone the official repository under ignored `local/deps/portablekit` and check
out the pinned revision above. Both configuration and the runner reject a
different revision or a dirty checkout; neither resets or edits the dependency.

```sh
python tools/portablekit_probe.py build
python tools/portablekit_probe.py run local/game/EBOOT.BIN --output local/portablekit-investigation
```

The output must be a fresh directory under `local/`. The executable is checked
before analysis or execution, again inside the native program, and its module
identity is independently checked. Runs have a 15-second wall limit and 100,000
dispatch limit. Inherited upstream switches are removed; settings and memory-stick
paths stay local, strict imports are enabled and busy-clock/fast-loading options
are disabled. No installer, decryption, ISO, generated corpus or Vulkan surface is
used by this headless target. Output is private diagnostic evidence.

## Reuse decision and concrete gaps

**VERIFIED source review:** `host/profile.hpp` removes many Monster Hunter
constants from kernel, ISO, GE, Vulkan, audio and input services. Its external
`portablekit_add_game` interface is a substantially better starting point for
future whole-host reuse than copying Yakumo's game profile. However:

| Component | Decision / dependency |
| --- | --- |
| Core/bootstrap | Optional side-by-side evaluation uses the existing original metadata handlers; no core lock or production configuration change. |
| Clock conversion | `hle_threadman.cpp` returns a remainder for a null-first wide-clock output. The profile overrides this with the same checked helper used by the primary runtime, including atomic output preflight and live-stack ownership. |
| GE synchronization | `hle_media.cpp` DrawSync unconditionally returns zero, including potential pending work. Azazel6 retains its tested completed-queue predicate and refusing asynchronous contract. |
| GE decoder and Vulkan | PortableKit separates profile policy and exposes backend-independent decoded draw calls. Prefer external compilation of that interface, with checked producer/vertex ownership, when integrating observed draws. Headless execution is not rendering evidence. |
| File services | `kernel/iso_image.*` offers a read-only ISO9660 reader; validate the supplied disc independently before mounting. The exploratory no-disc loop is not a filesystem success test. |
| Scheduling/overlays | Keep separate runtime owners. Verify module/thread initialization, interrupt/callback order, clock policy and exact overlay ranges before replacing existing behavior. |
| Audio/input | Available upstream; not exercised here. Media-disabled behavior cannot establish stock audio or movie progression. |

**PLANNED:** resolve the remaining compatibility gaps in a checked profile adapter,
then compare bounded traces with a validated disc and review actually invoked
services. Adopt larger components only with an explicit architecture decision
and regression evidence. Do not disable strict admission or import refusal to
make the comparison advance.

**VERIFIED validation:** 79 Python cases cover input refusal, revision/dirty
checkout rejection and environment isolation in addition to the existing suite.
The standalone target compiles on Windows and both bootstrap and clock
compatibility suites pass. Its Android, renderer, ISO and audio paths have not
been tested. All 20 primary CTest suites, including clock ownership/refusal tests,
pass; the primary Android ARM64 target also compiles with the shared helper.

PortableKit/PSPRecomp MIT notices are retained in
[THIRD_PARTY_LICENSE_PORTABLEKIT.txt](../../THIRD_PARTY_LICENSE_PORTABLEKIT.txt).
The unchanged external checkout retains notices for its dependencies. Shipping
SDL, FFmpeg, fonts or other optional assets requires their own applicable notices;
none of those binaries or assets is committed by this integration.
