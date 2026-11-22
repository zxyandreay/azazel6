# First exact-input PortableKit hybrid AOT checkpoint

**2026-11-21, Windows host only.** The user supplied the exact USA ISO in the
repository root. It and the decrypted EBOOT remain ignored; all generated code,
logs, captures and binaries are in `local/`. This checkpoint does not include an
Android game APK or a Tekken title screen.

## Question and method

Can one reviewed Tekken function execute as native host code in the same
PortableKit runtime that interprets the rest of startup and drives ISO, PSP
kernel, FFmpeg and Vulkan services?

**VERIFIED input:** the EBOOT is 4,158,480 bytes, SHA-256
`e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f`;
the ISO is 855,638,016 bytes, SHA-256
`45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8`.
The runner and native host both checked the ISO before the execution grant.
PortableKit remained at `33800c4060c53f3233e08f8ab2cc4590a0872063`,
unmodified.

The new generator validates the executable, then rechecks the entry CFG at
`0x08804124`: one contiguous 276-byte function, 69 instruction words, 56
dispatchable control PCs and 13 delay slots. It asks pinned `psp_recomp` to
compile only that function. The resulting 160,563-byte local C++ file has no
unsupported instruction site; its only `unsupported` call guards an invalid
internal entry. It registers that function's 56 control PCs and 268 exact
import wrappers. The exploratory 1,015-unit corpus is not linked.

The profile registers the generated corpus after exact ELF/module/import
validation, then installs PortableKit's existing system layer in the **same**
runtime. A profile wrapper at `0x08804124` increments a counter immediately
before invoking the generated `Tekken6Entry` function. It does not execute in
the interpreter or in an import wrapper. The interpreter count comes from the
same runtime after its bounded run. No guest state is copied into a second
runtime.

## Results

**VERIFIED Windows native AOT + interpreter fallback:** the exact-ISO,
100,000-dispatch headless run recorded `native_aot_entry_calls=1` and
`interpreter_instructions=73,261,212`. The first interpreter miss was
`0x08804238`, immediately after the generated entry span. Strict HLE reported
268 imports, 539 implemented handlers and **zero logging stubs**. The run
stopped at its dispatch limit, with 188 VBLANKs. These counters prove one
native Tekken function invocation and subsequent interpreter work; they are
not a game boot or performance comparison.

**VERIFIED Windows game Vulkan output:** a second bounded exact-ISO hybrid run
recorded the same AOT/interpreter counts and captured the 960×544 Bandai Namco
Games opening logo. Its 522,240 nonblack pixels and local BMP SHA-256
`0f268c2a92c873dc6b1c4d0450b0ae0264b2c6bec336bdc85c59fb3a35fc69a1`
match the prior interpreter-only logo capture. This is actual game rendering
through PortableKit, not a Tekken title or menu.

**VERIFIED tests:** 103 Python tests pass, including exact-input refusal and
counter parsing failures. The four optional PortableKit compatibility CTest
suites pass. The Vulkan build's FFmpeg 7.1.5 `avcodec-61.dll`, `avutil-59.dll`
and `swresample-5.dll` exactly match the pinned extracted copies by SHA-256.
Windows CTest needs that build's `bin` directory on `PATH`; without it, Windows
can wait on a missing-DLL dialog. `avcodec-59.dll` is not part of this pinned
FFmpeg ABI.

## Limits and next experiment

**BLOCKED for title progression:** the longer interpreter comparison stopped
at `scePsmfPlayerGetCurrentPts` (`0x3ED62233`). ATRAC ID-pool and GE completion
semantics also need review. This AOT slice did not change those services.

**VERIFIED 2026-11-22:** the shared hybrid host, exact media admission,
FFmpeg and Vulkan stack were packaged into a signed ARM64 APK. See the
[Android checkpoint](hybrid-android-apk-2026-11-22.md) for translated-emulator
runtime evidence and the remaining physical-device/title limits.

To reproduce generation and the Windows run, use the commands in
[building](../development/building.md#optional-windows-hybrid-aot-checkpoint).
Every generated source and run output needs a fresh ignored `local/` directory.
