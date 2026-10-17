"""Strict, dependency-free ELF32 validation. Addresses are guest virtual values."""
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import re
import struct

HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM = struct.Struct("<IIIIIIII")
SECTION = struct.Struct("<IIIIIIIIII")
MAX_INPUT = 64 * 1024 * 1024


class ValidationError(ValueError):
    pass


def bounded(offset, size, limit, label):
    if offset < 0 or size < 0 or offset > limit or size > limit - offset:
        raise ValidationError(f"{label} lies outside its containing range")


@dataclass(frozen=True)
class Segment:
    type: int
    offset: int
    vaddr: int
    paddr: int
    filesz: int
    memsz: int
    flags: int
    align: int


@dataclass
class Elf:
    data: bytes
    entry: int
    segments: list[Segment]
    flags: int

    @classmethod
    def parse(cls, data):
        if len(data) < HEADER.size or len(data) > MAX_INPUT:
            raise ValidationError("ELF size outside supported bounds (52 bytes to 64 MiB)")
        ident, kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = HEADER.unpack_from(data)
        if ident[:7] != b"\x7fELF\x01\x01\x01":
            raise ValidationError("Expected decrypted ELF32 little-endian version 1 input")
        if kind != 2 or machine != 8 or version != 1:
            raise ValidationError("Profile supports ET_EXEC MIPS only; PRX/relocatable inputs need a separate profile")
        if ehsize != HEADER.size or phsize != PROGRAM.size or phnum == 0:
            raise ValidationError("Invalid ELF/program header dimensions")
        bounded(phoff, phsize * phnum, len(data), "Program headers")
        segments = [Segment(*PROGRAM.unpack_from(data, phoff + i * phsize)) for i in range(phnum)]
        loads = [s for s in segments if s.type == 1]
        if not loads:
            raise ValidationError("No PT_LOAD segments")
        for s in loads:
            bounded(s.offset, s.filesz, len(data), "PT_LOAD file bytes")
            bounded(s.vaddr, s.memsz, 1 << 32, "PT_LOAD guest address")
            if s.filesz > s.memsz:
                raise ValidationError("PT_LOAD filesz exceeds memsz")
            if s.align > 1 and (s.align & (s.align - 1) or s.vaddr % s.align != s.offset % s.align):
                raise ValidationError("Invalid PT_LOAD alignment")
        ordered = sorted(loads, key=lambda s: s.vaddr)
        for a, b in zip(ordered, ordered[1:]):
            if a.vaddr + a.memsz > b.vaddr:
                raise ValidationError("Overlapping PT_LOAD guest ranges")
        if entry % 4 or not any(s.flags & 1 and s.vaddr <= entry and entry + 4 <= s.vaddr + s.filesz for s in loads):
            raise ValidationError("Entry is not aligned file-backed executable code")
        if shnum:
            if shsize != SECTION.size or shstr >= shnum:
                raise ValidationError("Invalid section header dimensions/index")
            bounded(shoff, shsize * shnum, len(data), "Section headers")
            for i in range(shnum):
                section = SECTION.unpack_from(data, shoff + i * shsize)
                if section[1] != 8:  # SHT_NOBITS occupies no file bytes.
                    bounded(section[4], section[5], len(data), "Section contents")
        elif shoff or shstr:
            raise ValidationError("Extended/absent section table encoding is unsupported")
        return cls(data, entry, loads, flags)

    def guest_to_offset(self, address, size=1):
        bounded(address, size, 1 << 32, "Guest request")
        if size == 0:
            raise ValidationError("Empty address conversion")
        for segment in self.segments:
            delta = address - segment.vaddr
            if 0 <= delta < segment.filesz and size <= segment.filesz - delta:
                return segment.offset + delta
        raise ValidationError("Guest address is unmapped or BSS-only")

    def offset_to_guest(self, offset, size=1):
        bounded(offset, size, len(self.data), "File request")
        if size == 0:
            raise ValidationError("Empty offset conversion")
        matches = [s.vaddr + offset - s.offset for s in self.segments
                   if 0 <= offset - s.offset < s.filesz and size <= s.filesz - (offset - s.offset)]
        if len(matches) != 1:
            raise ValidationError("File offset is unmapped or ambiguous")
        return matches[0]

    def report(self):
        return {"format": "ELF32-LE-MIPS", "size": len(self.data),
                "sha256": hashlib.sha256(self.data).hexdigest(), "entry": f"0x{self.entry:08X}",
                "load_address": f"0x{min(s.vaddr for s in self.segments):08X}",
                "segments": [asdict(s) for s in self.segments],
                "limitations": ["No dynamic-overlay inference", "Function discovery belongs to PSPRecomp",
                                "PT_LOAD executable flags may also cover data"]}


def load_profile(path):
    try:
        profile = json.loads(Path(path).read_text(encoding="utf-8"))
        exe = profile["executable"]
        generation = profile["generation"]
        if (profile["schema_version"] != 1 or profile["game_id"] != "ULUS10466"
                or profile["region"] != "USA" or exe["format"] != "ELF32-LE-MIPS"
                or type(exe["size"]) is not int or not 52 <= exe["size"] <= MAX_INPUT
                or re.fullmatch(r"[0-9a-f]{64}", exe["sha256"]) is None
                or any(re.fullmatch(r"0x[0-9a-fA-F]{8}", exe[key]) is None for key in ["entry", "load_address"])
                or type(generation["unit_span"]) is not int or not 256 <= generation["unit_span"] <= 65536
                or generation["unit_span"] % 4 or generation["prefix"] != "tekken6"):
            raise ValidationError("Unsupported profile values")
        return profile
    except (KeyError, TypeError, json.JSONDecodeError) as error:
        raise ValidationError(f"Malformed profile: {error}") from error


def validate(path, profile):
    path = Path(path)
    if path.stat().st_size > MAX_INPUT:
        raise ValidationError("Input too large; provide the decrypted EBOOT, not the ISO")
    elf = Elf.parse(path.read_bytes())
    report = elf.report()
    for key in ["format", "size", "sha256", "entry", "load_address"]:
        if str(report[key]).lower() != str(profile["executable"][key]).lower():
            raise ValidationError(f"Unsupported game build: {key} mismatch (observed {report[key]})")
    return elf
