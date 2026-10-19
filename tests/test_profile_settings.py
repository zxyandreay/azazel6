import unittest
from entry_probe import language_mode


class ProfileSettingsTests(unittest.TestCase):
    def test_explicit_language_and_button_values(self):
        for language in range(12):
            for button in (0, 1):
                self.assertEqual(language_mode({'language_mode': {'language': language, 'confirm_button': button}}),
                                 (language, button))

    def test_missing_ambiguous_or_invalid_settings_reject(self):
        for settings in (None, [], {}, {'language': 1}, {'language': 1, 'confirm_button': 1, 'extra': 0}):
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                language_mode({'language_mode': settings})
        with self.assertRaises(ValueError):
            language_mode({})
        for field, values in (('language', [-1, 12, True, '1', 1.0]), ('confirm_button', [-1, 2, False, '1', 1.0])):
            for value in values:
                settings = {'language': 1, 'confirm_button': 1, field: value}
                with self.subTest(settings=settings), self.assertRaises(ValueError):
                    language_mode({'language_mode': settings})
