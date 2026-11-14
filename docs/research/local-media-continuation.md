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

## First transfer into game memory (2026-11-06)

**VERIFIED on the native host.** The requesting worker uses the same queue for
open, read and close. At `0x08AE4578` the file object selects slot zero in
the disc device table. Its open method resolves a six-entry mode switch and
calls `sceIoOpen` with read-only flags and creation permissions 0644. The ISO
provider permits these permissions on an existing file. The read method takes
the resulting handle and requests 120 bytes at `0x090B02A0`, inside a live
partition block. The probe transfers all 120 bytes from the ISO. The game
closes the handle, worker 6 exits and the main callback advances to
`0x08898554` on frame ten. Scanout remains black.

**VERIFIED synthetic contracts.** Each positive read must fit within one live
block of the calling runtime and the 8 MiB transfer cap. The provider stages
the read before copying it into guest RAM and advancing the handle's offset.
Tests exercise consecutive reads, short reads, EOF, zero length, closed
handles, freed blocks, overflow and a request crossing a block boundary.

**VERIFIED continuation.** The next main callback repeats the same checked
queue path for `DATA401.IDX`. The file's 26,028 bytes are transferred into a
second live partition buffer, bringing total ISO data read to 26,148 bytes.
The game enters its index parser and probes flat optional module names under
`ms0:/`, beginning with `MEMSTICK.IND` and `MSTK_PRO.IND`. Syntactically
bounded flat names receive PSP error `0x80020146`, which prohibits user-mode
module loading from that device. The game explicitly checks this result. No
module image is admitted or acknowledged.
The parser then creates an index worker at `0x08AA2ACC`; its two object calls
remain guarded while their concrete targets are observed.

The first index-worker run exhausted the former 32,768-dispatch cap while
making 1,311 bounded calls to the existing overlap-safe copy routine. The
worker remained runnable and was processing short record fields. This is
evidence for a larger diagnostic slice. A 131,072-dispatch follow-up remained
in the same worker after 13,226 copy calls and 6,614 record-list operations.
A 524,288 slice then exposed a stable poll of an unset initialization object.
The cause was the provisional file-not-found result for `sceKernelLoadModule`
on `ms0:`. Exact PSP and guest branch evidence require prohibited-device
instead. The native cap remains 524,288 for the corrected bounded rerun.

**VERIFIED raw-disc continuation.** A write watch showed the earlier `openPac`
worker store raw sector `0x38370` in the streaming object. The index worker
formats `disc0:/sce_lbn0x38370_size0x16e42800`. The provider now parses this
ISOFS pseudo-path with bounded hexadecimal fields and admits it only when the
entire extent remains inside the fingerprinted image. Malformed, zero-length,
overflowing and out-of-image extents reject. The game opens the range, seeks
to zero and reads its first 2,048 bytes into the live worker stack. Checked
seek contracts cover set/current/end origins, signed underflow, stale handles
and 64-bit return values. The first raw block advances execution to parser
helper `0x08B0FB08` with 28,196 total ISO bytes transferred.
The independent 15-second wall limit remains active, and either limit still
reports failure rather than partial success.

**VERIFIED, 2026-11-06.** The parser queries the absent Memory Stick savedata
directory. After receiving file-not-found it reaches the ordinary thread sleep
import at `0x08B88F60`. Saving the worker continuation and scheduling the main
thread allows the latter to advance to its next state. The address pair at
`0x0886267C/80` supplies callback `0x0886272C`, stored at object offset
`0x60` and observed at dispatch site `0x08860FBC`. The subsequent guard at
`0x08894518` receives `0x08894B00` (file-backed slot `0x08B947FC`).
That callback calls `0x08868270` directly with return PC `0x08894B4C`.
The 557-root native run stops there with 14 frames, 13 GE lists and a black
480×272 capture. The next object call at `0x08894B5C` has a terminal guard.

**VERIFIED object initialization.** The direct helper `0x08868270` returned
to the main thread. At `0x08894B5C`, the guest loaded `0x08895254` from
table `0x08B94698` at offset 8. The callback cleared three object fields.
The caller then constructed its second object through `0x0889D1D4` and
loaded `0x08895264` from `0x08B94650 + 8` at `0x08894BB8`. Each
dispatch checks its own file-backed pointer slot.

