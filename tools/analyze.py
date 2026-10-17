"""Validate ULUS10466, run pinned PSPRecomp tools, retain local evidence."""
import argparse
import json
import hashlib
from pathlib import Path
import subprocess
import sys

from elf import ValidationError, load_profile, validate
from psp_module import module_report

ROOT = Path(__file__).resolve().parents[1]


def local_output(path):
    resolved = path.resolve()
    if not resolved.is_relative_to((ROOT / "local").resolve()):
        raise ValidationError("Analysis/generated output must be inside this checkout's ignored local/ directory")
    return resolved


def execute(command, log, timeout):
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run([str(x) for x in command], stdout=output, stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        raise ValidationError(f"{Path(command[0]).name} exited {result.returncode}; inspect {log.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--profile", type=Path, default=ROOT / "profiles/tekken6/config/profile.json")
    parser.add_argument("--output", type=Path, default=ROOT / "local/analysis/tekken6")
    parser.add_argument("--tools-dir", type=Path, help="Directory containing psp_analyze and psp_recomp")
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=True)
    report = {"status": "failed", "game_id": "ULUS10466", "steps": []}
    try:
        if args.timeout < 1 or args.timeout > 3600:
            raise ValidationError("Timeout must be 1..3600 seconds")
        if args.generate and not args.tools_dir:
            raise ValidationError("--generate requires --tools-dir")
        profile = load_profile(args.profile)
        elf = validate(args.input, profile)
        report["validation"] = elf.report()
        report["profile_module"] = module_report(elf)
        report["steps"].append("validated")
        if args.tools_dir:
            suffix = ".exe" if sys.platform == "win32" else ""
            analyzer = args.tools_dir.resolve() / ("psp_analyze" + suffix)
            report["tool_sha256"] = {"psp_analyze": hashlib.sha256(analyzer.read_bytes()).hexdigest()}
            execute([analyzer, args.input.resolve(), output / "psprecomp.json"], output / "analyzer.log", args.timeout)
            report["analysis"] = json.loads((output / "psprecomp.json").read_text())
            report["steps"].append("analyzed")
            if args.generate:
                report["tool_sha256"]["psp_recomp"] = hashlib.sha256(
                    (args.tools_dir.resolve() / ("psp_recomp" + suffix)).read_bytes()).hexdigest()
                generated = output / "generated"
                if generated.exists() and any(generated.iterdir()):
                    raise ValidationError("Generated directory is nonempty; choose a fresh --output to avoid stale code")
                # ET_EXEC addresses are absolute; the framework default load base does not rebase them.
                execute([args.tools_dir.resolve() / ("psp_recomp" + suffix), args.input.resolve(),
                         "--auto", generated, "0x08804000", str(profile["generation"]["unit_span"]),
                         "--prefix", profile["generation"]["prefix"]], output / "generation.log", args.timeout)
                (generated / "azazel6-input.json").write_text(json.dumps({
                    "sha256": report["validation"]["sha256"],
                    "expected_framework_revision": json.loads((ROOT / "dependencies.json").read_text())["yakumo"]["revision"],
                    "tool_sha256": report["tool_sha256"],
                    "profile": profile}, indent=2) + "\n", encoding="utf-8")
                report["steps"].append("generated")
        report["status"] = "passed"
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        report["error"] = str(error)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {', '.join(report['steps']) or 'no completed steps'}; report: {output / 'report.json'}")
    if report["status"] != "passed":
        print(report["error"], file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValidationError) as error:
        print(f"analyze: {error}", file=sys.stderr)
        sys.exit(1)
