"""ULUS10466 sectionless module metadata, using the first PT_LOAD p_paddr file offset.

This is a profile convention verified against this hash, not a generic ELF rule.
"""
import struct
from elf import ValidationError


def module_report(elf):
    offset = elf.segments[0].paddr
    address = elf.offset_to_guest(offset, 52)
    attributes, minor, major, raw_name, gp, ent_top, ent_end, stub_top, stub_end = struct.unpack_from("<HBB28sIIIII", elf.data, offset)
    name = raw_name.split(b"\0", 1)[0]
    if b"\0" not in raw_name or not name or any(c < 32 or c > 126 for c in name):
        raise ValidationError("Invalid PSP module name")
    if not any(s.vaddr <= gp < s.vaddr + s.memsz for s in elf.segments):
        raise ValidationError("Module GP outside loaded memory")
    for start, end in [(ent_top, ent_end), (stub_top, stub_end)]:
        if end < start or start % 4 or end % 4:
            raise ValidationError("Invalid module table range")
        if end > start:
            elf.guest_to_offset(start, end - start)
    exports = []
    start_thread = None
    cursor = ent_top
    while cursor < ent_end:
        if ent_end - cursor < 16:
            raise ValidationError("Truncated export descriptor")
        off = elf.guest_to_offset(cursor, 16)
        lib, version, flags, words, variables, functions, table = struct.unpack_from("<IHHBBHI", elf.data, off)
        if words != 4 or words * 4 > ent_end - cursor:
            raise ValidationError("Unsupported export descriptor size")
        count = variables + functions
        if count:
            table_off = elf.guest_to_offset(table, count * 8)
            for i in range(count):
                nid, target = (struct.unpack_from("<I", elf.data, table_off + index * 4)[0]
                               for index in (i, count + i))
                exports.append({"nid": f"0x{nid:08X}", "address": f"0x{target:08X}",
                                "kind": "function" if i < functions else "variable"})
                if lib == 0 and flags & 0x8000 and nid == 0x0F7C276C:
                    if i < functions or start_thread is not None:
                        raise ValidationError("Invalid/duplicate module start thread export")
                    params = struct.unpack_from("<IIII", elf.data, elf.guest_to_offset(target, 16))
                    if params[0] != 3:
                        raise ValidationError("Unsupported module start thread parameters")
                    start_thread = dict(zip(("priority", "stack_size", "attributes"), params[1:]))
        cursor += words * 4
    imports = []
    cursor = stub_top
    while cursor < stub_end:
        if stub_end - cursor < 20:
            raise ValidationError("Truncated import descriptor")
        off = elf.guest_to_offset(cursor, 20)
        lib, version, flags, words, variables, count, nids, stubs = struct.unpack_from("<IHHBBHII", elf.data, off)
        if words < 5 or words * 4 > stub_end - cursor:
            raise ValidationError("Invalid import descriptor size")
        if variables:
            raise ValidationError("Variable imports require explicit implementation")
        library_bytes = bytearray()
        for i in range(128):
            byte = elf.data[elf.guest_to_offset(lib + i)]
            if byte == 0:
                break
            if not 32 <= byte <= 126:
                raise ValidationError("Non-ASCII import library")
            library_bytes.append(byte)
        else:
            raise ValidationError("Unterminated import library")
        if not library_bytes:
            raise ValidationError("Empty import library")
        if count:
            nid_off = elf.guest_to_offset(nids, count * 4)
            elf.guest_to_offset(stubs, count * 8)
            for i in range(count):
                imports.append({"library": library_bytes.decode("ascii"),
                                "nid": f"0x{struct.unpack_from('<I', elf.data, nid_off + 4 * i)[0]:08X}",
                                "stub": f"0x{stubs + 8 * i:08X}"})
        cursor += words * 4
    return {"name": name.decode("ascii"), "address": f"0x{address:08X}",
            "file_offset": f"0x{offset:08X}", "gp": f"0x{gp:08X}", "attributes": attributes,
            "version": f"{major}.{minor}", "stub_top": f"0x{stub_top:08X}",
            "stub_end": f"0x{stub_end:08X}", "imports": imports,
            "exports": exports, "start_thread": start_thread}
