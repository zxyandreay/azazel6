import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from elf import ValidationError
from test_entry_cfg import program
from vfpu_overrides import VI2UC_WORD, lower_startup_vi2uc


BASE = 0x08804000


class VfpuOverrideTests(unittest.TestCase):
    def test_exact_word_and_generated_form_only(self):
        elf = program([VI2UC_WORD, VI2UC_WORD, 0])
        source = (f'L_{BASE:08X}:\n'
                  f'      rt.unsupported(0x{BASE:08X}u, 0x{VI2UC_WORD:08X}u, "vfpu4 not lowered yet"); return;\n'
                  f'L_{BASE + 4:08X}:\n'
                  f'      rt.unsupported(0x{BASE + 4:08X}u, 0x{VI2UC_WORD:08X}u, "vfpu4 not lowered yet"); return;\n'
                  f'L_{BASE + 8:08X}:\n')
        lowered = lower_startup_vi2uc(elf, source, [hex(BASE), hex(BASE + 4)])
        self.assertEqual(lowered.count('azazel6::execute_startup_vi2uc_q(ctx);'), 2)
        self.assertIn(f'goto L_{BASE + 8:08X};', lowered)
        self.assertNotIn('vfpu4 not lowered yet', lowered)
        partial = lower_startup_vi2uc(elf, source, [hex(BASE)])
        self.assertEqual(partial.count('azazel6::execute_startup_vi2uc_q(ctx);'), 1)
        self.assertIn(f'rt.unsupported(0x{BASE + 4:08X}u', partial)
        for bad_elf, bad_source, sites in (
                (elf, source, [hex(BASE), hex(BASE)]),
                (program([0, VI2UC_WORD, 0]), source, [hex(BASE)]),
                (elf, source.replace('vfpu4 not lowered yet', 'changed', 1), [hex(BASE)]),
                (elf, source.replace(f'L_{BASE + 8:08X}:\n', ''), [hex(BASE + 4)]),
                (elf, source, [hex(BASE + 1)])):
            with self.subTest(sites=sites), self.assertRaises(ValidationError):
                lower_startup_vi2uc(bad_elf, bad_source, sites)


if __name__ == '__main__':
    unittest.main()
