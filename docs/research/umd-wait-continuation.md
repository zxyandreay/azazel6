# UMD wait continuation: bounded static checkpoint

**Superseded dependency, 2026-11-05:** the local computer supplies a validated ISO
and a checked read-only provider. The media worker completes its actual sector
query and exits; startup reaches a later guarded callback. See
[local media continuation](local-media-continuation.md). The empty-drive results
below remain reproducible when no image is explicitly supplied.

**VERIFIED static, 2026-10-29.** Following authorization for bounded analysis of
the existing local EBOOT, the exact ULUS10466 input was revalidated against SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
During that static checkpoint, no native execution, interpreter execution, translation generation, media import
or AOT authorization was performed. Raw reports and bounded instruction listings
remain in ignored `local/root-inspection/umd-continuation/`.

## Question and procedure

Does the new empty-drive timeout allow the previously stopped worker to progress
beyond the UMD READY gate?

The existing inspector was run with a 512-instruction budget on the already
reviewed worker root `0x08AA2A34`, then on its direct candidate helper
`0x08AA17D0`. Each invocation revalidates the profile hash. The pinned Yakumo
`dump_function` utility supplied bounded listings for branch/delay-slot review;
module metadata supplied import library/NID identities. The external framework
remains at `39ac05f1a166703430d18826bc5877f4feb6b511`.

```sh
python tools/inspect_root.py local/game/EBOOT.BIN --root 0x08AA2A34 --inspect-existing-root --budget 512 --output local/root-inspection/umd-continuation
python tools/inspect_root.py local/game/EBOOT.BIN --root 0x08AA17D0 --budget 512 --output local/root-inspection/umd-continuation
python tools/analyze.py local/game/EBOOT.BIN --output local/root-inspection/umd-continuation/metadata
```

## Results

