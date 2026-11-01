# UMD wait continuation: bounded static checkpoint

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
