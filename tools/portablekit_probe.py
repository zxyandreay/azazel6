"""Bounded optional PortableKit comparison; never replaces the strict AOT probe."""
import argparse
import json
import os
import re
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


def clear_packet(trace, expected_sha):
    """Parse one bounded failed-list observation; never infer missing words."""
    if len(trace) > 8 * 1024 * 1024 or re.findall(r"^input_sha256=(\w+)$", trace, re.M) != [expected_sha]:
        raise ValueError("Expected one exact-input trace fingerprint within the size budget")
    headers = re.findall(r"^ge_clear_vertices=(\d+) address=(0x[0-9A-Fa-f]{8})$", trace, re.M)
    leases = re.findall(r"^ge_list=\d+ begin=(0x[0-9A-Fa-f]{8}) end=(0x[0-9A-Fa-f]{8}) pc=0x[0-9A-Fa-f]{8} commands=\d+ status=4$", trace, re.M)
    if len(headers) != 1 or len(leases) != 1:
        raise ValueError("Expected one failed list and one complete clear vertex observation")
    count, address = int(headers[0][0]), int(headers[0][1], 16)
    begin, end = (int(v, 16) for v in leases[0])
    if not (0 < count <= 64 and count % 2 == 0 and not ((address | begin | end) & 3)
            and 0x08000000 <= begin <= address < address + count * 12 <= end <= 0x0A000000):
        raise ValueError("Clear observation is outside its bounded canonical list lease")
    words = re.findall(r"^ge_vertex_word=(0x[0-9A-Fa-f]{8}) value=(0x[0-9A-Fa-f]{8})$", trace, re.M)
    if len(words) != count * 3 or any(int(at, 16) != address + i * 4 for i, (at, _) in enumerate(words)):
        raise ValueError("Clear observation is truncated, duplicated or noncontiguous")
    return f"AZAZEL6_CLEAR_V1 {count}\n" + "\n".join(str(int(word, 16)) for _, word in words) + "\n"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["build", "run", "decode-clear"])
    p.add_argument("input", nargs="?", type=Path)
    p.add_argument("--source", type=Path, default=ROOT / "local/deps/portablekit")
    p.add_argument("--build", type=Path, default=ROOT / "local/build/portablekit-probe")
    p.add_argument("--output", type=Path, default=ROOT / "local/portablekit-probe")
    p.add_argument("--trace", type=Path, help="Local native execution.log for decode-clear")
    args = p.parse_args(argv)
    if args.action != "build":
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
                        "^portablekit_(bootstrap|clock|ge)_compatibility$", "--output-on-failure"], check=True)
        return 0
    output = local_output(args.output)
    packet = None
    if args.action == "decode-clear":
        if args.trace is None:
            raise ValueError("decode-clear requires a native trace")
        trace = local_output(args.trace)
        if trace.stat().st_size > 8 * 1024 * 1024:
            raise ValueError("Trace exceeds size budget")
        packet = clear_packet(trace.read_text(), load_profile(ROOT / "profiles/tekken6/config/profile.json")["executable"]["sha256"])
    output.mkdir(parents=True, exist_ok=False)
    executable = build / ("bin/Release/azazel6_portablekit_probe.exe" if sys.platform == "win32"
                          else "bin/azazel6_portablekit_probe")
    if packet is not None:
        packet_path = output / "clear.packet"
        packet_path.write_text(packet)
        run = subprocess.run([str(executable), str(args.input.resolve()), "--decode-clear", str(packet_path)],
                             env=probe_environment(output), capture_output=True, text=True, timeout=15, check=True)
        result = json.loads(run.stdout)
        result["portablekit_revision"] = revision
        (output / "vertices.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"Decoded {len(result['vertices'])} vertices; no drawing or GE completion: {output / 'vertices.json'}")
        return 0
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
