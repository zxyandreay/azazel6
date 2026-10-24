"""Bounded optional PortableKit comparison; never replaces the strict AOT probe."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from analyze import ROOT, local_output
from elf import load_profile, validate


def checked_source(source):
    expected = json.loads((ROOT / "dependencies.json").read_text())["portablekit"]["revision"]
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip()
    if actual != expected or dirty:
        raise ValueError("PortableKit must be the unmodified pinned revision; checkout left untouched")
    return actual


def probe_environment(output):
    # Do not inherit upstream diagnostic switches, acceleration or user settings.
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(("AZAZEL6_EVAL_", "PSPRECOMP_"))}
    for name in ("STRICT_HLE", "NO_RENDER", "NO_AUDIO", "NO_BUSY_CLOCK"):
        env["AZAZEL6_EVAL_" + name] = "1"
    env["AZAZEL6_EVAL_FAST_LOADING"] = "0"
    env["AZAZEL6_EVAL_DATA_DIR"] = str(output)
    env["PSPRECOMP_TRACE_ON_ERROR"] = "1"
    env["PSPRECOMP_HLE_HISTOGRAM"] = "1"
    return env


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["build", "run"])
    p.add_argument("input", nargs="?", type=Path)
    p.add_argument("--source", type=Path, default=ROOT / "local/deps/portablekit")
    p.add_argument("--build", type=Path, default=ROOT / "local/build/portablekit-probe")
    p.add_argument("--output", type=Path, default=ROOT / "local/portablekit-probe")
    args = p.parse_args(argv)
    if args.action == "run":
        if args.input is None:
            raise ValueError("Run requires an exact supported executable")
        validate(args.input, load_profile(ROOT / "profiles/tekken6/config/profile.json"))
    revision = checked_source(args.source)
    build = local_output(args.build)
    if args.action == "build":
        subprocess.run(["cmake", "-S", str(ROOT / "profiles/tekken6/portablekit"), "-B", str(build),
                        "-DAZAZEL6_PORTABLEKIT_SOURCE=" + str(args.source.resolve())], check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Release", "--parallel", "2",
                        "--target", "azazel6_portablekit_probe", "azazel6_portablekit_bootstrap_tests",
                        "azazel6_portablekit_clock_tests"], check=True)
        subprocess.run(["ctest", "--test-dir", str(build), "-C", "Release", "-R",
                        "^portablekit_(bootstrap|clock)_compatibility$", "--output-on-failure"], check=True)
        return 0
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=False)
    executable = build / ("bin/Release/azazel6_portablekit_probe.exe" if sys.platform == "win32"
                          else "bin/azazel6_portablekit_probe")
    result = {"scope": "UNVERIFIED upstream compatibility experiment, not native AOT milestone evidence",
              "portablekit_revision": revision, "timeout_seconds": 15, "status": "failed"}
    with (output / "execution.log").open("w", encoding="utf-8") as log:
        try:
            run = subprocess.run([str(executable), str(args.input.resolve()), str(output)],
                                 env=probe_environment(output), stdout=log, stderr=subprocess.STDOUT, timeout=15)
            result["returncode"] = run.returncode
            if run.returncode == 3:
                result["status"] = "research_boundary"
        except subprocess.TimeoutExpired:
            result["status"] = "timeout"
    (output / "execution.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["status"] == "research_boundary" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"portablekit_probe: {error}", file=sys.stderr)
        sys.exit(1)
