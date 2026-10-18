"""Validate profile-declared indirect tables and guard generated transfers."""
from elf import ValidationError


def pointer_maps(elf, descriptors, *, tail=False):
    """Resolve explicit file-backed function-pointer slots, never literal targets."""
    result = {}
    for descriptor in descriptors:
        site, register = int(descriptor['site'], 16), descriptor['register']
        pointers = [int(value, 16) for value in descriptor['pointers']]
        if not 1 <= register <= (30 if tail else 31) or site % 4 or not 1 <= len(pointers) <= 128 or any(p % 4 for p in pointers):
            raise ValidationError('Invalid function-pointer call descriptor')
        offset = elf.guest_to_offset(site, 4)
        word = int.from_bytes(elf.data[offset:offset + 4], 'little')
        expected = (register << 21) | (8 if tail else (31 << 11) | 9)
        if site in result or word != expected:
            raise ValidationError('Duplicate or mismatched function-pointer transfer site')
        targets = []
        for pointer in pointers:
            offset = elf.guest_to_offset(pointer, 4)
            target = int.from_bytes(elf.data[offset:offset + 4], 'little')
            if target % 4 or not any(s.flags & 1 and s.vaddr <= target and target + 4 <= s.vaddr + s.filesz
                                     for s in elf.segments):
                raise ValidationError('Function pointer outside executable file-backed range')
            targets.append(target)
        result[site] = {'register': register, 'targets': targets, 'pointers': pointers}
    return result


def trap_maps(elf, descriptors):
    """Declare terminal JALR boundaries; these never authorize a target."""
    result = {}
    for descriptor in descriptors:
        site, register = int(descriptor['site'], 16), descriptor['register']
        if not 1 <= register <= 31 or site % 4:
            raise ValidationError('Invalid indirect-call trap descriptor')
        offset = elf.guest_to_offset(site, 4)
        word = int.from_bytes(elf.data[offset:offset + 4], 'little')
        if site in result or word != ((register << 21) | (31 << 11) | 9):
            raise ValidationError('Duplicate or mismatched indirect-call trap site')
        result[site] = {'register': register, 'targets': []}
    return result


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


def jump_maps(elf, descriptors):
    result = {}
    for descriptor in descriptors:
        site = int(descriptor['site'], 16)
        start = int(descriptor['table_start'], 16)
        register, count = descriptor['register'], descriptor['count']
        if not 1 <= register < 31 or not 1 <= count <= 1024 or site % 4 or start % 4:
            raise ValidationError('Invalid jump-table descriptor')
        site_offset = elf.guest_to_offset(site, 4)
        word = int.from_bytes(elf.data[site_offset:site_offset + 4], 'little')
        if site in result or word != ((register << 21) | 8):
            raise ValidationError('Duplicate or mismatched jump-table JR site')
        table_offset = elf.guest_to_offset(start, count * 4)
        targets = [int.from_bytes(elf.data[offset:offset + 4], 'little')
                   for offset in range(table_offset, table_offset + count * 4, 4)]
        if any(target % 4 or not any(s.flags & 1 and s.vaddr <= target and target + 4 <= s.vaddr + s.filesz
                                     for s in elf.segments) for target in targets):
            raise ValidationError('Jump-table target outside executable file-backed range')
        result[site] = {'register': register, 'targets': targets, 'table_start': start, 'table_end': start + count * 4}
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
