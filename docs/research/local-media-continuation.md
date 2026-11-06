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

## First file metadata request (2026-11-06)

**VERIFIED on the native host.** After worker 5 exits, the main dispatcher
at `0x08897ADC` selects `0x08898710`. Its object callback starts at
`0x088975F4`, installs `0x08898AB8` and launches priority-32 worker 6
through `0x0889B254`. The worker queues a metadata request at `0x08AE34C8`.
The priority-19 I/O thread wakes, takes the descriptor's handler
`0x08AE1BF4` and selects the device method from slot `0x08BEFBB4`.
The 521-root native run reaches the real `sceIoGetstat` import at
`0x08B88CD0` with an 88-byte result on the I/O thread's stack.

**DOCUMENTED ABI and profile policy.** The PSP SDK defines the 88-byte
`SceIoStat`. The profile reports the ISO entry's type, size, read/execute
permissions and first sector in private word zero. Like the reference, it
preserves the caller's remaining five private words. All three dates derive
from the directory entry's recording time. The research profile expresses
them in UTC using the recorded signed offset, preserving an unspecified date
as zero. The query advances no guest time. See
[reference stat implementation](https://github.com/hrydgard/ppsspp/blob/cf17b0ac7966ba4b172a32340333d209a2d747e7/Core/HLE/sceIo.cpp)
and [ISO metadata](https://github.com/hrydgard/ppsspp/blob/cf17b0ac7966ba4b172a32340333d209a2d747e7/Core/FileSystems/ISOFileSystem.cpp).

**VERIFIED on the native host.** With that contract, the query succeeds once
and the worker selects completion callback `0x08AE0550` at `0x08AE1C28`.
The address is assembled at `0x08AE0E6C/78`, passed to the enqueue function
and stored at descriptor offset 28. The two intervening instructions copy
arguments without writing the address register. The callback stores the
request result and signals its semaphore. Unknown worker callbacks retain
terminal guards.

**VERIFIED checkpoint.** With 522 roots and 33,292 words the completion callback
returns, the requesting worker receives its result and reaches `0x08AE0670`
from `0x0889A16C`. The probe reports uncovered code as failure. It records
one successful metadata query, zero open handles, nine frames, eight GE lists
and zero nonblack pixels. All 93 Python cases and 25 CTest suites pass. The
same generated slice cross-compiles and links for Android ARM64.
