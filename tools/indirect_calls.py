"""Validate profile-declared indirect tables and guard generated transfers."""
from elf import ValidationError


def merge_call_maps(*maps):
    """Combine independently verified target sources for one guarded call site."""
    result = {}
    for mapping in maps:
        for site, row in mapping.items():
            if site not in result:
                result[site] = dict(row)
                continue
            if result[site]['register'] != row['register']:
                raise ValidationError('Conflicting guarded-call registers')
            result[site]['targets'] = list(dict.fromkeys(
                result[site]['targets'] + row['targets']))
    return result


def immediate_maps(elf, descriptors):
    """Resolve reviewed bounded LUI/ADDIU or LUI/ORI address constructions."""
    result = {}

    def executable(address, size):
        return 0 <= address <= 0xFFFFFFFF and address % 4 == 0 and any(
            s.flags & 1 and s.vaddr <= address and address + size <= s.vaddr + s.filesz
            for s in elf.segments)

    for descriptor in descriptors:
        site, register = int(descriptor['site'], 16), descriptor['register']
        sources = []
        delay_branches = {}
        for value in descriptor['sources']:
            if isinstance(value, dict):
                if set(value) not in ({'upper', 'lower'}, {'upper', 'lower', 'delay_slot_branch'}):
                    raise ValidationError('Invalid immediate source descriptor')
                pair = (int(value['upper'], 16), int(value['lower'], 16))
                if 'delay_slot_branch' in value:
                    delay_branches[pair] = int(value['delay_slot_branch'], 16)
            else:
                pair = (int(value, 16), int(value, 16) + 4)
            sources.append(pair)
        if (not 1 <= register <= 31 or not executable(site, 4) or
                not 1 <= len(sources) <= 128 or len(set(sources)) != len(sources)):
            raise ValidationError('Invalid immediate call descriptor')
        offset = elf.guest_to_offset(site, 4)
        word = int.from_bytes(elf.data[offset:offset + 4], 'little')
        if site in result or word != (register << 21) | (31 << 11) | 9:
            raise ValidationError('Duplicate or mismatched immediate JALR site')
        targets = []
        for source, end in sources:
            if (end % 4 or end > 0xFFFFFFFC or not source + 4 <= end <= source + 32 or
                    not executable(source, end - source + 4)):
                raise ValidationError('Immediate source outside executable file-backed range')
            offset = elf.guest_to_offset(source, end - source + 4)
            upper = int.from_bytes(elf.data[offset:offset + 4], 'little')
            lower_offset = offset + end - source
            lower = int.from_bytes(elf.data[lower_offset:lower_offset + 4], 'little')
            dest = (upper >> 16) & 31
            opcode = lower >> 26
            if (upper >> 21 != 15 << 5 or not dest or opcode not in (9, 13) or
                    (lower >> 21) & 31 != dest or (lower >> 16) & 31 == 0):
                raise ValidationError('Immediate source is not a canonical address pair')
            accepted_delay_branch = False
            for middle in range(offset + 4, lower_offset, 4):
                word = int.from_bytes(elf.data[middle:middle + 4], 'little')
                op, rt = word >> 26, (word >> 16) & 31
                # A reviewed non-likely BLEZ executes its delay slot on *both* edges.
                # Require an explicit exact-word witness, LUI/BLEZ/ADDIU adjacency,
                # and a forward target past the pair. Other intervening branches,
                # including branch-likely (annulled delay), remain disallowed.
                if (source, end) in delay_branches and middle == offset + 4 == lower_offset - 4:
                    expected = delay_branches[(source, end)]
                    displacement = word & 0xFFFF
                    if displacement & 0x8000:
                        displacement -= 0x10000
                    branch_target = source + 8 + 4 * displacement
                    if (word == expected and op == 6 and rt == 0 and (word >> 21) & 31 and
                            end + 4 < branch_target <= 0xFFFFFFFC and executable(branch_target, 4)):
                        accepted_delay_branch = True
                        continue
                # Other forms must not change the address register or control flow.
                # Canonical move (ADDU rd, rs, zero) only writes rd.
                move = op == 0 and rt == 0 and (word & 0x7ff) == 0x21 and ((word >> 11) & 31) != dest
                if not (word == 0 or move or (op in (9, 13) and rt != dest) or
                        (op == 15 and (word >> 21) & 31 == 0 and rt != dest) or op in (40, 41, 43)):
                    raise ValidationError('Intervening instruction invalidates immediate address proof')
            if (source, end) in delay_branches and not accepted_delay_branch:
                raise ValidationError('Declared delay-slot branch was not verified')
            low = lower & 0xFFFF
            if opcode == 9 and low & 0x8000:
                low -= 0x10000
            target = (((upper & 0xFFFF) << 16) + low) & 0xFFFFFFFF
            if not executable(target, 4):
                raise ValidationError('Immediate target outside executable file-backed range')
            targets.append(target)
        result[site] = {'register': register, 'targets': targets, 'source_pairs': sources}
    return result


