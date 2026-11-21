"""Refusal checks for the narrow exact-input PortableKit AOT generator."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from test_elf import ROOT, fixture
from elf import ValidationError
from portablekit_aot import checked_entry, main


class PortableKitAotTests(unittest.TestCase):
    def test_wrong_executable_never_invokes_recompiler_or_creates_output(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'local') as directory:
            invalid = Path(directory) / 'wrong.elf'
            invalid.write_bytes(fixture())
            output = Path(directory) / 'aot'
            with patch('portablekit_aot.checked_source', return_value='a' * 40), patch(
                    'portablekit_aot.subprocess.run') as run:
                with self.assertRaises(ValidationError):
                    main([str(invalid), '--compiler', 'unused', '--output', str(output)])
                run.assert_not_called()
                self.assertFalse(output.exists())

    def test_changed_entry_control_flow_rejects(self):
        class FakeElf:
            entry = 0x08804124
        with patch('portablekit_aot.discover_entry', return_value={
                'ranges': [[0x08804124, 0x118]], 'instruction_words': 70,
                'instruction_addresses': list(range(0x08804124, 0x0880423c, 4))}):
            with self.assertRaisesRegex(ValidationError, 'CFG differs'):
                checked_entry(FakeElf())
