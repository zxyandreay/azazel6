"""Run the entry experiment with a hard wall-clock bound and retain exit evidence."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from analyze import ROOT, local_output
from elf import load_profile, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "local/entry-probe")
    args = parser.parse_args()
    validate(args.input, load_profile(ROOT / "profiles/tekken6/config/profile.json"))
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=True)
    result = {"status": "failed", "timeout_seconds": 15}
    with (output / "execution.log").open("w", encoding="utf-8") as log:
        try:
            process = subprocess.run([str(args.binary.resolve()), str(args.input.resolve())],
                                     stdout=log, stderr=subprocess.STDOUT, timeout=15)
            result["returncode"] = process.returncode
            if process.returncode == 3:
                result["status"] = "research_boundary"
        except subprocess.TimeoutExpired:
            result["status"] = "timeout"
    (output / "execution.json").write_text(json.dumps(result, indent=2) + "\n")
    print((output / "execution.log").read_text())
    print(json.dumps(result))
    return 0 if result["status"] == "research_boundary" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"run_probe: {error}", file=sys.stderr)
        sys.exit(1)
