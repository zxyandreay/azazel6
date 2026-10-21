"""Synthetic ELF packaging guards; no Android execution is implied."""
import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from android_host import elf_alignment


def library():
    data = bytearray(128)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HH", data, 16, 3, 183)
    struct.pack_into("<H", data, 52, 64)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HH", data, 54, 56, 1)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, 128, 128, 16384)
    return data


class AndroidPackagingTests(unittest.TestCase):
    def test_aligned_arm64(self):
        elf_alignment(library())

    def test_emulator_abi_and_mismatch(self):
        data = library()
        struct.pack_into("<H", data, 18, 62)
        elf_alignment(data, "x86_64")
        with self.assertRaises(ValueError):
            elf_alignment(data, "arm64-v8a")
        with self.assertRaises(ValueError):
            elf_alignment(library(), "x86_64")
        with self.assertRaises(ValueError):
            elf_alignment(data, "armeabi-v7a")

    def test_wrong_architecture_and_headers(self):
        for offset, fmt, value in [(16, "H", 2), (18, "H", 62), (32, "Q", 1000),
                                   (52, "H", 0), (54, "H", 48), (56, "H", 0), (64, "I", 0)]:
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data = library()
                struct.pack_into("<" + fmt, data, offset, value)
                elf_alignment(data)
        with self.assertRaises(ValueError):
            elf_alignment(b"not an ELF")

    def test_load_alignment_and_bounds(self):
        for offset, value in [(112, 4096), (112, 24576), (72, 1), (96, 129), (104, 127)]:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                data = library()
                struct.pack_into("<Q", data, offset, value)
                elf_alignment(data)


if __name__ == "__main__":
    unittest.main()
