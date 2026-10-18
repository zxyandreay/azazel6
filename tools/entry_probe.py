"""Generate a bounded native entry slice, without claiming a complete function map."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from analyze import ROOT, execute, local_output
from elf import load_profile, validate
from psp_module import module_report
from entry_cfg import discover_entry
from indirect_calls import constructor_maps, jump_maps, trap_maps, pointer_maps, guard_generated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--tools-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "local/entry-probe")
    args = parser.parse_args()
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=True)
    profile = load_profile(ROOT / "profiles/tekken6/config/profile.json")
    elf = validate(args.input, profile)
    module = module_report(elf)
    startup = json.loads((ROOT / "profiles/tekken6/config/startup.json").read_text())
    indirect = constructor_maps(elf, startup.get("constructor_calls", []))
    pointers = pointer_maps(elf, startup.get("pointer_calls", []))
    if indirect.keys() & pointers.keys():
        raise ValueError("Constructor and pointer call maps must own different sites")
    indirect |= pointers
    jumps = jump_maps(elf, startup.get("jump_tables", []))
    tails = pointer_maps(elf, startup.get("pointer_tails", []), tail=True)
    traps = trap_maps(elf, startup.get("trapped_calls", []))
    if (indirect.keys() & jumps.keys() or tails.keys() & (indirect.keys() | jumps.keys()) or
            traps.keys() & (indirect.keys() | jumps.keys() | tails.keys())):
        raise ValueError("Call, jump and trap maps must own different sites")
    cfg = {root: discover_entry(elf, startup["max_instructions_per_root"], int(root, 16),
                               constant_branches=root in startup.get("constant_branch_roots", []),
                               import_stubs=(int(imp["stub"], 16) for imp in module["imports"]),
                               external_entries=(int(other, 16) for other in startup["roots"] if other != root),
                               indirect_calls={site: mapping['targets'] for site, mapping in indirect.items()},
                               indirect_jumps={site: mapping['targets'] for site, mapping in jumps.items()},
                               trapped_calls=traps,
                               indirect_tails={site: mapping['targets'] for site, mapping in tails.items()})
           for root in startup["roots"]}
    ranges = sorted(pair for graph in cfg.values() for pair in graph["ranges"])
    if any(a + size > b for (a, size), (b, _) in zip(ranges, ranges[1:])):
        raise ValueError("Overlapping startup roots")
    start_thread = module["start_thread"]
    if not start_thread or not 8 <= start_thread["priority"] <= 119 or not 512 <= start_thread["stack_size"] <= 65536 or start_thread["attributes"] != 0x80000000:
        raise ValueError("Unsupported or missing module-start thread parameters")
    csv = "name,address,size\n" + "".join(
        f"entry_range_{i},0x{start:08X},0x{size:X}\n" for i, (start, size) in enumerate(ranges))
    (output / "entry.csv").write_text(csv, encoding="utf-8")
    exe = args.tools_dir.resolve() / ("psp_recomp.exe" if sys.platform == "win32" else "psp_recomp")
    execute([exe, args.input.resolve(), output / "entry.csv", output / "entry.cpp"], output / "generation.log", 60)
    used_sites = {site for graph in cfg.values() for site in graph['indirect_calls']}
    if used_sites != indirect.keys():
        raise ValueError("Declared indirect call sites are not covered by startup roots")
    if {site for graph in cfg.values() for site in graph['indirect_jumps']} != jumps.keys():
        raise ValueError("Declared jump-table sites are not covered by startup roots")
    if {site for graph in cfg.values() for site in graph['trapped_calls']} != traps.keys():
        raise ValueError("Declared trap sites are not covered by startup roots")
    if {site for graph in cfg.values() for site in graph['indirect_tails']} != tails.keys():
        raise ValueError("Declared indirect tail sites are not covered by startup roots")
    generated = output / "entry.cpp"
    generated.write_text(guard_generated(generated.read_text(), indirect | jumps | traps | tails), encoding="utf-8")
    wrappers = ['#include "azazel6/bootstrap.hpp"', 'namespace azazel6 {']
    for index, imp in enumerate(module["imports"]):
        wrappers.append(f'void import_{index}(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {{ '
                        f'invoke_returning_import(rt, ctx, {json.dumps(imp["library"])}, {imp["nid"]}u); }}')
    wrappers.append('void register_profile_imports(psprecomp::Runtime& rt) {')
    for index, imp in enumerate(module["imports"]):
        wrappers.append(f'rt.register_function({imp["stub"]}u, &import_{index}, "psp_import_{index}");')
    wrappers.append('}\n}')
    (output / "imports.cpp").write_text("\n".join(wrappers) + "\n", encoding="utf-8")
    (output / "probe_config.hpp").write_text(
        '#pragma once\n#define AZAZEL6_EXPECTED_SHA "' + profile["executable"]["sha256"] + '"\n'
        f'#define AZAZEL6_MODULE_ADDRESS {module["address"]}u\n'
        f'#define AZAZEL6_BOOTSTRAP_PRIORITY {start_thread["priority"]}u\n'
        f'#define AZAZEL6_BOOTSTRAP_STACK_SIZE {start_thread["stack_size"]}u\n', encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps({"input_sha256": profile["executable"]["sha256"],
        "cfg": cfg, "module": module, "indirect_maps": indirect, "jump_maps": jumps, "trap_maps": traps, "tail_maps": tails,
        "scope": "Bounded startup CFG, guarded constructor calls and returning imports; no fallback"}, indent=2) + "\n")
    print(f"Generated {sum(g['instruction_words'] for g in cfg.values())} startup instruction words in {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"entry_probe: {error}", file=sys.stderr)
        sys.exit(1)
