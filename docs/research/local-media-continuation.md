# Checked local media continuation

**VERIFIED native, 2026-11-05:** the local checkout was fast-forwarded from
`b95b656` to cloud checkpoint `231f9fd` without applying the preserved stash or
older backup. The local decrypted executable matches the existing profile hash.
The explicitly selected 855,638,016-byte ISO matches the independently pinned
media fingerprint, identifies ULUS10466 version 1.00, and contains 68 checked
directory/file entries. Game inputs and generated evidence remain local.

## Baseline and implementation

The unchanged cloud checkpoint reproduced on Windows: 32,768 dispatches, 51
frames, 58 idle events and 850,850 modeled guest microseconds, stopping at
`0x08867528`. Scanout is entirely black and matches the cloud capture hash.
This is a failed budget-limited run, not boot success.

[ADR-0005](../decisions/ADR-0005-read-only-local-media.md) selects an original
profile-owned read-only ISO provider. Native admission checks the opened stream's
SHA-256, volume geometry, bounded single-extent records and SFO identity.
The exact disc uses file-structure version 2 and mixed-case identifiers;
normalization rejects collisions and does not relax traversal or extent checks.

With actual validated media, the observed readable-state wait returns and reaches
helper `0x08AA17D0`. Exact-input review finds 40 words and four direct calls,
all to already covered code or known imports. Adding this single root yields
505 roots and 32,309 words. The helper opens a real file, obtains its first
sector through `sceIoIoctl(0x01020006)`, and closes its handle.

**DOCUMENTED reference:** the command returns the file's starting LBA in pinned
PortableKit `33800c4060c53f3233e08f8ab2cc4590a0872063`,
[`host/hle/hle_io.cpp`](https://github.com/TeamGDB/PortableKit/blob/33800c4060c53f3233e08f8ab2cc4590a0872063/host/hle/hle_io.cpp).
Azazel6 implements that narrow checked contract independently and retains
terminal failures for unsupported commands; it does not import fallback-success
behavior or PortableKit's runtime.

## Observed result

The media worker returns and its UID 5 is deleted by the existing startup flow.
The native run records one file open, one sector query and zero remaining file
handles. It reaches 9 display frames and 150,150 modeled guest microseconds,
then stops at guarded callback target `0x08898710` from site `0x08897ADC`.
The two VRAM clears remain the only drawing, and scanout remains black.
This removes the demonstrated empty-drive dependency; it does not establish title
boot, general file reading, rendering, audio, input or completed milestones.

**Next investigation:** review the new callback's source and bounded indirect
calls before authorizing coverage. The first static inspection rejects indirect
site `0x0889877C`; preserve that guard until target provenance is established.

Reproduce with freshly generated probe output and the explicitly selected local
image (replace `local/game/tekken6.iso` with its existing private location):

```sh
python tools/run_probe.py local/game/EBOOT.BIN --binary local/build/resume/profiles/tekken6/Release/azazel6_entry_probe.exe --iso local/game/tekken6.iso --capture-framebuffer --output local/media-run
```

Omitting `--iso` preserves the empty-drive baseline. Both executable and image
fingerprints are revalidated; generation and raw traces remain under `local/`.

The Python runner also has a portable Windows junction fixture for path-escape
testing: the original symlink fixture required a privilege absent on this machine.
The check still exercises actual filesystem resolution and refusal before output.

**VERIFIED validation, 2026-11-06:** all 93 Python cases and 25 integrated CTest
suites pass; the updated probe and media provider cross-compile and link for
Android ARM64. These are host/synthetic and compilation checks, not Android game
execution. No physical device is attached at this checkpoint.