def watched_slot_maps(elf, descriptors):
    """Guard a runtime-written BSS slot with reviewed exact instruction witnesses.

    The witnesses establish the source and storage shape; a bounded write watch
    must separately establish that the store actually executed. Other live
    indices/targets remain rejected by the generated runtime guard.
    """
    result = {}

    def read(address):
        if type(address) is not int or address & 3 or not any(
                s.flags & 1 and s.vaddr <= address and address + 4 <= s.vaddr + s.filesz
                for s in elf.segments):
            raise ValidationError('Watched-slot witness is not executable file-backed code')
        offset = elf.guest_to_offset(address, 4)
        return int.from_bytes(elf.data[offset:offset + 4], 'little')

    def address_pair(upper_address, lower_address):
        upper, lower = read(upper_address), read(lower_address)
        register = (upper >> 16) & 31
        if (upper >> 26 != 15 or (upper >> 21) & 31 or not register or
                lower >> 26 != 9 or (lower >> 21) & 31 != register or
                ((lower >> 16) & 31) == 0):
            raise ValidationError('Watched-slot address pair is not LUI/ADDIU')
        low = lower & 0xFFFF
        if low & 0x8000:
            low -= 0x10000
        return (((upper & 0xFFFF) << 16) + low) & 0xFFFFFFFF, (lower >> 16) & 31

    for descriptor in descriptors:
        if set(descriptor) != {'site', 'register', 'slot', 'index', 'stride',
                               'target_upper', 'target_lower', 'slot_upper',
                               'slot_lower', 'store', 'load'}:
            raise ValidationError('Invalid watched-slot descriptor fields')
        site = int(descriptor['site'], 16)
        slot = int(descriptor['slot'], 16)
        index, stride = descriptor['index'], descriptor['stride']
        register = descriptor['register']
        upper = int(descriptor['target_upper'], 16)
        lower = int(descriptor['target_lower'], 16)
        base_upper = int(descriptor['slot_upper'], 16)
        base_lower = int(descriptor['slot_lower'], 16)
        store = int(descriptor['store'], 16)
        load = int(descriptor['load'], 16)
        if (not 1 <= register <= 31 or slot & 3 or
                type(index) is not int or not 0 <= index < 1024 or
                type(stride) is not int or not 4 <= stride <= 4096 or stride & 3 or
                not upper < lower < store < load < site or
                base_lower != base_upper + 4 or base_lower >= store or load + 4 != site or
                read(site) != (register << 21) | (31 << 11) | 9):
            raise ValidationError('Invalid watched-slot call site or witness order')
        target, value_register = address_pair(upper, lower)
        base, base_register = address_pair(base_upper, base_lower)
        store_word, load_word = read(store), read(load)
        displacement = store_word & 0xFFFF
        if displacement & 0x8000:
            displacement -= 0x10000
        load_displacement = load_word & 0xFFFF
        if load_displacement & 0x8000:
            load_displacement -= 0x10000
        if (store_word >> 26 != 43 or (store_word >> 21) & 31 != base_register or
                (store_word >> 16) & 31 != value_register or
                (base + displacement) & 0xFFFFFFFF != slot or
                load_word >> 26 != 35 or (load_word >> 16) & 31 != register or
                displacement != index * stride + load_displacement):
            raise ValidationError('Watched-slot store/load witness mismatch')
        if not any(s.flags & 2 and s.vaddr + s.filesz <= base and
                   s.vaddr + s.filesz <= slot and
                   s.vaddr <= base and slot + 4 <= s.vaddr + s.memsz
                   for s in elf.segments):
            raise ValidationError('Watched slot is not owned writable BSS')
        if target & 3 or not any(s.flags & 1 and s.vaddr <= target and
                                 target + 4 <= s.vaddr + s.filesz
                                 for s in elf.segments):
            raise ValidationError('Watched-slot target is not executable file-backed code')
        row = result.setdefault(site, {'register': register, 'targets': [],
                                       'slots': [], 'witnesses': []})
        if row['register'] != register or slot in row['slots']:
            raise ValidationError('Duplicate or conflicting watched-slot call')
        row['targets'].append(target)
        row['slots'].append(slot)
        row['witnesses'].append([upper, lower, base_upper, base_lower, store, load])
    return result


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


def trap_maps(elf, descriptors, *, tail=False):
    """Declare terminal JALR/JR boundaries; these never authorize a target."""
    result = {}
    for descriptor in descriptors:
        site, register = int(descriptor['site'], 16), descriptor['register']
        if not 1 <= register <= (30 if tail else 31) or site % 4:
            raise ValidationError('Invalid indirect-call trap descriptor')
        offset = elf.guest_to_offset(site, 4)
        word = int.from_bytes(elf.data[offset:offset + 4], 'little')
        if site in result or word != ((register << 21) | (8 if tail else (31 << 11) | 9)):
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


def verify_break_stops(source, traps):
    """Require the pinned emitter's terminal stop for each discovered BREAK."""
    for site, word in traps.items():
        label = f'L_{site:08X}:\n'
        stop = (f'    rt.unsupported(0x{site:08X}u, 0x{word:08X}u, '
                '"guest break trap"); return;\n')
        if source.count(label) != 1 or label + stop not in source:
            raise ValidationError('Generated BREAK stop missing, changed or duplicated')


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
