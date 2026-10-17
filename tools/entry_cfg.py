"""Bounded startup CFG discovery; follows intraprocedural edges, not callees.

Only the control-flow forms needed by this entry profile are accepted. Code is
read from executable file-backed segments; no guessed contiguous function end.
"""
from elf import ValidationError


def discover_entry(elf, max_instructions=256, entry=None):
    pending = [elf.entry if entry is None else entry]
    words = set()
    instructions = set()
    delays = set()
    calls = {}

    def read(pc):
        if pc % 4 or not any(s.flags & 1 and s.vaddr <= pc and pc + 4 <= s.vaddr + s.filesz for s in elf.segments):
            raise ValidationError("CFG target outside executable file-backed range")
        offset = elf.guest_to_offset(pc, 4)
        return int.from_bytes(elf.data[offset:offset + 4], "little")

    def flow(word):
        op, rs, rt, function = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 63
        if op == 0 and function in (8, 9):
            if function == 8 and rs == 31:
                return "return"
            raise ValidationError("Indirect control transfer requires a verified target map")
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
        if pc in instructions:
            continue
        if pc in delays:
            raise ValidationError("Branch into a delay slot requires explicit handling")
        word = read(pc)
        kind = flow(word)
        instructions.add(pc)
        words.add(pc)
        if kind != "ordinary":
            delay = pc + 4
            if delay in instructions or flow(read(delay)) != "ordinary":
                raise ValidationError("Overlapping/control-transfer delay slot")
            delays.add(delay)
            words.add(delay)
        if len(words) > max_instructions:
            raise ValidationError("Startup CFG exceeds instruction budget")
        if kind == "ordinary":
            pending.append(pc + 4)
        elif kind == "branch":
            signed = (word & 0xFFFF) - (0x10000 if word & 0x8000 else 0)
            pending.extend([pc + 8, pc + 4 + signed * 4])
        elif kind in ("jump", "call"):
            target = ((pc + 4) & 0xF0000000) | ((word & 0x3FFFFFF) << 2)
            if kind == "jump":
                pending.append(target)
            else:
                calls[pc] = target
                pending.append(pc + 8)
    ranges = []
    for pc in sorted(words):
        if ranges and ranges[-1][0] + ranges[-1][1] == pc:
            ranges[-1][1] += 4
        else:
            ranges.append([pc, 4])
    return {"ranges": ranges, "instruction_words": len(words), "calls": calls,
            "instruction_addresses": sorted(instructions)}
