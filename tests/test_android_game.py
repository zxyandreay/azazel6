"""Synthetic refusal contracts for the genuine-game Android packager."""

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from android_game import validate_build_cache, verify_file_hash
from tempfile import TemporaryDirectory


class AndroidGamePackagingTests(unittest.TestCase):
    def test_all_required_runtime_components(self):
        required = ["AZAZEL6_PORTABLEKIT_FFMPEG:STRING=bundled",
                    "PORTABLEKIT_ANDROID_APP:BOOL=ON",
                    "AZAZEL6_PORTABLEKIT_RENDERER:BOOL=ON",
                    "AZAZEL6_PORTABLEKIT_AOT_SOURCE:FILEPATH=D:/repo/local/generated_entry.cpp"]
        validate_build_cache("\n".join(required))
        for line in required:
            with self.subTest(line=line), self.assertRaises(ValueError):
                validate_build_cache("\n".join(item for item in required if item != line))

    def test_decoder_disabled_and_empty_aot_rejected(self):
        common = "PORTABLEKIT_ANDROID_APP:BOOL=ON\nAZAZEL6_PORTABLEKIT_RENDERER:BOOL=ON\n"
        with self.assertRaisesRegex(ValueError, "FFMPEG"):
            validate_build_cache(common + "AZAZEL6_PORTABLEKIT_FFMPEG:STRING=OFF\n")
        with self.assertRaisesRegex(ValueError, "AOT source"):
            validate_build_cache(common + "AZAZEL6_PORTABLEKIT_FFMPEG:STRING=bundled\n"
                                 "AZAZEL6_PORTABLEKIT_AOT_SOURCE:FILEPATH=\n")

    def test_font_hash_admission(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "font.otf"
            path.write_bytes(b"font fixture")
            self.assertEqual(path, verify_file_hash(path, "d963d9d62922646a12c117e01b9c258ced855a4e6da7a584956769781bf3030d"))
            with self.assertRaisesRegex(ValueError, "Unexpected SHA-256"):
                verify_file_hash(path, "0" * 64)