**VERIFIED state transition.** The guest constructs `0x088977D4` with the
address pair at `0x088987F4/F8` and stores it at object offset `0x68` at
`0x08898814`. The guarded dispatch at `0x08897ADC` entered this 13-word
list-unlink callback. The main thread then reached its direct call from
`0x088627F4` to `0x08894734`. The native run recorded 15 frames and 14 GE
lists. Scanout still contained zero nonblack pixels.

**VERIFIED readiness check.** The main thread entered `0x08894734` and
its three-word predicate `0x08868224`. The second conditional helper
(`0x0889CE24`) has static coverage only. The next stop was the tail call at
`0x08808314`. Its receiver held table `0x08B90C40` and register 25 held
`0x0886804C` after the load at table offset `0x18`.

**VERIFIED virtual method dispatch.** The constructor installed table
`0x08B90C40` at `0x08868370`. Allowing its slot at `0x08B90C58` for the
tail call advanced the main thread into the method. Its inner call at
`0x088680C4` received `0x08868124` in register 5 with a zero adjustment
in register 16. The constructor builds that address at `0x08868324/28` and
stores it at `0x0886832C`. The inner guard stopped before invoking it.

**VERIFIED callback execution.** With that address pair admitted only at
`0x088680C4`, the guest entered `0x08868124`, requested 128 bytes from
`0x0885FA44`, and passed the result to its direct callee `0x088AB8D0`.
The native probe stopped at the callee entry, before its first instruction.
The later member call at `0x08868178` is guarded.

**VERIFIED system parameter request.** The constructor entered `0x0889DF24`
and called `sceUtilityGetSystemParamInt` (NID `0xA5DA2406`) at
`0x0889DF5C`. The native trace shows ID 9 and a word output at the current
SP. The pinned PSPSDK identifies this ID as the confirm-button assignment.
The guest tests the service result before reading its stack word.

**VERIFIED object/list setup, 2026-11-12.** After controller configuration,
the constructor invokes `0x08887C70`. Its reviewed body allocates a 112-byte
object, installs its tables and initializes the two record containers through
`0x08887B5C`. The caller clears the counters with `0x089B7A0C` and tails into
`0x089B8CDC`. Its first direct callee, `0x089B8A68`, resets/swaps the container
fields using `0x08887C30`; the two possible destructor calls at `0x089B8B80`
and `0x089B8BDC` retain terminal guards. Neither call executes in this run.
The next uncovered helper is `0x089B8894`, return `0x089B8D18`.
Coverage is 577 roots / 36,767 words. The run still records 15 frames, 14 GE
lists and 28,196 ISO bytes, with black scanout. The wrapper reports failure
at uncovered code, not boot success. Python, integrated CTest and ARM64 checks
pass; title boot remains open.

**VERIFIED container allocation boundary.** The next five reviewed roots cover
the population helper, record erasure, bounded fill/clear loops and the two
container growth paths. Unknown allocation calls remain terminal. Native execution
enters `0x089B8EF0` with a request for 60 records and observes `0x088874E4`
at `0x089B8FC4`; register 6 holds table `0x08B938A0`, whose first file-backed
word contains that target. The constructor installs this table at `0x08887D1C`.
Coverage is 582 roots / 37,470 words. All 94 Python tests, 25 CTest suites and
the ARM64 build pass. The native run is a guarded failure with unchanged black
scanout, 15 frames and 14 GE lists; it has not entered the allocation callback.

**VERIFIED record allocation.** The exact table slot now authorizes only
`0x088874E4` at that site. The six-word callback scales the requested record
count by 76 and tails into the existing allocator. Reviewed copy/fill helpers
`0x089B8E70` and `0x089B8DF8` complete. The adjacent byte container then requests
60 elements at `0x089B95F4`, where the guard observes `0x0887DC78`. Its table
slot is `0x08B939E8`, installed at `0x08887D5C`. Coverage is 585 roots / 37,607
words; the full Python/CTest/ARM64 checks pass. Native execution still ends at
the guard with 15 black display frames, before the byte allocation callback.

