"""Validate profile-declared constructor tables and guard generated JALR sites."""
from elf import ValidationError


def constructor_maps(elf, descriptors):
    result = {}

    def read(address):
        offset = elf.guest_to_offset(address, 4)
        return int.from_bytes(elf.data[offset:offset + 4], 'little')

    for descriptor in descriptors:
        site = int(descriptor['site'], 16)
        end = int(descriptor['table_end'], 16)
        register, limit = descriptor['register'], descriptor['max_entries']
        if not 1 <= register <= 31 or not 1 <= limit <= 1024 or site % 4 or end % 4:
            raise ValidationError('Invalid constructor call/table descriptor')
        if site in result or read(site) != (register << 21) | (31 << 11) | 9:
            raise ValidationError('Duplicate or mismatched constructor JALR site')
        targets = []
        for index in range(limit):
            address = end - 4 * (index + 1)
            target = read(address)
            if target == 0xFFFFFFFF:
                break
            if target % 4 or not any(s.flags & 1 and s.vaddr <= target and target + 4 <= s.vaddr + s.filesz
                                     for s in elf.segments):
                raise ValidationError('Constructor target outside executable file-backed range')
            targets.append(target)
        else:
            raise ValidationError('Constructor table has no sentinel within budget')
        if not targets:
            raise ValidationError('Empty constructor target map')
        result[site] = {'register': register, 'targets': targets, 'table_start': address, 'table_end': end}
    return result


def guard_generated(source, maps):
    for site, mapping in maps.items():
        label = f'L_{site:08X}:\n'
        if source.count(label) != 1:
            raise ValidationError('Generated indirect site label missing or duplicated')
        targets = ', '.join(f'0x{target:08X}u' for target in sorted(set(mapping['targets'])))
        guard = (f'    if (!azazel6::guard_indirect_call(rt, 0x{site:08X}u, '
                 f'ctx.gpr[{mapping["register"]}], {{{targets}}})) return;\n')
        source = source.replace(label, label + guard)
    return '#include "azazel6/control_flow.hpp"\n' + source if maps else source
