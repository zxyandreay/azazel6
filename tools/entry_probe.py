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
from indirect_calls import constructor_maps, jump_maps, trap_maps, pointer_maps, immediate_maps, guard_generated, verify_break_stops
from aot_registration import compact_registration


def language_mode(startup):
    settings = startup.get("language_mode")
    if not isinstance(settings, dict) or set(settings) != {"language", "confirm_button"}:
        raise ValueError("Profile must explicitly configure language and confirm button")
    language, button = settings["language"], settings["confirm_button"]
    if type(language) is not int or not 0 <= language <= 11 or type(button) is not int or button not in (0, 1):
        raise ValueError("Unsupported profile language mode")
    return language, button


def ge_list_spans(elf, startup):
    """Validate explicitly investigated, file-backed GE initialization leases."""
    return file_data_spans(elf, startup, "ge_list_spans", 4)


def cache_writeback_spans(elf, startup):
    """Authorize complete cache lines of investigated file-backed data only."""
    return file_data_spans(elf, startup, "cache_writeback_spans", 64)


def file_data_spans(elf, startup, key, alignment):
    rows = startup.get(key, [])
    if not isinstance(rows, list) or len(rows) > 64:
        raise ValueError(f"Invalid {key} table")
    result = []
    for row in rows:
        if not isinstance(row, dict) or set(row) != {"begin", "end"}:
            raise ValueError(f"{key} require begin/end")
        if not all(isinstance(row[k], str) for k in ("begin", "end")):
            raise ValueError(f"{key} addresses must be hexadecimal strings")
        begin, end = int(row["begin"], 16), int(row["end"], 16)
        if (begin | end) & (alignment - 1) or not 0x08000000 <= begin < end <= 0x0A000000 or end - begin > 16384:
            raise ValueError(f"Invalid {key} bounds")
        elf.guest_to_offset(begin, end - begin)
        result.append((begin, end))
    result.sort()
    if any(a[1] > b[0] for a, b in zip(result, result[1:])):
        raise ValueError(f"Overlapping {key}")
    return result


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
    language, button = language_mode(startup)
    ge_spans = ge_list_spans(elf, startup)
    cache_spans = cache_writeback_spans(elf, startup)
    indirect = constructor_maps(elf, startup.get("constructor_calls", []))
    pointers = pointer_maps(elf, startup.get("pointer_calls", []))
    if indirect.keys() & pointers.keys():
        raise ValueError("Constructor and pointer call maps must own different sites")
    indirect |= pointers
    immediates = immediate_maps(elf, startup.get("immediate_calls", []))
    if indirect.keys() & immediates.keys():
        raise ValueError("Immediate and table call maps must own different sites")
    indirect |= immediates
    jumps = jump_maps(elf, startup.get("jump_tables", []))
    tails = pointer_maps(elf, startup.get("pointer_tails", []), tail=True)
    traps = trap_maps(elf, startup.get("trapped_calls", []))
    tail_traps = trap_maps(elf, startup.get("trapped_tails", []), tail=True)
    if traps.keys() & tail_traps.keys():
        raise ValueError("Call and tail traps must own different sites")
    traps |= tail_traps
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
    if any(start < end and start + size > begin for start, size in ranges for begin, end in cache_spans):
        raise ValueError("Cache writeback span intersects discovered guest code")
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
    source = generated.read_text()
    verify_break_stops(source, {site: word for graph in cfg.values()
                               for site, word in graph['break_traps'].items()})
    generated.write_text(compact_registration(guard_generated(source, indirect | jumps | traps | tails)), encoding="utf-8")
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
        f'#define AZAZEL6_BOOTSTRAP_STACK_SIZE {start_thread["stack_size"]}u\n'
        f'#define AZAZEL6_LANGUAGE {language}u\n#define AZAZEL6_CONFIRM_BUTTON {button}u\n'
        '#define AZAZEL6_GE_LIST_SPANS {' + ','.join(f'{{{begin}u,{end}u}}' for begin, end in ge_spans) + '}\n'
        '#define AZAZEL6_CACHE_WRITEBACK_SPANS {' + ','.join(f'{{{begin}u,{end}u}}' for begin, end in cache_spans) + '}\n',
        encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps({"input_sha256": profile["executable"]["sha256"],
        "language_mode": startup["language_mode"],
        "ge_list_spans": ge_spans,
        "cache_writeback_spans": cache_spans,
        "cfg": cfg, "module": module, "indirect_maps": indirect, "jump_maps": jumps, "trap_maps": traps, "tail_maps": tails,
        "scope": "Bounded startup CFG, guarded constructor calls and returning imports; no fallback"}, indent=2) + "\n")
    print(f"Generated {sum(g['instruction_words'] for g in cfg.values())} startup instruction words in {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"entry_probe: {error}", file=sys.stderr)
        sys.exit(1)
