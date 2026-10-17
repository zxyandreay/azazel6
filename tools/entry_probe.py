"""Generate a bounded native entry slice, without claiming a complete function map."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from analyze import ROOT, execute, local_output
from elf import load_profile, validate
from psp_module import module_report


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
    # Stop at the first JAL after executing its delay slot. No guessed function end.
    size = None
    for delta in range(0, 256, 4):
        offset = elf.guest_to_offset(elf.entry + delta, 8)
        word = int.from_bytes(elf.data[offset:offset + 4], "little")
        if word >> 26 == 3:
            size = delta + 8
            break
    if size is None:
        raise ValueError("No first JAL within 256 bytes of the verified entry")
    (output / "entry.csv").write_text(f"name,address,size\nentry_slice,0x{elf.entry:08X},0x{size:X}\n", encoding="utf-8")
    exe = args.tools_dir.resolve() / ("psp_recomp.exe" if sys.platform == "win32" else "psp_recomp")
    execute([exe, args.input.resolve(), output / "entry.csv", output / "entry.cpp"], output / "generation.log", 60)
    (output / "probe_config.hpp").write_text(
        '#pragma once\n#define AZAZEL6_EXPECTED_SHA "' + profile["executable"]["sha256"] + '"\n'
        f'#define AZAZEL6_MODULE_ADDRESS {module["address"]}u\n', encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps({"input_sha256": profile["executable"]["sha256"],
        "slice_bytes": size, "module": module, "scope": "Entry through first JAL and delay slot; no fallback"}, indent=2) + "\n")
    print(f"Generated {size}-byte entry slice in {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"entry_probe: {error}", file=sys.stderr)
        sys.exit(1)
