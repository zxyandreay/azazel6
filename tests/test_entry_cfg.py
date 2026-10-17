import struct
import unittest
from test_elf import fixture
from elf import Elf, ValidationError
from entry_cfg import discover_entry


def program(words):
    data = fixture()
    data[0x100:] = struct.pack('<' + 'I' * len(words), *words)
    struct.pack_into('<II', data, 52 + 16, len(words) * 4, len(words) * 4)
    return Elf.parse(data)


class EntryCfgTests(unittest.TestCase):
    def test_return_and_delay(self):
        cfg = discover_entry(program([0x03E00008, 0]))
        self.assertEqual(cfg['ranges'], [[0x08804000, 8]])

    def test_call_resumes_but_callee_is_not_followed(self):
        cfg = discover_entry(program([0x0C000040, 0, 0x03E00008, 0]))
        self.assertEqual(cfg['instruction_words'], 4)
        self.assertEqual(cfg['calls'], {0x08804000: 0x100})

    def test_branch_covers_both_paths_and_skips_dead_data(self):
        cfg = discover_entry(program([0x10000005, 0, 0x03E00008, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0x03E00008, 0]))
        self.assertEqual(cfg['ranges'], [[0x08804000, 16], [0x08804018, 8]])

    def test_loop_is_bounded_by_discovery(self):
        cfg = discover_entry(program([0x1000FFFF, 0, 0x03E00008, 0]))
        self.assertEqual(cfg['instruction_words'], 4)

    def test_rejects_unknown_targets_delay_slots_and_truncation(self):
        for words in [[0x00200008, 0], [0x03E00008], [0x10000000, 0, 0x03E00008, 0],
                      [0x03E00008, 0x03E00008], [0x1000007F, 0, 0x03E00008, 0],
                      [0x04110000, 0], [0x0000000C, 0]]:
            with self.subTest(words=words), self.assertRaises(ValidationError):
                discover_entry(program(words))

    def test_instruction_budget(self):
        with self.assertRaisesRegex(ValidationError, 'budget'):
            discover_entry(program([0, 0, 0x03E00008, 0]), max_instructions=2)


if __name__ == '__main__':
    unittest.main()
