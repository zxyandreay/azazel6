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


def inspect_root(elf, address, roots, import_stubs=(), *, instruction_budget=4096):
    """Return bounded address-only evidence; never mutate the startup profile."""
    if (type(address) is not int or not 0 <= address <= 0xFFFFFFFC or address & 3 or
            type(instruction_budget) is not int or not 1 <= instruction_budget <= 4096):
        raise ValidationError("Invalid root address or instruction budget")
    existing = frozenset(roots)
    imports = frozenset(import_stubs)
    if address in existing or address in imports:
        raise ValidationError("Candidate is already a startup root or import stub")
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
              "instruction_budget": instruction_budget, "first_words": preview,
              "limitations": "Static CFG only; not AOT coverage, boot progress or a verified function boundary"}
    try:
        cfg = discover_entry(elf, instruction_budget, address,
                             import_stubs=imports, external_entries=existing)
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
    report.update(status="CFG_DISCOVERED_NOT_AUTHORIZED",
                  instruction_words=cfg["instruction_words"],
                  ranges=[[f"0x{a:08X}", size] for a, size in cfg["ranges"]],
                  direct_calls={f"0x{site:08X}": f"0x{target:08X}"
                                for site, target in sorted(cfg["calls"].items())},
                  candidate_callees=[f"0x{target:08X}" for target in sorted(candidates)],
                  external_boundaries=[f"0x{pc:08X}" for pc in cfg["external_boundaries"]],
                  break_traps=[f"0x{pc:08X}" for pc in sorted(cfg["break_traps"])])
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Your locally owned decrypted ULUS10466 EBOOT")
    parser.add_argument("--root", required=True, help="Uncovered guest function address, e.g. 0x08ACD010")
    parser.add_argument("--budget", type=int, default=4096)
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
                          instruction_budget=args.budget)
    report["input_sha256"] = elf.report()["sha256"]
    out = local_output(args.output)
    out.mkdir(parents=True, exist_ok=True)
    path = out / f"root-{root:08X}.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {path}")
    if "blocker" in report:
        print(f"Reason: {report['blocker']}", file=sys.stderr)
    return 0 if report["status"] == "CFG_DISCOVERED_NOT_AUTHORIZED" else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"inspect_root: {error}", file=sys.stderr)
        sys.exit(1)
