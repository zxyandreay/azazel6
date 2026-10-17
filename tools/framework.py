"""Fetch/build/test the pinned, framework-only Yakumo dependency."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["fetch", "build", "test"])
    parser.add_argument("--source", type=Path, default=ROOT / "local/deps/yakumo")
    parser.add_argument("--build", type=Path, default=ROOT / "local/build/framework")
    args = parser.parse_args()
    lock = json.loads((ROOT / "dependencies.json").read_text())["yakumo"]
    if args.action == "fetch" and not args.source.exists():
        args.source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--filter=blob:none", "--no-checkout", lock["url"], args.source)
        run("git", "-C", args.source, "sparse-checkout", "set", *lock["paths"])
        run("git", "-C", args.source, "checkout", "--detach", lock["revision"])
    revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip()
    if revision != lock["revision"]:
        raise ValueError("Dependency revision mismatch; refusing to reset an existing checkout")
    dirty = subprocess.check_output(["git", "-C", str(args.source), "status", "--porcelain", "--untracked-files=no"], text=True)
    if dirty:
        raise ValueError("Dependency has tracked modifications; restore it before building")
    if args.action == "build":
        run("cmake", "-S", args.source, "-B", args.build, "-DPSPRECOMP_PROFILE=",
            "-DPSPRECOMP_MSVC_MP_JOBS=1")
        run("cmake", "--build", args.build, "--config", "Release", "--parallel", "2")
    elif args.action == "test":
        run("ctest", "--test-dir", args.build, "-C", "Release", "--output-on-failure")
    print(f"Framework {args.action}: {revision}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"framework: {error}", file=sys.stderr)
        sys.exit(1)
