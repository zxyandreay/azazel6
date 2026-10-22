"""Inspect one unverified AOT startup root without authorizing or generating code.

All output belongs under ignored local/. A successful CFG scan is a research
candidate only; the exact game hash and resulting native execution must still be
verified before editing the startup profile.
"""
import argparse
import json
from pathlib import Path
import sys

from analyze import ROOT, local_output
from elf import ValidationError, load_profile, validate
from entry_cfg import discover_entry
from psp_module import module_report
from indirect_calls import (constructor_maps, immediate_maps, jump_maps, pointer_maps,
                            trap_maps)


def reviewed_transfers(elf, startup):
    """Use the same exact-input guarded transfer maps as entry_probe.py."""
    calls = constructor_maps(elf, startup.get("constructor_calls", []))
    pointers = pointer_maps(elf, startup.get("pointer_calls", []))
    immediates = immediate_maps(elf, startup.get("immediate_calls", []))
    jumps = jump_maps(elf, startup.get("jump_tables", []))
    tails = pointer_maps(elf, startup.get("pointer_tails", []), tail=True)
    traps = trap_maps(elf, startup.get("trapped_calls", []))
    tail_traps = trap_maps(elf, startup.get("trapped_tails", []), tail=True)
    families = (calls, pointers, immediates, jumps, tails, traps, tail_traps)
    if any(set(first) & set(second) for i, first in enumerate(families)
           for second in families[i + 1:]):
        raise ValidationError("Overlapping guarded transfer declarations")
    calls.update(pointers)
    calls.update(immediates)
    traps.update(tail_traps)
    return {"indirect_calls": {site: row["targets"] for site, row in calls.items()},
            "indirect_jumps": {site: row["targets"] for site, row in jumps.items()},
            "indirect_tails": {site: row["targets"] for site, row in tails.items()},
            "trapped_calls": traps}


def inspect_root(elf, address, roots, import_stubs=(), *, instruction_budget=4096,
                 known_transfers=None, inspect_existing_root=False):
    """Return bounded address-only evidence; never mutate the startup profile."""
    if (type(address) is not int or not 0 <= address <= 0xFFFFFFFC or address & 3 or
            type(instruction_budget) is not int or not 1 <= instruction_budget <= 4096 or
            type(inspect_existing_root) is not bool):
        raise ValidationError("Invalid root address or instruction budget")
    existing = frozenset(roots)
    imports = frozenset(import_stubs)
    if address in imports:
        raise ValidationError(f"0x{address:08X} is a PSP import stub, not a function root; "
                              "investigate its library/NID and HLE contract")
    is_existing_root = address in existing
    if is_existing_root and not inspect_existing_root:
        raise ValidationError(f"0x{address:08X} is already declared in the local startup roots; "
                              "use --inspect-existing-root for an audit, or regenerate "
                              "and rerun the bounded native probe to locate its next stop")
    if not any(s.flags & 1 and s.vaddr <= address and address + 4 <= s.vaddr + s.filesz
               for s in elf.segments):
        raise ValidationError("Candidate is not executable file-backed guest code")
    # Raw words are strictly local research output; file-backed bounds are checked
    # independently for every word, so a short function at a segment end is safe.
    preview = []
    for index in range(8):
        pc = address + index * 4
        if not any(s.flags & 1 and s.vaddr <= pc and pc + 4 <= s.vaddr + s.filesz
                   for s in elf.segments):
            break
        offset = elf.guest_to_offset(pc, 4)
        preview.append({"pc": f"0x{pc:08X}",
                        "word": f"0x{int.from_bytes(elf.data[offset:offset + 4], 'little'):08X}"})
    report = {"candidate": f"0x{address:08X}", "status": "RESEARCH_ONLY",
              "profile_membership": "EXISTING_STARTUP_ROOT" if is_existing_root else "NEW_CANDIDATE",
              "instruction_budget": instruction_budget, "first_words": preview,
              "limitations": "Static CFG only; not AOT coverage, boot progress or a verified function boundary"}
    try:
        cfg = discover_entry(elf, instruction_budget, address,
                             import_stubs=imports, external_entries=existing - {address},
                             **(known_transfers or {}))
    except ValidationError as error:
        report.update(status="BLOCKED_NEEDS_REVIEW", blocker=str(error))
        return report
    candidates = set()
    for target in cfg["calls"].values():
        if target in existing or target in imports or target == address:
            continue
        if any(s.flags & 1 and s.vaddr <= target and target + 4 <= s.vaddr + s.filesz
               for s in elf.segments):
            candidates.add(target)
    report.update(status="CFG_DISCOVERED_EXISTING_ROOT" if is_existing_root else
                  "CFG_DISCOVERED_NOT_AUTHORIZED",
                  instruction_words=cfg["instruction_words"],
                  ranges=[[f"0x{a:08X}", size] for a, size in cfg["ranges"]],
                  direct_calls={f"0x{site:08X}": f"0x{target:08X}"
                                for site, target in sorted(cfg["calls"].items())},
                  candidate_callees=[f"0x{target:08X}" for target in sorted(candidates)],
                  external_boundaries=[f"0x{pc:08X}" for pc in cfg["external_boundaries"]],
                  break_traps=[f"0x{pc:08X}" for pc in sorted(cfg["break_traps"])],
                  reviewed_indirect_sites=[f"0x{pc:08X}" for pc in sorted(cfg["indirect_calls"])],
                  trapped_transfer_sites=[f"0x{pc:08X}" for pc in sorted(cfg["trapped_calls"])])
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Your locally owned decrypted ULUS10466 EBOOT")
    parser.add_argument("--root", required=True, help="Uncovered guest function address, e.g. 0x08ACD010")
    parser.add_argument("--budget", type=int, default=4096)
    parser.add_argument("--inspect-existing-root", action="store_true",
                        help="Audit a root already in the local startup profile; never authorizes new AOT code")
    parser.add_argument("--output", type=Path, default=ROOT / "local/root-inspection")
    args = parser.parse_args(argv)
    # No side effects or output before verifying the exact executable fingerprint.
    elf = validate(args.input, load_profile(ROOT / "profiles/tekken6/config/profile.json"))
    startup = json.loads((ROOT / "profiles/tekken6/config/startup.json").read_text(encoding="utf-8"))
    module = module_report(elf)
    try:
        root = int(args.root, 16)
    except ValueError as error:
        raise ValidationError("Root must be a hexadecimal guest address") from error
    report = inspect_root(elf, root, (int(x, 16) for x in startup["roots"]),
                          (int(x["stub"], 16) for x in module["imports"]),
                          instruction_budget=args.budget,
                          known_transfers=reviewed_transfers(elf, startup),
                          inspect_existing_root=args.inspect_existing_root)
    report["input_sha256"] = elf.report()["sha256"]
    out = local_output(args.output)
    out.mkdir(parents=True, exist_ok=True)
    path = out / f"root-{root:08X}.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {path}")
    if "blocker" in report:
        print(f"Reason: {report['blocker']}", file=sys.stderr)
    return 0 if report["status"] in ("CFG_DISCOVERED_NOT_AUTHORIZED", "CFG_DISCOVERED_EXISTING_ROOT") else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"inspect_root: {error}", file=sys.stderr)
        sys.exit(1)
