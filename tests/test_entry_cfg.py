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

    def test_proven_dead_indirect_edge(self):
        # t9 = 0; branch around a null weak-symbol jump. No guessed target map.
        words = [0x3C020000, 0x24590000, 0x13200003, 0,
                 0x03200008, 0, 0x03E00008, 0]
        cfg = discover_entry(program(words), constant_branches=True)
        self.assertEqual(cfg['ranges'], [[0x08804000, 16], [0x08804018, 8]])
        with self.assertRaises(ValidationError):
            discover_entry(program(words))

    def test_constants_wrap_zero_extend_and_preserve_zero(self):
        for words in ([0x3C02FFFF, 0x3442FFFF, 0x24420001],  # FFFFFFFF + 1
                      [0x3C020000, 0x34428000, 0x24428000],  # ORI zero extension
                      [0x3C001234, 0x24020000]):  # writes to r0 ignored
            cfg = discover_entry(program([*words, 0x10400003, 0, 0x03200008, 0, 0x03E00008, 0]),
                                 constant_branches=True)
            self.assertNotIn(0x08804000 + (len(words) + 2) * 4, cfg['instruction_addresses'])

    def test_delay_executes_after_condition_and_likely_annuls(self):
        # Branch uses the old zero value even though the delay slot sets it to 1.
        cfg = discover_entry(program([0x24020000, 0x10400003, 0x24020001,
                                      0x03200008, 0, 0x03E00008, 0]), constant_branches=True)
        self.assertNotIn(0x0880400C, cfg['instruction_addresses'])
        # Not-taken BEQL must not set v0 to 1; the next branch remains taken.
        cfg = discover_entry(program([0x24020000, 0x24030001, 0x50430005, 0x24020001,
                                      0x10400003, 0, 0x03200008, 0, 0x03E00008, 0]), constant_branches=True)
        self.assertNotIn(0x08804018, cfg['instruction_addresses'])

    def test_unknown_instruction_and_call_invalidate_facts(self):
        for middle in ([0x8C420000], [0x0C000040, 0]):
            with self.assertRaises(ValidationError):
                discover_entry(program([0x24020000, *middle, 0x10400003, 0,
                                        0x03200008, 0, 0x03E00008, 0]), constant_branches=True)

    def test_conflicting_join_revisits_branch(self):
        # Unknown a0 chooses zero or one; both reach a conditional indirect edge.
        words = [0x10800004, 0, 0x24020001, 0x10000002, 0, 0x24020000,
                 0x10400003, 0, 0x03200008, 0, 0x03E00008, 0]
        with self.assertRaises(ValidationError):
            discover_entry(program(words), constant_branches=True)

    def test_tail_import_does_not_inline_unpatched_stub(self):
        stub = 0x08804008
        elf = program([0x08000000 | (stub >> 2), 0, 0x03E00008, 0])
        cfg = discover_entry(elf, import_stubs=[stub])
        self.assertEqual(cfg['ranges'], [[0x08804000, 8]])
        self.assertEqual(cfg['tail_calls'], {0x08804000: stub})
        self.assertNotIn(stub, cfg['instruction_addresses'])
        for invalid in ['0x08804008', -4, 0x100000000, stub + 1]:
            with self.subTest(stub=invalid), self.assertRaises(ValidationError):
                discover_entry(elf, import_stubs=[invalid])


if __name__ == '__main__':
    unittest.main()
