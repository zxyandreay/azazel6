"""Run the entry experiment with a hard wall-clock bound and retain exit evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
from analyze import ROOT, local_output
from elf import load_profile, validate


def run_mounted(command, log, deadline, execution_seconds=15):
    """The child flushes a marker and blocks on stdin before any guest execution."""
    phase = "media_validation"
    with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT) as child:
        try:
            with open(log.name, "rb") as reader:
                while time.monotonic() < deadline:
                    reader.seek(0)
                    if b"validated_media_waiting" in reader.read(1024).splitlines():
                        phase = "execution"
                        child.stdin.write(b"G")
                        child.stdin.flush()
                        return child.wait(timeout=execution_seconds), None
                    if child.poll() is not None:
                        return 1, None
                    time.sleep(0.02)
            return None, phase
        except subprocess.TimeoutExpired:
            return None, phase
        finally:
            if child.poll() is None:
                child.kill()
            child.wait()


def summarize_framebuffer(path):
    """Describe pixels only; neither nonblack pixels nor capture imply game boot."""
    path = local_output(path)
    if not path.exists():
        return {"status": "unavailable"}
    if path.stat().st_size != 480 * 272 * 4:
        return {"status": "invalid", "error": "Expected exactly 480x272 RGBA bytes"}
    data = path.read_bytes()
    rgb = b"".join(data[index:index + 3] for index in range(0, len(data), 4))
    ppm = local_output(path.with_suffix(".ppm"))
    ppm.write_bytes(b"P6\n480 272\n255\n" + rgb)
    return {"status": "captured", "width": 480, "height": 272,
            "rgba_file": path.name, "preview_file": ppm.name,
            "sha256": hashlib.sha256(data).hexdigest(),
            "nonblack_pixels": sum(any(rgb[i:i + 3]) for i in range(0, len(rgb), 3)),
            "scope": "Diagnostic current scanout; no title or gameplay inference"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "local/entry-probe")
    parser.add_argument("--iso", type=Path, help="Explicit read-only ULUS10466 disc image")
    parser.add_argument("--capture-framebuffer", action="store_true",
                        help="Retain current 480x272 scanout as local RGBA/PPM, including failed runs")
    args = parser.parse_args(argv)
    profile = load_profile(ROOT / "profiles/tekken6/config/profile.json")
    validate(args.input, profile)
    if args.iso:
        validation_seconds = 240
        validation_deadline = time.monotonic() + validation_seconds
        if args.iso.stat().st_size != profile['media']['size']:
            raise ValueError('Unsupported ISO size')
        with args.iso.open('rb') as source:
            digest = hashlib.sha256()
            for block in iter(lambda: source.read(1024 * 1024), b''):
                if time.monotonic() >= validation_deadline:
                    raise ValueError("ISO preflight validation timed out")
                digest.update(block)
        if digest.hexdigest() != profile['media']['sha256']:
            raise ValueError('Unsupported ISO SHA-256')
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=True)
    result = {"status": "failed", "timeout_seconds": 15}
    command = [str(args.binary.resolve()), str(args.input.resolve())]
    if args.iso:
        command.extend(['--media-gate', 'stdin', '--iso', str(args.iso.resolve())])
        result['media_validation_timeout_seconds'] = validation_seconds
        result['media_sha256'] = digest.hexdigest()
    if args.capture_framebuffer:
        # Refuse escaping symlinks; remove old artifacts so timeout/off cannot
        # accidentally report a previous run's pixels as fresh evidence.
        capture = local_output(output / "framebuffer.rgba")
        preview = local_output(output / "framebuffer.ppm")
        capture.unlink(missing_ok=True)
        preview.unlink(missing_ok=True)
        command.extend(["--framebuffer-rgba", str(capture)])
    with (output / "execution.log").open("w", encoding="utf-8") as log:
        try:
            if args.iso:
                code, timeout_phase = run_mounted(command, log, validation_deadline)
            else:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=15).returncode
                timeout_phase = None
            if timeout_phase:
                result.update(status="timeout", timeout_phase=timeout_phase)
            else:
                result["returncode"] = code
            if code == 3:
                result["status"] = "research_boundary"
        except subprocess.TimeoutExpired:
            result["status"] = "timeout"
    if args.capture_framebuffer:
        result["framebuffer"] = (summarize_framebuffer(capture) if "returncode" in result
                                 else {"status": "unavailable"})
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
