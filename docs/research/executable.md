# ULUS10466 executable and import investigation

**Question:** can the supplied decrypted executable be validated, analyzed and
partially executed through the chosen framework? **Date:** 2026-10-17.

The [Jin8 fingerprint](https://github.com/zxyandreay/tekken6-jin8/blob/dd3894fe72c09e67226f5482ca1ef5afa9c0d1a0/research/eboot/fingerprint.md)
provided the expected baseline. Independent local byte/hash validation confirmed:

| Property | VERIFIED value |
| --- | --- |
| Size | 4,158,480 bytes |
| SHA-256 | `e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f` |
| Format | ELF32, little-endian, MIPS, ET_EXEC |
| Entry guest VA | `0x08804124` |
| PT_LOAD file offset / guest VA | `0x1018` / `0x08804018` |
| File-backed / in-memory segment bytes | 4,154,024 / 4,583,468 |
| Segment flags | RWX (7), not proof every byte is code |
| Section headers | None |
| PSP module file offset / guest VA | `0x00386740` / `0x08B89740` |
| Module name / version / attributes | `tekken` / 1.1 / 0 |
| Module GP guest value | `0x08C022B0` |
| Import table guest range | `[0x08B894E4, 0x08B8973C)` |
| Framework RAM fit / relocations | 32 MiB / 0 |

## Analyzer limitations and profile correction

Run the commands in [building](../development/building.md). Upstream `psp_analyze`
found 10,072 function seeds/functions, 458,194 unique instruction labels, 111,908
block entries, 2,699 indirect-call sites, 69,314 unsupported instruction occurrences,
17,980 overlapping labels and zero truncated functions. These are heuristic counts,
not validated functions or coverage of all runtime code.

The upstream module finder requires a named section, so its raw report says
`module: null`, `import_count: 0`. Inspection of the first PT_LOAD `p_paddr` as a
file offset exposed a consistent module record. The independent profile reader
checks that structure, mapped table extents, descriptor lengths and strings. It
enumerates 268 imports across 30 libraries, without altering the raw upstream report.

| Import library | Count | Import library | Count |
| --- | ---: | --- | ---: |
| ThreadManForUser | 53 | sceSasCore | 27 |
| IoFileMgrForUser | 23 | sceUtility | 17 |
| scePsmfPlayer | 14 | sceAtrac3plus | 13 |
| sceRtc | 12 | sceAudio | 9 |
| sceNetAdhocctl | 9 | ModuleMgrForUser | 7 |
| sceNetApctl | 7 | UtilsForUser | 7 |
| sceGe_user | 6 | sceNetAdhoc | 6 |
| SysMemUserForUser | 6 | sceCtrl | 5 |
| sceDisplay | 5 | sceUmdUser | 5 |
| scePower | 5 | Kernel_Library | 5 |
| sceSuspendForUser | 5 | sceNet | 4 |
| sceNetInet | 3 | InterruptManager | 3 |
| StdioForUser | 3 | sceImpose | 2 |
| sceWlanDrv | 2 | sceMpeg | 2 |
| LoadExecForUser | 2 | sceOpenPSID | 1 |

Full automatic generation completed in 1,015 units, registering 1,038,506 block
addresses and emitting zero import wrappers because the upstream finder missed the
module. **Do not link this corpus as a trusted game build.** Sectionless import
support and code/data discrimination need to be carried into generation first.

## Actual native boundary

The manual generator translated 52 bytes from entry through the first JAL and its
delay slot. A Windows x64 MSVC build ran 13 real translated instructions with
fallback disabled, then dispatched to an explicitly rejecting import wrapper:

```text
entry=0x08804124 gp=0x08C022B0 ram=33554432 imports=268 initial_sp=0x09FFFFF0
dispatch=0x08804124 a0=0x00000000
dispatch=0x08B88DD0 a0=0x05050010
after_pc=0x08B88DD0 ra=0x08804158 sp=0x09FFFFD0
reason=Unimplemented PSP import SysMemUserForUser:0x91DE343C
```

The native process returns 3 for this research boundary. The bounded runner records
that separately from a crash or timeout. It does not call this a boot success.
The GP is discovered; the stack reservation and zero entry arguments are explicitly
experimental. The first import's semantics and module-start ABI remain unresolved.

Next: identify this library/NID from authoritative PSP interface sources, test its
observable return/state behavior and extend the verified entry range past return
PC `0x08804158`. Record each subsequent import/dispatch stop. Do not make every
unimplemented import return zero merely to advance execution.
