import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from elf import Elf, HEADER, PROGRAM, ValidationError, load_profile, validate
from analyze import local_output


def fixture():
    data = bytearray(0x120)
    HEADER.pack_into(data, 0, b"\x7fELF\x01\x01\x01" + bytes(9), 2, 8, 1,
                     0x08804000, 52, 0, 0, 52, 32, 1, 0, 0, 0)
    PROGRAM.pack_into(data, 52, 1, 0x100, 0x08804000, 0, 0x20, 0x40, 5, 0x100)
    struct.pack_into("<II", data, 0x100, 0x03E00008, 0)  # synthetic jr ra; nop
    return data


class ElfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "local").mkdir(exist_ok=True)

    def test_synthetic_valid(self):
        elf = Elf.parse(fixture())
        self.assertEqual(elf.entry, 0x08804000)
        self.assertEqual(elf.report()["size"], 0x120)

    def test_file_guest_round_trip(self):
        elf = Elf.parse(fixture())
        for offset in range(0x100, 0x120):
            self.assertEqual(elf.guest_to_offset(elf.offset_to_guest(offset)), offset)

    def test_bss_unmapped_overflow_and_cross_boundary(self):
        elf = Elf.parse(fixture())
        for address, size in [(0x08804020, 1), (0, 4), (0x0880401F, 2),
                              (0xFFFFFFFF, 2), (-1, 1), (0x08804000, 0)]:
            with self.subTest(address=address, size=size), self.assertRaises(ValidationError):
                elf.guest_to_offset(address, size)
        for offset in [0, 0x120, -1]:
            with self.assertRaises(ValidationError):
                elf.offset_to_guest(offset)

    def test_wrong_format(self):
        for offset, value in [(0, 0), (4, 2), (5, 2), (6, 0), (16, 3), (18, 3)]:
            data = fixture()
            data[offset] = value
            with self.subTest(offset=offset), self.assertRaises(ValidationError):
                Elf.parse(data)

    def test_header_and_segment_bounds(self):
        for offset, value in [(28, 0xFFFF), (52 + 4, 0xFFFF), (52 + 16, 0x41),
                              (52 + 8, 0xFFFFFFF0), (52 + 28, 3), (24, 0x08804021),
                              (52 + 24, 4), (32, 100)]:
            data = fixture()
            struct.pack_into("<I", data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValidationError):
                Elf.parse(data)

    def test_all_truncations_rejected(self):
        data = fixture()
        for length in range(len(data)):
            with self.subTest(length=length), self.assertRaises(ValidationError):
                Elf.parse(data[:length])

    def test_overlapping_segments(self):
        data = fixture()
        struct.pack_into("<H", data, 44, 2)
        data[84:116] = data[52:84]
        with self.assertRaisesRegex(ValidationError, "Overlapping"):
            Elf.parse(data)

    def test_version_and_hash(self):
        data = fixture()
        report = Elf.parse(data).report()
        profile = {"executable": report}
        with tempfile.TemporaryDirectory(dir=ROOT / "local") as folder:
            path = Path(folder) / "synthetic.elf"
            path.write_bytes(data)
            self.assertEqual(validate(path, profile).entry, 0x08804000)
            data[-1] = 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValidationError, "sha256 mismatch"):
                validate(path, profile)
            with self.assertRaises(FileNotFoundError):
                validate(Path(folder) / "missing", profile)

    def test_profile(self):
        path = ROOT / "profiles/tekken6/config/profile.json"
        profile = load_profile(path)
        self.assertEqual(profile["game_id"], "ULUS10466")
        mutations = [("game_id", "ULES00000"), ("schema_version", 2), ("generation", {}),
                     ("executable", {"size": 1})]
        with tempfile.TemporaryDirectory(dir=ROOT / "local") as folder:
            for key, value in mutations:
                changed = copy.deepcopy(profile)
                changed[key] = value
                bad = Path(folder) / "profile.json"
                bad.write_text(json.dumps(changed))
                with self.assertRaises(ValidationError):
                    load_profile(bad)

    def test_local_output_confinement(self):
        self.assertEqual(local_output(ROOT / "local/test"), (ROOT / "local/test").resolve())
        for path in [ROOT / "generated", ROOT / "local/../tests", ROOT.parent]:
            with self.assertRaises(ValidationError):
                local_output(path)


if __name__ == "__main__":
    unittest.main()
