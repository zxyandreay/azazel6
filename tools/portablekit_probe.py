"""Bounded optional PortableKit comparison; never replaces the strict AOT probe."""
import argparse
import hashlib
import json
import os
import re
import struct
from pathlib import Path
import subprocess
import sys
import time

from analyze import ROOT, local_output
from elf import load_profile, validate
from run_probe import run_mounted


def checked_source(source):
    expected = json.loads((ROOT / "dependencies.json").read_text())["portablekit"]["revision"]
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip()
    if actual != expected or dirty:
        raise ValueError("PortableKit must be the unmodified pinned revision; checkout left untouched")
    return actual


def probe_environment(output, build=None):
    # Do not inherit upstream diagnostic switches, acceleration or user settings.
    env = {k: v for k, v in os.environ.items()
           if not k.upper().startswith(("AZAZEL6_EVAL_", "PSPRECOMP_"))}
    for name in ("STRICT_HLE", "NO_RENDER", "NO_AUDIO", "NO_BUSY_CLOCK"):
        env["AZAZEL6_EVAL_" + name] = "1"
    env["AZAZEL6_EVAL_FAST_LOADING"] = "0"
    env["AZAZEL6_EVAL_LIST_STUBS"] = "1"
    env["AZAZEL6_EVAL_DATA_DIR"] = str(output)
    env["PSPRECOMP_TRACE_ON_ERROR"] = "1"
    env["PSPRECOMP_HLE_HISTOGRAM"] = "1"
    if build is not None:
        # PortableKit places pinned FFmpeg DLLs in bin/, beside bin/Release/.
        env["PATH"] = str(build / "bin") + os.pathsep + env.get("PATH", "")
    return env


def validate_media(path, profile, deadline):
    """Preflight the exact disc before creating output or starting the guest."""
    if path.stat().st_size != profile['media']['size']:
        raise ValueError('Unsupported ISO size')
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            if time.monotonic() >= deadline:
                raise ValueError('ISO preflight validation timed out')
            digest.update(block)
    if digest.hexdigest() != profile['media']['sha256']:
        raise ValueError('Unsupported ISO SHA-256')
    return digest.hexdigest()


