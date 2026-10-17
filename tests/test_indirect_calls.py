import unittest
from elf import ValidationError
from entry_cfg import discover_entry
from indirect_calls import constructor_maps, guard_generated
from test_entry_cfg import program

BASE = 0x08804000


class IndirectCallTests(unittest.TestCase):
    def descriptor(self, **changes):
        return dict(site=hex(BASE), register=4, table_end=hex(BASE + 24), max_entries=8) | changes

    def fixture(self, target=BASE + 24):
        return program([0x0080F809, 0, 0x03E00008, 0, 0xFFFFFFFF, target, 0x03E00008, 0])

    def test_bounded_table_and_call_continuation(self):
        elf = self.fixture()
        maps = constructor_maps(elf, [self.descriptor()])
        self.assertEqual(maps[BASE]['targets'], [BASE + 24])
        self.assertEqual(maps[BASE]['table_start'], BASE + 16)
        cfg = discover_entry(elf, indirect_calls={BASE: maps[BASE]['targets']})
        self.assertEqual(cfg['ranges'], [[BASE, 16]])
        self.assertEqual(cfg['indirect_calls'], {BASE: [BASE + 24]})
        with self.assertRaises(ValidationError):
            discover_entry(elf, indirect_calls={BASE: []})

    def test_descriptor_mismatch_bounds_and_sentinel(self):
        for changes in ({'register': 5}, {'register': 0}, {'max_entries': 0}, {'max_entries': 1025},
                        {'max_entries': 1}, {'site': hex(BASE + 1)}, {'table_end': hex(BASE + 20)},
                        {'table_end': '0'}, {'table_end': hex(BASE + 100)}, {'table_end': hex(BASE + 23)}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                constructor_maps(self.fixture(), [self.descriptor(**changes)])
        with self.assertRaises(ValidationError):
            constructor_maps(self.fixture(), [self.descriptor(), self.descriptor()])

    def test_invalid_targets(self):
        for target in (0, BASE + 1, BASE + 32, 0xFFFFFFFC, 0x88804000):
            with self.subTest(target=target), self.assertRaises(ValidationError):
                constructor_maps(self.fixture(target), [self.descriptor()])

    def test_guard_injection_must_match_exactly_once(self):
        maps = constructor_maps(self.fixture(), [self.descriptor()])
        label = f'L_{BASE:08X}:\n'
        generated = guard_generated(label + '    jump_target = ctx.gpr[4];\n', maps)
        self.assertIn('ctx.gpr[4], {0x08804018u}', generated)
        self.assertLess(generated.index('guard_indirect_call'), generated.index('jump_target'))
        for source in ('', label + label):
            with self.assertRaises(ValidationError):
                guard_generated(source, maps)


if __name__ == '__main__':
    unittest.main()
