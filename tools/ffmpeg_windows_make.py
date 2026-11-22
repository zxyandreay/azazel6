"""Bridge FFmpeg's Git Bash source paths to native Windows GNU Make.

PortableKit invokes this as PORTABLEKIT_MAKE during an Android cross-build on
Windows. It only edits ignored FFmpeg build files, never the pinned source.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys

from analyze import ROOT


def patch_generated_paths(build):
    build = Path(build).resolve()
    if os.name != "nt" or not build.is_relative_to(ROOT / "local") or build.name != "build" or build.parent.name != "ffmpeg":
        raise ValueError("Expected an ignored local FFmpeg Android build directory")
    source = build.parent / "ffmpeg-7.1.5"
    if not source.is_dir() or not source.drive:
        raise ValueError("Pinned FFmpeg source directory is missing")
    msys = "/" + source.drive[0].lower() + "/" + source.as_posix()[3:]
    windows = source.as_posix()
    count = 0
    for path in (build / "Makefile", build / "ffbuild/config.mak"):
        original = path.read_text(encoding="utf-8")
        replaced = original.replace(msys, windows)
        if original != replaced:
            path.write_text(replaced, encoding="utf-8")
            count += 1
    if count == 0 and windows not in (build / "Makefile").read_text(encoding="utf-8"):
        raise ValueError("FFmpeg generated Makefile source path was not recognized")


def repair_installed_headers(build, installed):
    """Complete Win32 Make's truncated avutil header copy in local output."""
    build, installed = Path(build).resolve(), Path(installed).resolve()
    if (not build.is_relative_to(ROOT / "local") or
            not installed.is_relative_to(ROOT / "local") or
            build.name != "build" or build.parent.name != "ffmpeg"):
        raise ValueError("FFmpeg header repair requires ignored local build/install paths")
    destination = installed / "include/libavutil"
    if (destination / "samplefmt.h").is_file():
        return 0
    source = build.parent / "ffmpeg-7.1.5/libavutil"
    headers = sorted(source.glob("*.h")) + sorted((build / "libavutil").glob("*.h"))
    if not (source / "samplefmt.h").is_file() or not headers:
        raise ValueError("Pinned FFmpeg public headers are missing")
    destination.mkdir(parents=True, exist_ok=True)
    for header in headers:
        shutil.copyfile(header, destination / header.name)
    if not (destination / "samplefmt.h").is_file():
        raise ValueError("FFmpeg header repair failed")
    return len(headers)


def main():
    patch_generated_paths(Path.cwd())
    real_make = shutil.which("make.exe")
    if not real_make:
        raise ValueError("Native Windows GNU Make is missing")
    git_tools = Path("C:/Program Files/Git/usr/bin")
    if not (git_tools / "touch.exe").is_file() or not (git_tools / "mkdir.exe").is_file():
        raise ValueError("Git Bash POSIX build tools are missing")
    environment = os.environ.copy()
    environment["PATH"] = str(git_tools) + os.pathsep + environment["PATH"]
    result = subprocess.call([real_make, *sys.argv[1:]], env=environment)
    if result == 0 and "install" in sys.argv[1:]:
        destdir = next((item.partition("=")[2] for item in sys.argv[1:]
                        if item.startswith("DESTDIR=")), None)
        if not destdir:
            raise ValueError("FFmpeg install requires an explicit local DESTDIR")
        copied = repair_installed_headers(Path.cwd(), Path(destdir) / "ffmpeg")
        if copied:
            print(f"Completed missing local libavutil headers: {copied}")
    return result


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"ffmpeg_windows_make: {error}", file=sys.stderr)
        sys.exit(1)
