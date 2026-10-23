import struct
import unittest
from elf import Elf, ValidationError
from entry_cfg import discover_entry
from indirect_calls import constructor_maps, jump_maps, trap_maps, pointer_maps, immediate_maps, guard_generated, verify_break_stops
from test_entry_cfg import program

BASE = 0x08804000


class IndirectCallTests(unittest.TestCase):
    def test_related_slot_calls_keep_separate_targets_and_terminal_tail(self):
        # Synthetic three-call/final-tail sequence, with independent slot identity.
        words = [0x0040F809, 0x24840004, 0x0040F809, 0,
                 0x0040F809, 0, 0x03200008, 0x27BD0010,
                 BASE + 48, BASE + 56, BASE + 64, BASE + 72,
                 0x03E00008, 0, 0x03E00008, 0, 0x03E00008, 0, 0x03E00008, 0]
        elf = program(words)
        calls = pointer_maps(elf, [dict(site=hex(BASE + i * 8), register=2,
                                       pointers=[hex(BASE + 32 + i * 4)]) for i in range(3)])
        tails = pointer_maps(elf, [dict(site=hex(BASE + 24), register=25,
                                       pointers=[hex(BASE + 44)])], tail=True)
        cfg = discover_entry(elf, indirect_calls={s: m['targets'] for s, m in calls.items()},
                             indirect_tails={s: m['targets'] for s, m in tails.items()})
        self.assertEqual(cfg['ranges'], [[BASE, 32]])
        self.assertEqual(cfg['indirect_calls'], {BASE + i * 8: [BASE + 48 + i * 8] for i in range(3)})
        self.assertEqual(cfg['indirect_tails'], {BASE + 24: [BASE + 72]})
        for site, row in (calls | tails).items():
            text = guard_generated(f'L_{site:08X}:\n    ctx.gpr[4] += 4;\n', {site: row})
            self.assertLess(text.index('guard_indirect_call'), text.index('ctx.gpr[4]'))
            self.assertIn(f'{{0x{row["targets"][0]:08X}u}}', text)
        words[11] = 0xFFFFFFFF
        with self.assertRaises(ValidationError):
            pointer_maps(program(words), [dict(site=hex(BASE + 24), register=25,
                                               pointers=[hex(BASE + 44)])], tail=True)

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
        for lower in (0x24424020, 0x34424020, 0x24434020):  # ADDIU / ORI / different destination
            elf = program([0x0080F809, 0, 0x03E00008, 0, 0x3C020880, lower, 0, 0, 0x03E00008, 0])
            desc = dict(site=hex(BASE), register=4, sources=[hex(BASE + 16)])
            mapping = immediate_maps(elf, [desc])
            self.assertEqual(mapping[BASE]['targets'], [BASE + 32])
            self.assertEqual(mapping[BASE]['source_pairs'], [(BASE + 16, BASE + 20)])
            self.assertEqual(discover_entry(elf, indirect_calls={BASE: [BASE + 32]})['ranges'], [[BASE, 16]])
            self.assertIn('ctx.gpr[4], {0x08804020u}', guard_generated(f'L_{BASE:08X}:\n', mapping))
        data = bytearray(elf.data)
        base = 0x08808000
        struct.pack_into('<I', data, 24, base)
        struct.pack_into('<I', data, 60, base)
        struct.pack_into('<II', data, 0x110, 0x3C020881, 0x24428020)
        mapping = immediate_maps(Elf.parse(data), [dict(site=hex(base), register=4, sources=[hex(base + 16)])])
        self.assertEqual(mapping[base]['targets'], [base + 32])

    def test_nonadjacent_address_pair_requires_preserved_register_and_flow(self):
        desc = dict(site=hex(BASE), register=4,
                    sources=[dict(upper=hex(BASE + 16), lower=hex(BASE + 24))])
        for middle in (0, 0x2485005C, 0x34A50004, 0x3C030886, 0xAC820004):
            elf = program([0x0080F809, 0, 0x03E00008, 0, 0x3C020880, middle, 0x24424020, 0, 0x03E00008, 0])
            self.assertEqual(immediate_maps(elf, [desc])[BASE]['targets'], [BASE + 32])
        for middle in (0x24420001, 0x3C020881, 0x34424000, 0x8C820000, 0x0C000000,
                       0x08000000, 0x10000000, 0x03E00008, 0x0000000D, 0x00001021):
            elf = program([0x0080F809, 0, 0x03E00008, 0, 0x3C020880, middle, 0x24424020, 0, 0x03E00008, 0])
            with self.subTest(middle=middle), self.assertRaises(ValidationError):
                immediate_maps(elf, [desc])
        for lower in (BASE + 16, BASE + 25, BASE + 52):
            with self.subTest(lower=lower), self.assertRaises(ValidationError):
                immediate_maps(elf, [desc | {'sources': [dict(upper=hex(BASE + 16), lower=hex(lower))]}])

    def test_two_state_sources_preserve_both_guarded_callbacks(self):
        words = [0x00A0F809, 0, 0x03E00008, 0, 0x3C020880, 0x24454030,
                 0, 0x3C030880, 0x34654038, 0, 0, 0, 0x03E00008, 0,
                 0x03E00008, 0]
        desc = dict(site=hex(BASE), register=5, sources=[hex(BASE + 16), hex(BASE + 28)])
        mapping = immediate_maps(program(words), [desc])
        self.assertEqual(mapping[BASE]['targets'], [BASE + 48, BASE + 56])
        self.assertEqual(mapping[BASE]['source_pairs'], [(BASE + 16, BASE + 20),
                                                       (BASE + 28, BASE + 32)])
        guarded = guard_generated(f'L_{BASE:08X}:\n    jump_target = ctx.gpr[5];\n', mapping)
        self.assertIn('ctx.gpr[5], {0x08804030u, 0x08804038u}', guarded)
        self.assertLess(guarded.index('guard_indirect_call'), guarded.index('jump_target'))
        words[8] = 0x34454038  # Second pair reads the wrong upper register.
        with self.assertRaises(ValidationError):
            immediate_maps(program(words), [desc])

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
                             (0x3C020880, 0x24824020), (0x3C020880, 0x24404020),
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

    def test_v0_slot_call_keeps_continuation_and_guards_before_delay(self):
        # Independent synthetic JALR-v0 fixture with a separately stored slot.
        elf = program([0x0040F809, 0x24840004, 0x03E00008, 0,
                       BASE + 20, 0x03E00008, 0])
        descriptor = dict(site=hex(BASE), register=2, pointers=[hex(BASE + 16)])
        mapping = pointer_maps(elf, [descriptor])
        cfg = discover_entry(elf, indirect_calls={BASE: mapping[BASE]['targets']})
        self.assertEqual(mapping[BASE]['targets'], [BASE + 20])
        self.assertEqual(cfg['ranges'], [[BASE, 16]])
        self.assertEqual(cfg['trapped_calls'], [])
        source = guard_generated(f'L_{BASE:08X}:\n'
            '    jump_target = ctx.gpr[2];\n'
            '    ctx.set_gpr(31, 0x08804008u);\n'
            '    ctx.gpr[4] += 4;\n', mapping)
        self.assertIn('ctx.gpr[2], {0x08804014u}', source)
        self.assertLess(source.index('guard_indirect_call'), source.index('jump_target'))
        self.assertLess(source.index('guard_indirect_call'), source.index('ctx.set_gpr'))
        self.assertLess(source.index('guard_indirect_call'), source.index('ctx.gpr[4]'))

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
