# UMD wait continuation: bounded static checkpoint

**VERIFIED static, 2026-10-29.** Following authorization for bounded analysis of
the existing local EBOOT, the exact ULUS10466 input was revalidated against SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`.
No native execution, interpreter execution, translation generation, media import
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
worker with that error, its next import is the currently unsupported delay
service. The order of activity in other runnable threads is not established by
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

**PLANNED:** model a bounded delay service using the existing shared guest
timeline and test it synthetically before any native continuation. This can
make the empty-drive retry behavior accurate; it cannot make a medium READY.

**BLOCKED media progression:** this worker requires a nonnegative drive-state
result to leave its retry loop, and one subsequent path performs file/device
operations. The empty-drive host supplies neither media nor readable files.
This establishes a media prerequisite for this worker, not proof that all game
threads are blocked or that no independent startup work remains.

Native execution remains outside the current analysis-only authorization.
Milestones 003 and 004 remain open; no new title, scene or gameplay result is
claimed. The next native observation, if separately authorized, must retain the
existing execution/time bounds and explicit failure boundaries.
