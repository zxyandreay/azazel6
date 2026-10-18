"""Bounded startup CFG discovery; follows intraprocedural edges, not callees.

Only the control-flow forms needed by this entry profile are accepted. Code is
read from executable file-backed segments; no guessed contiguous function end.
"""
from elf import ValidationError


def discover_entry(elf, max_instructions=256, entry=None, *, constant_branches=False, import_stubs=(),
                   indirect_calls=None, external_entries=(), indirect_jumps=None):
    unknown = (0,) + (None,) * 31
    pending = []
    states = {}
    words = set()
    instructions = set()
    delays = set()
    calls = {}
    imports = frozenset(import_stubs) | frozenset(external_entries)
    if any(type(address) is not int or address < 0 or address > 0xFFFFFFFC or address % 4 for address in imports):
        raise ValidationError("External entries must be aligned 32-bit guest addresses")
    tail_calls = {}
    indirect = indirect_calls or {}
    used_indirect = {}
    jumps = indirect_jumps or {}
    used_jumps = {}

    def enqueue(pc, registers):
        previous = states.get(pc)
        merged = registers if previous is None else tuple(
            a if a == b else None for a, b in zip(previous, registers))
        if previous != merged:
            states[pc] = merged
            pending.append(pc)

    def transfer(word, registers):
        # A deliberately tiny, conservative lattice: unknown instructions kill
        # every fact, calls kill all facts, and joins retain only equal constants.
        if not constant_branches or word == 0:
            return registers
        op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
        immediate = word & 0xFFFF
        result = list(registers)
        if op == 15 and rs == 0:  # LUI
            result[rt] = immediate << 16
        elif op in (9, 13):  # ADDIU (wraps), ORI (zero extends)
            value = registers[rs]
            signed = immediate - (0x10000 if immediate & 0x8000 else 0)
            result[rt] = None if value is None else (
                (value + signed) & 0xFFFFFFFF if op == 9 else value | immediate)
        elif op in (40, 41, 42, 43, 46):  # SB, SH, SWL, SW, SWR do not write GPRs.
            return registers
        elif op in (32, 33, 34, 35, 36, 37, 38, 48):  # Integer loads / LL.
            result[rt] = None
        else:
            return unknown
        result[0] = 0
        return tuple(result)

    enqueue(elf.entry if entry is None else entry, unknown)

    def read(pc):
        if pc % 4 or not any(s.flags & 1 and s.vaddr <= pc and pc + 4 <= s.vaddr + s.filesz for s in elf.segments):
            raise ValidationError("CFG target outside executable file-backed range")
        offset = elf.guest_to_offset(pc, 4)
        return int.from_bytes(elf.data[offset:offset + 4], "little")

    def flow(word, pc):
        op, rs, rt, function = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 63
        if op == 0 and function in (8, 9):
            if function == 8 and rs == 31:
                return "return"
            if function == 9 and pc in indirect and indirect[pc] and word == (rs << 21) | (31 << 11) | 9:
                return "indirect_call"
            if function == 8 and pc in jumps and jumps[pc] and word == (rs << 21) | 8:
                return "indirect_jump"
            raise ValidationError(f"Indirect control transfer at 0x{pc:08X} requires a verified target map")
        if op == 0 and function in (12, 13):
            raise ValidationError("Inline syscall/break requires explicit profile handling")
        if op in (2, 3):
            return "jump" if op == 2 else "call"
        if op in (4, 5, 6, 7, 20, 21, 22, 23) or (op == 1 and rt in (0, 1, 2, 3)):
            return "branch"
        if op == 1 or (op in (16, 17, 18) and rs == 8):
            raise ValidationError("Unsupported branch form in startup CFG")
        return "ordinary"

    while pending:
        pc = pending.pop()
        registers = states[pc]
        if pc in delays:
            raise ValidationError("Branch into a delay slot requires explicit handling")
        word = read(pc)
        kind = flow(word, pc)
        instructions.add(pc)
        words.add(pc)
        if kind != "ordinary":
            delay = pc + 4
            if delay in instructions or flow(read(delay), delay) != "ordinary":
                raise ValidationError("Overlapping/control-transfer delay slot")
            delays.add(delay)
            words.add(delay)
        if len(words) > max_instructions:
            raise ValidationError("Startup CFG exceeds instruction budget")
        if kind == "ordinary":
            enqueue(pc + 4, transfer(word, registers))
        elif kind == "branch":
            signed = (word & 0xFFFF) - (0x10000 if word & 0x8000 else 0)
            op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
            taken = None
            if constant_branches and op in (4, 5, 20, 21) and registers[rs] is not None and registers[rt] is not None:
                taken = (registers[rs] == registers[rt]) == (op in (4, 20))
            delayed = transfer(read(pc + 4), registers)
            if taken is not True:
                # Branch-likely annuls the delay slot on the not-taken edge.
                likely = op in (20, 21, 22, 23) or (op == 1 and rt in (2, 3))
                enqueue(pc + 8, registers if likely else delayed)
            if taken is not False:
                enqueue(pc + 4 + signed * 4, delayed)
        elif kind == "indirect_call":
            used_indirect[pc] = indirect[pc]
            enqueue(pc + 8, unknown)
        elif kind == "indirect_jump":
            used_jumps[pc] = jumps[pc]
            for target in jumps[pc]:
                enqueue(target, unknown)
        elif kind in ("jump", "call"):
            target = ((pc + 4) & 0xF0000000) | ((word & 0x3FFFFFF) << 2)
            if kind == "jump":
                if target in imports:
                    tail_calls[pc] = target
                else:
                    enqueue(target, transfer(read(pc + 4), registers))
            else:
                calls[pc] = target
                enqueue(pc + 8, unknown)
    ranges = []
    for pc in sorted(words):
        if ranges and ranges[-1][0] + ranges[-1][1] == pc:
            ranges[-1][1] += 4
        else:
            ranges.append([pc, 4])
    return {"ranges": ranges, "instruction_words": len(words), "calls": calls, "tail_calls": tail_calls,
            "indirect_calls": used_indirect,
            "indirect_jumps": used_jumps,
            "instruction_addresses": sorted(instructions)}
