"""Synthetic path-bridge checks; no FFmpeg source is used."""

import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from analyze import ROOT
from ffmpeg_windows_make import patch_generated_paths, repair_installed_headers


@unittest.skipUnless(os.name == "nt", "Windows-only FFmpeg path bridge")
class WindowsMakeBridgeTests(unittest.TestCase):
    def test_generated_paths_only(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "local") as directory:
            parent = Path(directory) / "ffmpeg"
            source = parent / "ffmpeg-7.1.5"
            build = parent / "build"
            source.mkdir(parents=True)
            (build / "ffbuild").mkdir(parents=True)
            msys = "/" + source.drive[0].lower() + "/" + source.as_posix()[3:]
            (build / "Makefile").write_text(f"include {msys}/Makefile\n")
            (build / "ffbuild/config.mak").write_text(f"SRC_PATH={msys}\n")
            patch_generated_paths(build)
            self.assertEqual((build / "Makefile").read_text(), f"include {source.as_posix()}/Makefile\n")
            self.assertEqual((build / "ffbuild/config.mak").read_text(), f"SRC_PATH={source.as_posix()}\n")
            patch_generated_paths(build)
            self.assertFalse(list(source.iterdir()))

    def test_outside_local_refused(self):
        with self.assertRaises(ValueError):
            patch_generated_paths(ROOT)
        with self.assertRaises(ValueError):
            repair_installed_headers(ROOT, ROOT / "local")

    def test_missing_installed_header_is_copied_from_local_source(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "local") as directory:
            parent = Path(directory) / "ffmpeg"
            source = parent / "ffmpeg-7.1.5/libavutil"
            build = parent / "build"
            installed = parent / "install"
            source.mkdir(parents=True)
            build.mkdir()
            (source / "samplefmt.h").write_text("public sample format\n")
            self.assertEqual(repair_installed_headers(build, installed), 1)
            self.assertEqual((installed / "include/libavutil/samplefmt.h").read_text(),
                             "public sample format\n")
            self.assertEqual(repair_installed_headers(build, installed), 0)
