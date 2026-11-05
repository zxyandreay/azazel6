"""Run the entry experiment with a hard wall-clock bound and retain exit evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from analyze import ROOT, local_output
from elf import load_profile, validate


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
        if args.iso.stat().st_size != profile['media']['size']:
            raise ValueError('Unsupported ISO size')
        with args.iso.open('rb') as source:
            digest = hashlib.sha256()
            for block in iter(lambda: source.read(1024 * 1024), b''):
                digest.update(block)
        if digest.hexdigest() != profile['media']['sha256']:
            raise ValueError('Unsupported ISO SHA-256')
    output = local_output(args.output)
    output.mkdir(parents=True, exist_ok=True)
    result = {"status": "failed", "timeout_seconds": 15}
    command = [str(args.binary.resolve()), str(args.input.resolve())]
    if args.iso:
        command.extend(['--iso', str(args.iso.resolve())])
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
            process = subprocess.run(command,
                                     stdout=log, stderr=subprocess.STDOUT, timeout=15)
            result["returncode"] = process.returncode
            if process.returncode == 3:
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
