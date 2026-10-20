import unittest
from test_elf import fixture
from elf import Elf
from entry_probe import ge_list_spans


class GeSpanTests(unittest.TestCase):
    def setUp(self):
        self.elf = Elf.parse(fixture())

    def test_file_backed_and_optional(self):
        self.assertEqual(ge_list_spans(self.elf, {}), [])
        self.assertEqual(ge_list_spans(self.elf, {"ge_list_spans": [
            {"begin": "0x08804000", "end": "0x08804020"}]}), [(0x08804000, 0x08804020)])

    def test_bounds_and_shape(self):
        for row in [
            {"begin": "0x08804001", "end": "0x08804020"},
            {"begin": "0x08804000", "end": "0x08804024"},  # BSS is not a validated list.
            {"begin": "0x08804000", "end": "0x08804000"},
            {"begin": "0x08804000", "end": "0x08809000"},
            {"begin": "0xFFFFFFFC", "end": "0x100000004"},
            {"begin": 0x08804000, "end": "0x08804020"},
            {"begin": "0x08804000"},
        ]:
            with self.subTest(row=row), self.assertRaises(ValueError):
                ge_list_spans(self.elf, {"ge_list_spans": [row]})

    def test_overlap_and_capacity(self):
        row = {"begin": "0x08804000", "end": "0x08804020"}
        for rows in [[row, row], [row] * 65, {}]:
            with self.assertRaises(ValueError):
                ge_list_spans(self.elf, {"ge_list_spans": rows})
