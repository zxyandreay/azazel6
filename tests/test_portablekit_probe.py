"""Synthetic refusal and isolation checks for the optional upstream comparison."""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from test_elf import ROOT, fixture
from elf import ValidationError
from portablekit_probe import checked_source, main, probe_environment


class PortableKitProbeTests(unittest.TestCase):
    def test_wrong_input_has_no_side_effects(self):
        (ROOT / 'local').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'local') as directory:
            elf = Path(directory) / 'synthetic.elf'
            elf.write_bytes(fixture())
            output = Path(directory) / 'output'
            with patch('portablekit_probe.checked_source') as source, patch('subprocess.run') as run:
                with self.assertRaises(ValidationError):
                    main(['run', str(elf), '--output', str(output)])
                source.assert_not_called()
                run.assert_not_called()
                self.assertFalse(output.exists())

    def test_unpinned_or_modified_reference_is_refused(self):
        pin = json.loads((ROOT / 'dependencies.json').read_text())['portablekit']['revision']
        for revision, dirty in [('0' * 40, ''), (pin, ' M host/system.cpp'),
                                (pin, '?? host/unknown.cpp')]:
            with patch('subprocess.check_output', side_effect=[revision, dirty]), self.assertRaises(ValueError):
                checked_source(Path('unused'))
        with patch('subprocess.check_output', side_effect=[pin, '']):
            self.assertEqual(checked_source(Path('unused')), pin)

    def test_environment_drops_inherited_runtime_overrides(self):
        with patch.dict(os.environ, {'PSPRECOMP_MAX_DISPATCHES': '0', 'psprecomp_no_interpreter': '1',
                                     'AZAZEL6_EVAL_FAST_LOADING': '1', 'AZAZEL6_EVAL_DATA_DIR': 'outside'}, clear=True):
            env = probe_environment(Path('local/probe'))
        self.assertNotIn('PSPRECOMP_MAX_DISPATCHES', env)
        self.assertNotIn('psprecomp_no_interpreter', env)
        self.assertEqual(env['AZAZEL6_EVAL_FAST_LOADING'], '0')
        self.assertEqual(env['AZAZEL6_EVAL_STRICT_HLE'], '1')
        self.assertEqual(env['AZAZEL6_EVAL_DATA_DIR'], str(Path('local/probe')))
