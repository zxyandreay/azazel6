"""Independent registration fixtures; no game code or game execution."""
import re
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from aot_registration import compact_registration


def fixture(count):
    rows = [f'    runtime.register_function(0x{0x08804000 + i * 4:08X}u, '
            f'&entry_range_{i // 4}, "entry_range_{i // 4}");' for i in range(count)]
    return ('// synthetic preceding functions\n'
            'void register_generated_functions(Runtime &runtime) {\n'
            + '\n'.join(rows) + '\n}\n} // namespace psprecomp\n')


class RegistrationTests(unittest.TestCase):
    def test_large_table_preserves_all_entries_and_order(self):
        source = fixture(32768)
        transformed = compact_registration(source)
        before = re.findall(r'register_function\((0x\w+), &(\w+), "(\w+)"\)', source)
        after = re.findall(r'\{(0x\w+), &(\w+), "(\w+)"\}', transformed)
        self.assertEqual(after, before)
        self.assertEqual(transformed.count('runtime.register_function('), 1)
        self.assertIn('static const Registration registrations[]', transformed)
        self.assertTrue(transformed.startswith('// synthetic preceding functions\n'))
        self.assertTrue(transformed.endswith('\n}\n} // namespace psprecomp\n'))

    def test_single_entry(self):
        result = compact_registration(fixture(1))
        self.assertIn('{0x08804000u, &entry_range_0, "entry_range_0"}', result)

    def test_unknown_output_fails_closed(self):
        source = fixture(2)
        for invalid in [
            '', fixture(0), source + source, source.replace('\n}', ''),
            source.replace('    runtime.register_function', '    unknown_call', 1),
            source.replace('0x08804004u', '0x08804000u'),
            source.replace('0x08804004u', '0x08804001u'),
            source.replace('"entry_range_0"', '"different_name"', 1),
            source.replace('\n}', '\n    runtime.stop("unexpected");\n}', 1),
            compact_registration(source),
        ]:
            with self.subTest(invalid=invalid[:80]), self.assertRaises(ValueError):
                compact_registration(invalid)


if __name__ == '__main__':
    unittest.main()
