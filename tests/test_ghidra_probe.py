"""Synthetic evidence contracts; no installed Ghidra or game input required."""
import copy
import struct
from pathlib import Path
import tempfile
import unittest

from test_elf import fixture, ROOT
from elf import Elf, ValidationError
from ghidra_probe import addresses, rea_preflight, verify_observations, main


class GhidraProbeTests(unittest.TestCase):
    def test_wrong_input_creates_no_evidence_or_process(self):
        from unittest.mock import patch
        (ROOT / 'local').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'local') as directory:
            path = Path(directory) / 'synthetic.elf'
            path.write_bytes(fixture())
            output = Path(directory) / 'evidence'
            with patch('ghidra_probe.execute') as launch, self.assertRaises(ValidationError):
                main([str(path), '--root', '08804000', '--output', str(output), '--ghidra', directory])
            self.assertFalse(output.exists())
            launch.assert_not_called()

    def test_missing_abi_is_rejected_even_with_psp_flags_on_linux(self):
        data = fixture()
        struct.pack_into('<I', data, 36, 0x10A23001)
        result = rea_preflight(Elf.parse(data), 'Linux', 'x86_64')
        self.assertEqual(result['status'], 'REJECTED_PREREQUISITES')
        self.assertEqual(len(result['reasons']), 1)
        self.assertIn('missing ABI', result['reasons'][0])
        self.assertFalse(result['abi_record_declared'])

    def test_host_and_flags_refusals_are_independent(self):
        result = rea_preflight(Elf.parse(fixture()), 'Windows', 'AMD64')
        self.assertEqual(len(result['reasons']), 3)
        self.assertIn('Linux x64', result['reasons'][0])

    def test_address_bounds(self):
        self.assertEqual(addresses(['0x08804000', '0xfffffffc']), [0x08804000, 0xfffffffc])
        for values in ([], ['-4'], ['100000000'], ['1'], ['0', '0'], [hex(i * 4) for i in range(17)]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                addresses(values)

    def test_image_language_bytes_and_address_identity(self):
        elf = Elf.parse(fixture())
        result = {'input_sha256': elf.report()['sha256'], 'language': 'Allegrex:LE:32:default',
                  'functions': [{'requested': '0x08804000', 'instructions': [
                      {'address': '0x08804000', 'bytes': '0800e003'}]}]}
        self.assertIs(verify_observations(elf, result, [elf.entry]), result)
        variants = []
        for key, value in [('input_sha256', '0' * 64), ('language', 'MIPS:LE:32:default')]:
            changed = copy.deepcopy(result)
            changed[key] = value
            variants.append(changed)
        for key, value in [('bytes', '00000000'), ('bytes', '08'), ('address', '0x88804000'), ('address', '0x08804001')]:
            changed = copy.deepcopy(result)
            changed['functions'][0]['instructions'][0][key] = value
            variants.append(changed)
        changed = copy.deepcopy(result)
        changed['functions'][0]['instructions'] = []
        variants.append(changed)
        for changed in variants:
            with self.subTest(changed=changed), self.assertRaises(ValidationError):
                verify_observations(elf, changed, [elf.entry])
        with self.assertRaises(ValidationError):
            verify_observations(elf, result, [elf.entry + 4])


if __name__ == '__main__':
    unittest.main()