**VERIFIED constructor return.** Slot `0x08B939E8` now authorizes that
byte-allocation call. Its body and the direct byte copy/fill helpers execute,
and the enclosing constructor returns to `0x08868168`. The next guard at
`0x08868178` observes `0x088685D4`, loaded from receiver table `0x08B90C88`
at offset 8. Coverage is 588 roots / 37,722 words. Python, integrated CTest
and ARM64 validation pass. The native checkpoint still has 15 frames, 14 GE
lists and zero nonblack pixels; no title or input result is claimed.

**VERIFIED member reset.** The parent constructor installs table `0x08B90C88`
at object offset `0x78` with the store at `0x08868398`. Its slot at
`0x08B90C90` now authorizes the observed four-word reset method at
`0x08868178`. The method returns and startup reaches shared tail dispatch
`0x0885F16C`, with receiver table `0x08B95320` and target `0x0889CE0C`.
Coverage is 589 roots / 37,767 words; all Python, CTest and ARM64 checks pass.
The run remains a guarded failure with 15 black display frames.

**VERIFIED next main state.** Constructor store `0x0889D2CC` installs table
`0x08B95320`. Its status slot at `0x08B95334` authorizes the observed two-word
method `0x0889CE0C` for `0x0885F16C`. The alternate branch at `0x0885F144`
has checked slot `0x08B95338` and reviewed method `0x0889D0C0`; its internal
member calls remain guarded. The native run advances to frame 16 and 15 GE
lists, then the main dispatch at `0x08860FBC` observes `0x08862D40` in register
5. Coverage is 591 roots / 37,818 words. Python/CTest/ARM64 checks pass;
captured scanout remains black and the unknown callback still stops execution.

**VERIFIED geometry preparation.** The address pair `0x08862808/0C` and store
at `0x08862824` establish the next main callback. Its reviewed direct helpers
include startup state setters and geometry-buffer setup. The shared body at
`0x089C94FC` has one explicit root to avoid overlapping its two callers.
Native execution enters `0x08862D40`, completes the geometry preparation and
reaches resource helper `0x08ADF158`, return `0x089C97EC`. Cache diagnostics
increase to 29 ranges, including 20 writebacks. Later state setters have static
coverage only. Coverage is 602 roots / 38,134 words; full Python, integrated
CTest and ARM64 checks pass. Sixteen frames and 15 GE lists still produce black
scanout. Resource allocation and visible title boot remain unverified.

**VERIFIED resource query boundary.** The reviewed wrapper `0x08ADF158`
reorders its arguments and tails into `0x08AED9D0`. The latter obtains the
existing global resource object through `0x08ACF110`. Its first virtual call
at `0x08AEDA14` observes `0x08ACF924` from table `0x08BCFC88 + 0x38`, with
resource identifier `0x20002000`. The call remains terminal pending target
review. Coverage is 604 roots / 38,171 words. Python/CTest/ARM64 checks pass;
the real-media run still records 16 black frames and 15 GE lists.

**VERIFIED resource lookup, 2026-11-14.** The existing reviewed method
`0x08ACF924` was admitted only through file-backed table slot `0x08BCFCC0` at
`0x08AEDA14`. The game executes the query and continues to `0x08AEDA70`, where
the guard observes `0x08ACF8C4` for the next resource operation. Its receiver
still holds table `0x08BCFC88`; the next callback remains unapproved. The
regenerated slice covers 604 roots / 38,194 words. All 94 Python tests, 25
integrated CTest suites and ARM64 compilation pass. The 16-frame capture remains
uniformly black, so title boot is still open.

**VERIFIED resource operation, 2026-11-14.** The same file-backed resource
table's slot `0x08BCFC9C` admits `0x08ACF8C4` at `0x08AEDA70`. Its reviewed
tail enters the existing allocation path. Azazel6 then reaches an uncovered
direct helper at `0x08AEB858`, return `0x08AEB674`. The next virtual call
inside the resource path remains guarded. Coverage is 605 roots / 38,203
words. All 94 Python tests, 25 CTest suites and ARM64 compilation pass; the
real-media capture remains black at 16 frames and 15 GE lists.