def summarize_bmp(path):
    """Count RGB pixels from a bounded uncompressed renderer capture."""
    if not path.exists():
        return {"status": "unavailable"}
    if path.stat().st_size > 16 * 1024 * 1024:
        return {"status": "invalid"}
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        return {"status": "invalid"}
    offset = struct.unpack_from('<I', data, 10)[0]
    dib = struct.unpack_from('<I', data, 14)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    planes, bits = struct.unpack_from('<HH', data, 26)
    compression = struct.unpack_from('<I', data, 30)[0]
    if dib < 40 or not (0 < width <= 4096 and 0 < abs(height) <= 4096) or planes != 1 or bits not in (24, 32) or compression:
        return {"status": "invalid"}
    pixel_bytes = bits // 8
    stride = (width * pixel_bytes + 3) & ~3
    if offset < 54 or offset + stride * abs(height) != len(data):
        return {"status": "invalid"}
    nonblack = 0
    for row in range(abs(height)):
        start = offset + row * stride
        nonblack += sum(any(data[start + x * pixel_bytes:start + x * pixel_bytes + 3]) for x in range(width))
    return {"status": "captured", "width": width, "height": abs(height), "nonblack_pixels": nonblack,
            "sha256": hashlib.sha256(data).hexdigest(), "scope": "Diagnostic Vulkan target; no title or gameplay inference"}


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
    p.add_argument("action", choices=["build", "run", "render-game", "decode-clear", "render-test"])
    p.add_argument("input", nargs="?", type=Path)
    p.add_argument("--source", type=Path, default=ROOT / "local/deps/portablekit")
    p.add_argument("--build", type=Path, default=ROOT / "local/build/portablekit-probe")
    p.add_argument("--output", type=Path, default=ROOT / "local/portablekit-probe")
    p.add_argument("--trace", type=Path, help="Local native execution.log for decode-clear")
    p.add_argument("--iso", type=Path, help="Exact read-only ULUS10466 disc for a bounded run")
    p.add_argument("--ffmpeg", choices=["OFF", "system", "bundled"], default="OFF",
                   help="Build-only real decoder selection; default OFF")
    p.add_argument("--renderer", action="store_true", help="Build-only Vulkan renderer selection")
    p.add_argument("--dispatches", type=int, choices=[100000, 250000], default=100000,
                   help="Bounded visual dispatch budget; default 100000")
    args = p.parse_args(argv)
    if args.iso is not None and args.action not in ("run", "render-game"):
        raise ValueError("--iso is only valid for a game run")
    if args.action == "render-game" and args.iso is None:
        raise ValueError("Game rendering requires exact ISO")
    if args.ffmpeg != "OFF" and args.action != "build":
        raise ValueError("--ffmpeg is only valid for build")
    if args.renderer and args.action != "build":
        raise ValueError("--renderer is only valid for build")
    if args.dispatches != 100000 and args.action != "render-game":
        raise ValueError("--dispatches is only valid for game rendering")
    profile = load_profile(ROOT / "profiles/tekken6/config/profile.json")
    if args.action in ("run", "render-game", "decode-clear"):
        if args.input is None:
            raise ValueError("Run requires an exact supported executable")
        validate(args.input, profile)
    validation_deadline = time.monotonic() + 240 if args.iso else None
    media_sha = validate_media(args.iso, profile, validation_deadline) if args.iso else None
    revision = checked_source(args.source)
    build = local_output(args.build)
    if args.action == "build":
        subprocess.run(["cmake", "-S", str(ROOT / "profiles/tekken6/portablekit"), "-B", str(build),
                        "-DAZAZEL6_PORTABLEKIT_SOURCE=" + str(args.source.resolve()),
                        "-DAZAZEL6_PORTABLEKIT_FFMPEG=" + args.ffmpeg,
                        "-DAZAZEL6_PORTABLEKIT_RENDERER=" + ("ON" if args.renderer else "OFF")], check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Release", "--parallel", "2",
                        "--target", "azazel6_portablekit_probe", "azazel6_portablekit_bootstrap_tests",
                        "azazel6_portablekit_clock_tests", "azazel6_portablekit_wakeup_tests"], check=True)
        subprocess.run(["ctest", "--test-dir", str(build), "-C", "Release", "-R",
                        "^portablekit_(bootstrap|clock|ge|wakeup)_compatibility$", "--output-on-failure",
                        "--timeout", "30"], env=probe_environment(build, build), check=True)
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
    if args.action == "render-test":
        env = probe_environment(output, build)
        del env["AZAZEL6_EVAL_NO_RENDER"]
        result = {"scope": "SYNTHETIC Vulkan GE readback only; no game execution",
                  "portablekit_revision": revision, "timeout_seconds": 30, "status": "failed"}
        with (output / "renderer.log").open("w", encoding="utf-8") as log:
            try:
                run = subprocess.run([str(executable), "--test-renderer"], env=env,
                                     stdout=log, stderr=subprocess.STDOUT, timeout=30)
                result["returncode"] = run.returncode
                if run.returncode == 0:
                    result["status"] = "synthetic_contract_passed"
            except subprocess.TimeoutExpired:
                result["status"] = "timeout"
        (output / "renderer.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result))
        return 0 if result["status"] == "synthetic_contract_passed" else 1
    if packet is not None:
        packet_path = output / "clear.packet"
        packet_path.write_text(packet)
        run = subprocess.run([str(executable), str(args.input.resolve()), "--decode-clear", str(packet_path)],
                             env=probe_environment(output, build), capture_output=True, text=True, timeout=15, check=True)
        result = json.loads(run.stdout)
        result["portablekit_revision"] = revision
        (output / "vertices.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"Decoded {len(result['vertices'])} vertices; no drawing or GE completion: {output / 'vertices.json'}")
        return 0
    result = {"scope": ("UNVERIFIED game Vulkan diagnostic, not title or native AOT milestone evidence"
                         if args.action == "render-game" else
                         "UNVERIFIED upstream compatibility experiment, not native AOT milestone evidence"),
              "portablekit_revision": revision,
              "timeout_seconds": 30 if args.action == "render-game" else 15,
              "max_dispatches": args.dispatches if args.action == "render-game" else 100000,
              "status": "failed"}
    command = [str(executable), str(args.input.resolve()), str(output)]
    if args.iso:
        command.extend(["--iso", str(args.iso.resolve()), "--media-gate", "stdin"])
        result.update(media_sha256=media_sha, media_validation_timeout_seconds=240)
    capture = output / "game-frame.bmp"
    if args.action == "render-game":
        capture = local_output(capture)
        command.extend(["--capture-game-frame", str(capture), "--dispatches", str(args.dispatches)])
        env = probe_environment(output, build)
        del env["AZAZEL6_EVAL_NO_RENDER"]
    else:
        env = probe_environment(output, build)
    with (output / "execution.log").open("w", encoding="utf-8") as log:
        try:
            if args.iso:
                code, timeout_phase = run_mounted(command, log, validation_deadline,
                                                  execution_seconds=result["timeout_seconds"], env=env)
            else:
                code = subprocess.run(command, env=env, stdout=log,
                                      stderr=subprocess.STDOUT, timeout=15).returncode
                timeout_phase = None
            if timeout_phase:
                result.update(status="timeout", timeout_phase=timeout_phase)
            else:
                result["returncode"] = code
            if code == 3:
                result["status"] = "research_boundary"
        except subprocess.TimeoutExpired:
            result["status"] = "timeout"
    if args.action == "render-game":
        result["frame"] = summarize_bmp(capture) if "returncode" in result else {"status": "unavailable"}
    (output / "execution.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["status"] == "research_boundary" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"portablekit_probe: {error}", file=sys.stderr)
        sys.exit(1)