The worker CFG contains 38 instruction words. After `sceUmdWaitDriveStatCB`
returns at `0x08AA2A78`, a signed-negative branch returns to the call at
`0x08AA2A64`. Its delay slot supplies 96,000 microseconds to import stub
`0x08B88E08`, `ThreadManForUser::0xCEADEB47` (`sceKernelDelayThread`, also named
in the pinned framework's NID registry). The worker subsequently requests READY
with another 96,000-microsecond timeout.

The modeled timeout error `0x800201A8` is signed-negative, so it follows this
retry path. **VERIFIED static:** returning timeout does not let this worker pass
the READY gate. **CONDITIONAL prediction:** if native execution resumes this
worker with that error, its next import is the delay service (now modeled for
this duration with synthetic verification). The order of activity in other runnable threads is not established by
this inspection.

A nonnegative wait result instead checks a field at offset 8 in the worker's
argument object. A zero field leads to helper `0x08AA17D0`. This candidate has
40 discovered instruction words and is not in the authorized startup root set.
It calls an existing formatting helper, then `sceIoOpen` with flags 1 and mode 0.
After a nonnegative file descriptor, it invokes `IoFileMgrForUser::0x63632449`
with command `0x01020006`, no input buffer, and a four-byte stack output, then
calls `sceIoClose`. The exact input confirms these import stubs and arguments;
the device command's semantics and actual runtime result remain unverified.

The helper's static CFG discovery does not authorize its AOT registration.
Neither the success path nor the retry path was executed during this analysis.

## Consequence and next evidence

**VERIFIED synthetic:** the observed 96,000-us `sceKernelDelayThread` call now
suspends its caller on the existing guest timeline and returns zero at the modeled
deadline. Other durations stop pending separate evidence. Interrupt-context or
interrupt-disabled calls reject without scheduling a wait.

**DOCUMENTED reference policy:** PPSSPP revision
`cf17b0ac7966ba4b172a32340333d209a2d747e7`,
[`Core/HLE/sceKernelThread.cpp`](https://github.com/hrydgard/ppsspp/blob/cf17b0ac7966ba4b172a32340333d209a2d747e7/Core/HLE/sceKernelThread.cpp),
adds 10 us for delays above its short-delay threshold and uses a non-callback
wait. The narrow profile models a 96,010-us deadline accordingly. It does not
model the reference's CPU cycle cost, short-delay behavior or full scheduler,
and makes no hardware timing claim. Synthetic tests verify context restoration,
five intervening LCD edges plus the final deadline event, and invalid-context
rejection. The modeled delay cannot make a medium READY.

**BLOCKED media progression:** this worker requires a nonnegative drive-state
result to leave its retry loop, and one subsequent path performs file/device
operations. The empty-drive host supplies neither media nor readable files.
This establishes a media prerequisite for this worker, not proof that all game
threads are blocked or that no independent startup work remains.

## Native follow-up (2026-11-01)

**VERIFIED native:** expanded user authorization superseded the analysis-only
restriction. The same exact input was validated, regenerated and built under
`local/build/cloud/`, using the unchanged pinned framework, Debug configuration
and two build jobs. The existing 15-second runner timeout, 32,768-dispatch limit
and 64-idle-event budget were retained.

The first run reached a guarded callback at `0x08896EA4`. Reviewed immediate and
vtable provenance admitted three roots; the next run stopped at missing
`sceKernelGetThreadExitStatus`. Implementing that state query allowed the main
thread to continue polling while the media worker remained waiting. See
[callback and status-query evidence](startup-indirect-2026-10-22.md#worker-status-polling-and-native-timeouts-2026-11-01).

The final trace contains four READY-wait calls, four 96,000-us delay calls and
43 thread-exit-status calls. Call counts do not imply that the final delay has
completed. It reaches 51 display frames, 50 Complete GE lists, 58 idle events and
850,850 modeled guest microseconds. It stops at `0x08867528` on the dispatch
limit, with runner status `failed` / exit 1. This is budget exhaustion, not a
successful boot or a successful research-boundary exit. The two earlier VRAM
clears remain the only verified drawing; title boot and Milestones 003/004 remain
open. Readable media is still a prerequisite for this worker's progress beyond its
retry loop; the observation does not establish that all independent work is blocked.

Reproduce with the generated local probe:

```sh
python tools/run_probe.py local/game/EBOOT.BIN --binary local/build/cloud/probe/profiles/tekken6/azazel6_entry_probe --output local/build/cloud/run-2026-10-30-exit-status
```

Raw traces and generated translations remain ignored local artifacts. No disc,
asset, host filesystem bridge or fabricated READY result was introduced.

## Main-thread dependency and actual scanout (2026-11-01)

**VERIFIED static:** a fresh bounded review of the existing callback
`0x08896EA4` and helper `0x08AA2008` explains the main-thread dependency. The
helper reads the worker UID at `0x08C2B9C8`. A negative exit-status result branches
at `0x08AA2034` without deleting or clearing that UID; the helper returns one
only when the UID is zero. At `0x08896EC4`, a zero result skips the callback's
state-transition writes, including replacement of the callback at object offset
4. The later object-state branch has not been admitted or executed by this work.
This review does not claim that every possible startup state requires media.

**VERIFIED native:** every one of the 43 exit-status queries in the repeated
bounded run has argument UID 5, the waiting media worker. The main display loop
continues, but this callback cannot transition through worker completion while
that worker remains in its empty-drive retry loop. More display frames alone do
not resolve this dependency.

The probe now supports an optional diagnostic capture of current scanout after
the bounded run. The checked display readback preserves guest state, omits stride
padding and produces tightly packed RGBA; the runner retains it only under
ignored `local/`, writes an RGB PPM preview and records dimensions, checksum and
nonblack-pixel count separately from execution status. Old captures are removed
before a requested run. Timeout, missing output and malformed output cannot be
reported as a fresh successful capture. Capture does not establish title boot.

```sh
python tools/run_probe.py local/game/EBOOT.BIN --binary local/build/cloud/probe/profiles/tekken6/azazel6_entry_probe --capture-framebuffer --output local/build/cloud/run-2026-11-01-capture
```

The exact native run again stops at the same 32,768-dispatch budget after 51
display frames and 50 Complete lists. Current scanout is `0x04044000`, stride
512, format 1 (5551). Its 522,240-byte 480x272 RGBA capture has **zero nonblack
pixels**, SHA-256 `e6b5e274ce500e2a412af9e9277cc71db8586dbe4f167260a53b0efb898aa56e`.
This verifies black current scanout; it does not establish visible menu graphics
or the content of every earlier frame. Raw images remain local.

**BLOCKED title/gameplay continuation:** the available local game-input directory
contains only the exact EBOOT. It supplies no disc filesystem, textures or other
assets, and the current I/O host has no readable-media implementation. The
executable cannot substitute for the disc contents. Fabricating READY or worker
completion would bypass the observed dependency without providing those files.
The concrete next work is an explicitly selected, checked read-only local media
source, validated against the expected title/region, followed by the observed
file/device requests and their AOT dependencies. No input acquisition, media
mount or arbitrary host-directory exposure was performed. Broader renderer and
input implementation remains possible with synthetic contracts, but cannot
verify this game's title/gameplay gate without the missing game data. Physical
Android validation independently requires attached hardware.

Validation: 91 Python cases, 24 CTest suites and the repository audit pass. The
new eight runner cases are synthetic only, covering capture on failed runs,
black pixels, stale files, timeouts, malformed size, escaping symlinks, default
behavior and validation before side effects.
