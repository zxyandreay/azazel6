"""Apply exact-word, exact-site Tekken startup lowering to generated AOT code."""
import re
from elf import ValidationError


VI2UC_WORD = 0xD03C80E0


def lower_startup_vi2uc(elf, source, sites):
    if not isinstance(sites, list) or not 1 <= len(sites) <= 8:
        raise ValidationError("Invalid startup vi2uc site list")
    if len(set(sites)) != len(sites):
        raise ValidationError("Duplicate startup vi2uc site")
    replacements = []
    for value in sites:
        if not isinstance(value, str):
            raise ValidationError("Startup vi2uc site must be a hexadecimal string")
        try:
            site = int(value, 16)
        except ValueError as error:
            raise ValidationError("Invalid startup vi2uc site") from error
        if site & 3 or site > 0xFFFFFFF8 or site < 0:
            raise ValidationError("Invalid startup vi2uc address")
        offset = elf.guest_to_offset(site, 4)
        if int.from_bytes(elf.data[offset:offset + 4], "little") != VI2UC_WORD:
            raise ValidationError("Startup vi2uc word changed")
        label = f"L_{site:08X}"
        next_label = f"L_{site + 4:08X}"
        pattern = re.compile(
            rf"(?m)^{label}:\n[ \t]+rt\.unsupported\(0x{site:08X}u, "
            rf"0x{VI2UC_WORD:08X}u, \"vfpu4 not lowered yet\"\); return;\n")
        matches = list(pattern.finditer(source))
        if len(matches) != 1 or source.count(f"{next_label}:") != 1:
            raise ValidationError("Startup vi2uc generated form changed")
        replacement = (f"{label}:\n"
                       f"    azazel6::execute_startup_vi2uc_q(ctx);\n"
                       f"    goto {next_label};\n")
        replacements.append((matches[0].group(), replacement))
    for original, replacement in replacements:
        source = source.replace(original, replacement, 1)
    return '#include "azazel6/vfpu_vi2uc.hpp"\n' + source
