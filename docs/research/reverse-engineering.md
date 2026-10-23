# Local Allegrex evidence workflow

**VERIFIED, 2026-10-23:** native Windows Ghidra 12.1.3 with ghidra-allegrex
v21.4 imports the unchanged, exact-hash sectionless ULUS10466 ELF using
`PSP Executable (ELF)` / `Allegrex:LE:32:default:default`. Headless export of
`0x08AC3DD0` and its four setter methods recovered 45 instructions and
pseudocode. Every exported instruction's address and bytes matched the ELF.
This is static analysis, not execution. The decompiler's unresolved-tail warning
at `0x08AC3E30` is retained, not treated as a resolved jump table.

## Tools and compatibility

The local host has Python 3.14, Node 24.14.1, Java 17 on PATH and an existing
Android Studio Java 21.0.10 runtime. No PSP compiler was found on PATH. WSL is
available but has no installed distribution. No system installation was made.
The optional tools and databases live under ignored `local/`.

Official archives were downloaded and checked against GitHub release SHA-256:

| Tool | Official release | Archive SHA-256 |
| --- | --- | --- |
| Ghidra | [12.1.3](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.3_build) | `93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54` |
| Allegrex | [v21.4](https://github.com/kotcrab/ghidra-allegrex/releases/tag/v21.4) | `62e829fa9e6ed6f2bd343763fb339a3f8812b02a4e0ed5a9409b14007b408243` |

Ghidra 12.2 was available, but the published extension targets 12.1.3; do not
silently pair incompatible versions. Extract the extension into the selected
distribution's `Ghidra/Extensions/ghidra-allegrex`. Its `extension.properties`
version is the Ghidra version. Ghidra and Allegrex retain their upstream licenses;
no implementation or distribution files are committed here.

**VERIFIED input / DOCUMENTED REA refusal:** at
[REA revision bc411c2](https://github.com/morluto/rea/blob/bc411c24befceced5a9ea1ca1c1ce123fd02bf48/src/ghidra/GhidraPspProfile.ts),
the PSP provider requires an inspected separate ABI-flags record in addition to
`e_flags=0x10A23001`. This input has the matching flags, one PT_LOAD segment,
no section table and no declared PT_MIPS_ABIFLAGS/SHT_MIPS_ABIFLAGS record.
The [provider documentation](https://github.com/morluto/rea/blob/bc411c24befceced5a9ea1ca1c1ce123fd02bf48/docs/ghidra-psp.md)
also requires Linux x64. The wrapper reports these failed necessary conditions;
REA itself was not installed or executed. Even adding WSL would not supply the
missing producer metadata. Do not patch the ELF or weaken REA admission.

## Reusable address investigation

Start with the newest native stop, not an old checkpoint. The Python path needs
no Ghidra installation and reuses the profile, import inventory, indirect maps
and conservative `inspect_root` CFG:

```sh
python tools/ghidra_probe.py local/game/EBOOT.BIN --root 0x08AC3DD0 --root 0x08AC3CD8 --output local/re-matrix
```

Add `--ghidra <installation> --java-home <Java-21-home>` for bounded headless
disassembly and decompilation. Up to 16 unique aligned guest addresses are
accepted; output must be a fresh directory inside this checkout's `local/`.
The wrapper checks the executable before creating output or starting Java,
checks Ghidra's image hash/language and verifies every exported instruction byte.
Extension file hashes are recorded. Unknown CFG transfers remain blocked.

Ghidra rejects `!` in installation/project paths. For a checkout with that
character, use a temporary unused Windows drive alias to the **same checkout**:

```powershell
subst Q: (Get-Location).Path
python Q:/tools/ghidra_probe.py Q:/local/game/EBOOT.BIN --root 0x08AC3DD0 --output Q:/local/re-matrix-ghidra --ghidra Q:/local/re-tools/ghidra_12.1.3_PUBLIC --java-home '<Java-21-home>'
subst Q: /D
```

Choose an unused drive, preserve the alias in all supplied paths, and remove it
after analysis. The wrapper invokes the distribution's Java headless entry point
with its own VM properties; it avoids batch expansion and redirects settings,
caches and temporary files into the output. The tested run needed normal host
execution permissions for the alias. Reference checkouts remain read-only.

`evidence.json` combines CFG roots, direct call candidates, reviewed/trapped
indirect sites, import libraries/NIDs and optional Ghidra observations. The latter
include function ranges, instruction/flow/delay context, incoming references,
outgoing code/data references and pseudocode/errors. References are limited to
256 per direction/site, instructions to 4096 per function, pseudocode to 64 KiB,
decompilation to 15 seconds per function and the Java process to 240 seconds.
No global auto-analysis runs; incoming references are incomplete, computed data
references/types and constant propagation remain advisory, and functions may
overlap or be misidentified. Never infer runtime memory ownership from these.

Keep raw output local. Review indirect provenance and delay slots against bytes
and native register/ownership observations before changing `startup.json`.
Regenerate, build and run the bounded native probe to establish actual progress.
Allegrex VFPU prefix effects are not fully modeled by the extension: validate
them independently in the pinned decoder/runtime and synthetic tests. This
workflow never translates pseudocode into host code or authorizes roots.
