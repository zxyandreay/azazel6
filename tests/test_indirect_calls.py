import struct
import unittest
from elf import Elf, ValidationError
from entry_cfg import discover_entry
from indirect_calls import constructor_maps, jump_maps, trap_maps, pointer_maps, immediate_maps, guard_generated, verify_break_stops
from test_entry_cfg import program

BASE = 0x08804000


class IndirectCallTests(unittest.TestCase):
    def test_tail_traps_stop_before_delay_without_authorizing_targets(self):
        elf = program([0x03200008, 0x24040001, 0xFFFFFFFF])
        desc = dict(site=hex(BASE), register=25)
        maps = trap_maps(elf, [desc], tail=True)
        self.assertEqual(maps[BASE]['targets'], [])
        cfg = discover_entry(elf, trapped_calls=maps)
        self.assertEqual(cfg['ranges'], [[BASE, 8]])
        self.assertEqual(cfg['trapped_calls'], [BASE])
        source = guard_generated(f'L_{BASE:08X}:\n    ctx.gpr[4] = 1;\n', maps)
        self.assertIn('ctx.gpr[25], {}', source)
        self.assertLess(source.index('guard_indirect_call'), source.index('ctx.gpr[4]'))
        for changes in ({'register': 31}, {'register': 0}, {'register': 24}, {'site': hex(BASE + 1)}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                trap_maps(elf, [desc | changes], tail=True)
        with self.assertRaises(ValidationError):
            trap_maps(elf, [desc])
        with self.assertRaises(ValidationError):
            discover_entry(elf, trapped_calls=maps, indirect_tails={BASE: [BASE + 8]})

    def test_immediate_address_pairs_and_guard(self):
        for lower in (0x24424020, 0x34424020):  # ADDIU / ORI
            elf = program([0x0080F809, 0, 0x03E00008, 0, 0x3C020880, lower, 0, 0, 0x03E00008, 0])
            desc = dict(site=hex(BASE), register=4, sources=[hex(BASE + 16)])
            mapping = immediate_maps(elf, [desc])
            self.assertEqual(mapping[BASE]['targets'], [BASE + 32])
            self.assertEqual(mapping[BASE]['sources'], [BASE + 16])
            self.assertEqual(discover_entry(elf, indirect_calls={BASE: [BASE + 32]})['ranges'], [[BASE, 16]])
            self.assertIn('ctx.gpr[4], {0x08804020u}', guard_generated(f'L_{BASE:08X}:\n', mapping))
        data = bytearray(elf.data)
        base = 0x08808000
        struct.pack_into('<I', data, 24, base)
        struct.pack_into('<I', data, 60, base)
        struct.pack_into('<II', data, 0x110, 0x3C020881, 0x24428020)
        mapping = immediate_maps(Elf.parse(data), [dict(site=hex(base), register=4, sources=[hex(base + 16)])])
        self.assertEqual(mapping[base]['targets'], [base + 32])

    def test_immediate_pairs_reject_unverified_sources_and_targets(self):
        def fixture(upper=0x3C020880, lower=0x24424020):
            return program([0x0080F809, 0, 0x03E00008, 0, upper, lower, 0, 0, 0x03E00008, 0])
        desc = dict(site=hex(BASE), register=4, sources=[hex(BASE + 16)])
        for changes in ({'register': 0}, {'register': 32}, {'register': 5}, {'site': hex(BASE + 1)},
                        {'site': '0xFFFFFFFF'}, {'sources': []}, {'sources': [hex(BASE + 16)] * 129},
                        {'sources': [hex(BASE + 16)] * 2}, {'sources': [hex(BASE + 17)]},
                        {'sources': [hex(BASE + 36)]}, {'sources': ['0x88004010']}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                immediate_maps(fixture(), [desc | changes])
        for upper, lower in ((0, 0x24424020), (0x3C000880, 0x24004020), (0x3C220880, 0x24424020),
                             (0x3C020880, 0x24824020), (0x3C020880, 0x24434020),
                             (0x3C020880, 0x20424020), (0x3C020880, 0x24424021),
                             (0x3C020880, 0x24424028), (0x3C02FFFF, 0x2442FFFF)):
            with self.subTest(upper=upper, lower=lower), self.assertRaises(ValidationError):
                immediate_maps(fixture(upper, lower), [desc])
        with self.assertRaises(ValidationError):
            immediate_maps(fixture(), [desc, desc])
        data = bytearray(fixture().data)
        struct.pack_into('<I', data, 76, 6)  # no execute permission
        with self.assertRaises(ValidationError):
            immediate_maps(Elf.parse(data), [desc])

    def test_generated_break_must_stop_before_any_successor(self):
        label = f'L_{BASE:08X}:\n'
        stop = f'    rt.unsupported(0x{BASE:08X}u, 0x000001CDu, "guest break trap"); return;\n'
        verify_break_stops(label + stop, {BASE: 0x1CD})
        for source in ('', label, label + stop + label, label + stop.replace(' return;', ''),
                       label + '    ctx.gpr[2] = 0;\n' + stop,
                       label + stop.replace('000001CD', '0000000D')):
            with self.subTest(source=source), self.assertRaises(ValidationError):
                verify_break_stops(source, {BASE: 0x1CD})

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

    def test_jump_table_targets_are_followed_without_scanning_data(self):
        elf = program([0x00800008, 0, BASE + 16, BASE + 24, 0x03E00008, 0, 0x03E00008, 0])
        descriptor = dict(site=hex(BASE), register=4, table_start=hex(BASE + 8), count=2)
        maps = jump_maps(elf, [descriptor])
        cfg = discover_entry(elf, indirect_jumps={BASE: maps[BASE]['targets']})
        self.assertEqual(cfg['ranges'], [[BASE, 8], [BASE + 16, 16]])
        self.assertEqual(cfg['indirect_jumps'], {BASE: [BASE + 16, BASE + 24]})
        for changes in ({'count': 0}, {'count': 1025}, {'count': 100}, {'register': 31},
                        {'register': 5}, {'table_start': hex(BASE + 1)}, {'site': hex(BASE + 4)}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                jump_maps(elf, [descriptor | changes])
        with self.assertRaises(ValidationError):
            jump_maps(elf, [descriptor, descriptor])
        for target in (0, BASE + 1, 0xFFFFFFFF):
            bad = program([0x00800008, 0, target, BASE + 24, 0x03E00008, 0, 0x03E00008, 0])
            with self.subTest(target=target), self.assertRaises(ValidationError):
                jump_maps(bad, [descriptor])

    def test_jump_table_cannot_enter_delay_slot(self):
        with self.assertRaises(ValidationError):
            discover_entry(program([0x00800008, 0]), indirect_jumps={BASE: [BASE + 4]})

    def test_pointer_slots_are_validated_before_authorizing_calls(self):
        elf = self.fixture()
        descriptor = dict(site=hex(BASE), register=4, pointers=[hex(BASE + 20)])
        maps = pointer_maps(elf, [descriptor])
        self.assertEqual(maps[BASE]['targets'], [BASE + 24])
        self.assertEqual(maps[BASE]['pointers'], [BASE + 20])
        self.assertEqual(discover_entry(elf, indirect_calls={BASE: maps[BASE]['targets']})['ranges'], [[BASE, 16]])
        for changes in ({'register': 5}, {'register': 0}, {'site': hex(BASE + 1)},
                        {'pointers': []}, {'pointers': [hex(BASE + 20)] * 129},
                        {'pointers': [hex(BASE + 21)]}, {'pointers': ['0xFFFFFFFC']},
                        {'pointers': [hex(BASE + 16)]}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                pointer_maps(elf, [descriptor | changes])
        with self.assertRaises(ValidationError):
            pointer_maps(elf, [descriptor, descriptor])
        for target in (0, BASE + 1, BASE + 32, 0x88804000):
            with self.subTest(target=target), self.assertRaises(ValidationError):
                pointer_maps(self.fixture(target), [descriptor])

    def test_pointer_tails_preserve_function_boundaries(self):
        elf = program([0x00800008, 0, BASE + 16, 0xFFFFFFFF, 0x03E00008, 0])
        descriptor = dict(site=hex(BASE), register=4, pointers=[hex(BASE + 8)])
        maps = pointer_maps(elf, [descriptor], tail=True)
        cfg = discover_entry(elf, indirect_tails={BASE: maps[BASE]['targets']})
        self.assertEqual(cfg['ranges'], [[BASE, 8]])
        self.assertEqual(cfg['indirect_tails'], {BASE: [BASE + 16]})
        self.assertFalse(cfg['indirect_calls'])
        with self.assertRaises(ValidationError):
            pointer_maps(elf, [descriptor])
        with self.assertRaises(ValidationError):
            pointer_maps(elf, [descriptor | {'register': 31}], tail=True)
        with self.assertRaises(ValidationError):
            discover_entry(elf, indirect_tails={BASE: []})
        with self.assertRaises(ValidationError):
            discover_entry(elf, indirect_tails={BASE: [BASE + 16]}, trapped_calls=[BASE])

    def test_trap_is_terminal_and_authorizes_no_target(self):
        # The word after the delay slot deliberately has no supported CFG form.
        elf = program([0x0080F809, 0x24050001, 0x0000000C])
        descriptor = dict(site=hex(BASE), register=4)
        maps = trap_maps(elf, [descriptor])
        cfg = discover_entry(elf, trapped_calls=maps)
        self.assertEqual(cfg['ranges'], [[BASE, 8]])
        self.assertEqual(cfg['trapped_calls'], [BASE])
        self.assertFalse(cfg['indirect_calls'])
        generated = guard_generated(f'L_{BASE:08X}:\n    jump_target = ctx.gpr[4];\n', maps)
        self.assertIn('ctx.gpr[4], {}', generated)
        self.assertLess(generated.index('guard_indirect_call'), generated.index('jump_target'))
        for changes in ({'register': 0}, {'register': 32}, {'register': 5},
                        {'site': hex(BASE + 1)}, {'site': hex(BASE + 100)}):
            with self.subTest(changes=changes), self.assertRaises(ValidationError):
                trap_maps(elf, [descriptor | changes])
        with self.assertRaises(ValidationError):
            trap_maps(elf, [descriptor, descriptor])
        with self.assertRaises(ValidationError):
            discover_entry(elf, trapped_calls=maps, indirect_calls={BASE: [BASE + 8]})
        with self.assertRaises(ValidationError):
            discover_entry(program([0x0080F809, 0x03E00008]), trapped_calls=maps)


if __name__ == '__main__':
    unittest.main()
