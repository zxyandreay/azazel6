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
    cfg = discover_entry(elf)
    csv = "name,address,size\n" + "".join(
        f"entry_range_{i},0x{start:08X},0x{size:X}\n" for i, (start, size) in enumerate(cfg["ranges"]))
    (output / "entry.csv").write_text(csv, encoding="utf-8")
    exe = args.tools_dir.resolve() / ("psp_recomp.exe" if sys.platform == "win32" else "psp_recomp")
    execute([exe, args.input.resolve(), output / "entry.csv", output / "entry.cpp"], output / "generation.log", 60)
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
        f'#define AZAZEL6_MODULE_ADDRESS {module["address"]}u\n', encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps({"input_sha256": profile["executable"]["sha256"],
        "cfg": cfg, "module": module, "scope": "Bounded startup CFG and returning import wrappers; no callees or fallback"}, indent=2) + "\n")
    print(f"Generated {cfg['instruction_words']} startup instruction words in {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"entry_probe: {error}", file=sys.stderr)
        sys.exit(1)
