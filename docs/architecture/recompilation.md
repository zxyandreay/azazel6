# Recompilation and guest memory

**VERIFIED:** the input is an absolute-address ET_EXEC ELF with one RWX PT_LOAD,
no section table and no relocations found by the upstream loader. File offset
`0x1018` maps to guest `0x08804018`. Do not subtract a guessed constant everywhere:
the validator converts through checked segment ranges, refuses BSS/file confusion,
ambiguous mappings, overlap, overflow and non-file-backed entry instructions.

The framework's `required_ram_size()` selects 32 MiB to hold this executable and
BSS. This is a loader fit result, not a measurement of gameplay or mod requirements.
The probe reserves a 64 KiB experimental stack below `0x0A000000`, verifies it
does not overlap loaded segments, and starts SP at `0x09FFFFF0`. Module GP is read
from verified metadata, not guessed. Entry arguments begin at zero for this experiment.

**VERIFIED limitation:** full automatic generation classifies the entire RWX
segment as potentially executable, including data. Its 69,314 unsupported
instruction occurrences are not 69,314 demonstrated missing gameplay opcodes.
Overlapping functions and embedded data inflate those counts. The 1,015 generated
units are exploratory output; this milestone compiled only a 52-byte entry slice.

The slice ends immediately after the first JAL's delay slot. PSPRecomp emits its
C++; the host registers the code and discovered import boundaries. Two dispatches
execute the entry and reject the import. No hand-written substitute emulates the
entry. See [actual observations](../research/executable.md).

**PLANNED:** establish code/data ownership, repair import-aware generation for
sectionless input, validate function boundaries and expand AOT in bounded slices.
Trace dynamic allocations and code writes before choosing overlay windows or
memory sizes. Native-generated code can retain guest memory errors; do not bypass
range checks because a pointer looks plausible.

PSP cached/uncached aliases belong to framework memory semantics. The Python ELF
converter deliberately handles canonical ELF addresses only. Future mods need
generation/lifetime tokens and ownership checks in addition to bounds checking.
