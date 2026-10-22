"""Synthetic, no-game tests for the guarded candidate-root inspector."""
import struct
import unittest

from test_elf import fixture
from elf import Elf, ValidationError
from inspect_root import inspect_root, reviewed_transfers
from indirect_calls import guard_generated

BASE = 0x08804000


def program(words):
    data = fixture()
    struct.pack_into('<' + 'I' * len(words), data, 0x100, *words)
    return Elf.parse(data)


class InspectRootTests(unittest.TestCase):
    def test_direct_call_is_only_a_candidate(self):
        jal = 0x0C000000 | ((BASE + 16) >> 2 & 0x03FFFFFF)
        elf = program([jal, 0, 0x03E00008, 0, 0x03E00008, 0, 0, 0])
        report = inspect_root(elf, BASE, (), instruction_budget=8)
        self.assertEqual(report['status'], 'CFG_DISCOVERED_NOT_AUTHORIZED')
        self.assertEqual(report['direct_calls'], {f'0x{BASE:08X}': f'0x{BASE+16:08X}'})
        self.assertEqual(report['candidate_callees'], [f'0x{BASE+16:08X}'])
        self.assertEqual(report['instruction_words'], 4)
        self.assertEqual(report['first_words'][0]['word'], f'0x{jal:08X}')
        self.assertNotIn('generated', report)
        excluded = inspect_root(elf, BASE, (BASE + 16,), instruction_budget=8)
        self.assertEqual(excluded['candidate_callees'], [])
        self.assertEqual(excluded['external_boundaries'], [])
        self.assertEqual(inspect_root(elf, BASE, (), (BASE + 16,), instruction_budget=8)['candidate_callees'], [])

    def test_unsupported_indirect_fails_closed_with_local_preview(self):
        elf = program([0x0080F809, 0, 0x03E00008, 0])
        report = inspect_root(elf, BASE, ())
        self.assertEqual(report['status'], 'BLOCKED_NEEDS_REVIEW')
        self.assertIn('verified target map', report['blocker'])
        self.assertEqual(report['first_words'][0]['word'], '0x0080F809')
        self.assertNotIn('candidate_callees', report)

    def test_reviewed_indirect_remains_guarded_but_scannable(self):
        elf = program([0x0080F809, 0, 0x03E00008, 0])
        known = {"indirect_calls": {BASE: [BASE + 16]}}
        report = inspect_root(elf, BASE, (), known_transfers=known)
        self.assertEqual(report['status'], 'CFG_DISCOVERED_NOT_AUTHORIZED')
        self.assertEqual(report['reviewed_indirect_sites'], [f'0x{BASE:08X}'])
        self.assertEqual(report['candidate_callees'], [])

    def test_object_call_diagnostic_stops_before_link_and_delay(self):
        # Synthetic object/slot loads and JALR v0. The unsupported successor
        # must not become reachable merely because a terminal guard is present.
        elf = program([0x8C830000, 0x8C620018, 0x0040F809, 0x24840004, 0x0000000C])
        site = BASE + 8
        transfers = reviewed_transfers(elf, {
            'trapped_calls': [{'site': hex(site), 'register': 2}]})
        report = inspect_root(elf, BASE, (), known_transfers=transfers)
        self.assertEqual(report['instruction_words'], 4)
        self.assertEqual(report['trapped_transfer_sites'], [f'0x{site:08X}'])
        self.assertEqual(report['reviewed_indirect_sites'], [])
        self.assertEqual(report['candidate_callees'], [])
        source = guard_generated(f'L_{site:08X}:\n'
            '    jump_target = ctx.gpr[2];\n'
            '    ctx.set_gpr(31, 0x08804010u);\n'
            '    ctx.gpr[4] += 4;\n', transfers['trapped_calls'])
        self.assertIn('ctx.gpr[2], {})) return;', source)
        self.assertLess(source.index('guard_indirect_call'), source.index('jump_target'))
        self.assertLess(source.index('guard_indirect_call'), source.index('ctx.set_gpr'))
        self.assertLess(source.index('guard_indirect_call'), source.index('ctx.gpr[4]'))

    def test_existing_root_requires_explicit_read_only_audit(self):
        jal = 0x0C000000 | ((BASE + 16) >> 2 & 0x03FFFFFF)
        elf = program([jal, 0, 0x03E00008, 0, 0x03E00008, 0])
        with self.assertRaisesRegex(ValidationError, "already declared in the local startup roots"):
            inspect_root(elf, BASE, (BASE,))
        report = inspect_root(elf, BASE, (BASE,), inspect_existing_root=True)
        self.assertEqual(report['status'], 'CFG_DISCOVERED_EXISTING_ROOT')
        self.assertEqual(report['profile_membership'], 'EXISTING_STARTUP_ROOT')
        self.assertEqual(report['candidate_callees'], [f'0x{BASE+16:08X}'])
        self.assertNotIn('generated', report)
        # The inspector may review an existing root, but cannot treat a second
        # already-declared function as new AOT coverage.
        excluded = inspect_root(elf, BASE, (BASE, BASE + 16), inspect_existing_root=True)
        self.assertEqual(excluded['candidate_callees'], [])
        self.assertEqual(excluded['instruction_words'], 4)

    def test_import_stub_still_rejects_existing_root_audit(self):
        elf = program([0x03E00008, 0])
        with self.assertRaisesRegex(ValidationError, "PSP import stub"):
            inspect_root(elf, BASE, (), (BASE,))
        with self.assertRaisesRegex(ValidationError, "PSP import stub"):
            inspect_root(elf, BASE, (BASE,), (BASE,), inspect_existing_root=True)
        for bad in (None, 1, 0, "true"):
            with self.subTest(flag=bad), self.assertRaises(ValidationError):
                inspect_root(elf, BASE, (), inspect_existing_root=bad)
        blocked = inspect_root(program([0x0080F809, 0]), BASE, (BASE,), inspect_existing_root=True)
        self.assertEqual(blocked['status'], 'BLOCKED_NEEDS_REVIEW')
        self.assertEqual(blocked['profile_membership'], 'EXISTING_STARTUP_ROOT')
        self.assertIn('verified target map', blocked['blocker'])

    def test_bad_address_budget_and_owned_boundaries(self):
        elf = program([0x03E00008, 0])
        for address in (BASE + 1, BASE + 0x20, 0xFFFFFFFF, -1, None, True):
            with self.subTest(address=address), self.assertRaises(ValidationError):
                inspect_root(elf, address, ())
        for limit in (0, -1, 4097, None, 1.0):
            with self.subTest(limit=limit), self.assertRaises(ValidationError):
                inspect_root(elf, BASE, (), instruction_budget=limit)
        with self.assertRaises(ValidationError):
            inspect_root(elf, BASE, (BASE,))
        with self.assertRaises(ValidationError):
            inspect_root(elf, BASE, (), (BASE,))
        self.assertEqual(inspect_root(elf, BASE, ())['instruction_words'], 2)


if __name__ == '__main__':
    unittest.main()
