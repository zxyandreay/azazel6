"""Generate the one reviewed Tekken 6 entry function for PortableKit hybrid runs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from analyze import ROOT, local_output
from elf import ValidationError, load_profile, validate
from entry_cfg import discover_entry
from portablekit_probe import checked_source

ENTRY = 0x08804124
ENTRY_SIZE = 0x114


def checked_entry(elf):
    cfg = discover_entry(elf, max_instructions=256)
    if elf.entry != ENTRY or cfg["ranges"] != [[ENTRY, ENTRY_SIZE]] or cfg["instruction_words"] != 69:
        raise ValidationError("Entry CFG differs from the reviewed exact-EBOOT function")
    if len(cfg["instruction_addresses"]) != 56 or not set(cfg["instruction_addresses"]).issubset(
            range(ENTRY, ENTRY + ENTRY_SIZE, 4)):
        raise ValidationError("Entry CFG control PCs differ from the reviewed function")
    return cfg


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--compiler", type=Path, required=True, help="psp_recomp from the pinned PortableKit build")
    parser.add_argument("--source", type=Path, default=ROOT / "local/deps/portablekit")
    parser.add_argument("--output", type=Path, default=ROOT / "local/portablekit-hybrid")
    args = parser.parse_args(argv)

    # Complete both checks before writing any output or invoking code generation.
    revision = checked_source(args.source)
    profile = load_profile(ROOT / "profiles/tekken6/config/profile.json")
    elf = validate(args.input, profile)
    cfg = checked_entry(elf)
    output = local_output(args.output)
    if output.exists() and any(output.iterdir()):
        raise ValidationError("AOT output directory must be empty; choose a fresh local/ path")
    compiler = args.compiler.resolve(strict=True)
    output.mkdir(parents=True, exist_ok=True)
    csv = output / "functions.csv"
    generated = output / "generated_entry.cpp"
    csv.write_text("name,address,size\nTekken6Entry,0x08804124,0x114\n", encoding="ascii")
    with (output / "generation.log").open("w", encoding="utf-8") as log:
        run = subprocess.run([str(compiler), str(args.input.resolve()), str(csv), str(generated)],
                             stdout=log, stderr=subprocess.STDOUT, timeout=300)
    if run.returncode:
        raise ValidationError("Pinned recompiler failed; inspect local generation.log")
    source = generated.read_text(encoding="utf-8")
    # This generated routine has only the compiler's unreachable-entry guard.
    if re.findall(r"rt\.unsupported\([^;]+;", source) != [
            'rt.unsupported(ctx.pc, 0u, "invalid internal function entry");']:
        raise ValidationError("Generated entry contains a new unsupported instruction")
    registrations = re.findall(r"runtime\.register_function\((0x[0-9A-Fa-f]+)u, &Tekken6Entry,", source)
    if {int(pc, 16) for pc in registrations} != set(cfg["instruction_addresses"]):
        raise ValidationError("Generated entry registration differs from the reviewed CFG")
    report = {"scope": "Exact-input entry-function AOT generation; execution requires a hybrid run",
              "portablekit_revision": revision, "executable_sha256": profile["executable"]["sha256"],
              "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
              "entry": f"0x{ENTRY:08X}", "function_bytes": ENTRY_SIZE,
              "registered_entry_pcs": len(registrations),
              "generated_source_sha256": hashlib.sha256(generated.read_bytes()).hexdigest()}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"portablekit_aot: {error}", file=sys.stderr)
        sys.exit(1)
