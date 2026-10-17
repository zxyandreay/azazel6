import struct
import unittest
from test_elf import fixture
from elf import Elf, ValidationError
from psp_module import module_report


def module_fixture():
    data = fixture() + bytearray(0x200)
    struct.pack_into("<I", data, 52 + 12, 0x140)
    struct.pack_into("<II", data, 52 + 16, 0x220, 0x220)
    struct.pack_into("<HBB28sIIIII", data, 0x140, 0, 1, 1, b"synthetic", 0x08804100,
                     0x08804080, 0x08804080, 0x08804080, 0x08804094)
    struct.pack_into("<IHHBBHII", data, 0x180, 0x088040C0, 0, 0, 5, 0, 1, 0x088040D0, 0x088040E0)
    data[0x1C0:0x1C5] = b"test\0"
    struct.pack_into("<I", data, 0x1D0, 0x12345678)
    return data


class ModuleTests(unittest.TestCase):
    def export_fixture(self):
        data = module_fixture()
        struct.pack_into("<II", data, 0x140 + 36, 0x08804120, 0x08804130)
        struct.pack_into("<IHHBBHI", data, 0x220, 0, 0, 0x8000, 4, 1, 1, 0x08804140)
        struct.pack_into("<IIII", data, 0x240, 0xD632ACDB, 0x0F7C276C, 0x08804000, 0x08804160)
        struct.pack_into("<IIII", data, 0x260, 3, 32, 1024, 0x80000000)
        return data

    def test_start_thread_export(self):
        report = module_report(Elf.parse(self.export_fixture()))
        self.assertEqual(report["start_thread"], {"priority": 32, "stack_size": 1024, "attributes": 0x80000000})
        self.assertEqual(report["exports"][1]["kind"], "variable")

    def test_malformed_exports(self):
        for offset, value in [(0x260, 4), (0x22C, 0xFFFFFFFF), (0x24C, 0xFFFFFFFF)]:
            data = self.export_fixture()
            struct.pack_into("<I", data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValidationError):
                module_report(Elf.parse(data))
        data = self.export_fixture()
        data[0x228] = 5
        with self.assertRaises(ValidationError):
            module_report(Elf.parse(data))

    def test_metadata_and_import(self):
        report = module_report(Elf.parse(module_fixture()))
        self.assertEqual(report["name"], "synthetic")
        self.assertEqual(report["gp"], "0x08804100")
        self.assertEqual(report["imports"], [{"library": "test", "nid": "0x12345678", "stub": "0x088040E0"}])

    def test_reject_malformed_metadata(self):
        for offset, value in [(0x140 + 32, 0), (0x140 + 48, 0x08804084),
                              (0x180 + 12, 0xFFFFFFFC), (0x180 + 16, 0), (52 + 12, 0)]:
            data = module_fixture()
            struct.pack_into("<I", data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValidationError):
                module_report(Elf.parse(data))

    def test_descriptor_length_variables_and_name(self):
        for offset, value in [(0x188, 0), (0x188, 6), (0x189, 1), (0x1C0, 255), (0x1C0, 0)]:
            data = module_fixture()
            data[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValidationError):
                module_report(Elf.parse(data))


if __name__ == "__main__":
    unittest.main()
