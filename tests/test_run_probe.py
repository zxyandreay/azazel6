"""Synthetic runner/capture contracts; these fixtures never execute the game."""
import json
import os
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from test_elf import ROOT
from run_probe import main, run_mounted, summarize_framebuffer
import sys
import time


class RunProbeTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'local').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'local')
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name)

    def run_fixture(self, code=1, pixels=None, timeout=False):
        def execute(command, **kwargs):
            self.assertEqual(kwargs['timeout'], 15)
            self.assertEqual(command[-2], '--framebuffer-rgba')
            if pixels is not None:
                Path(command[-1]).write_bytes(pixels)
            if timeout:
                raise subprocess.TimeoutExpired(command, 15)
            return subprocess.CompletedProcess(command, code)
        with patch('run_probe.validate'), patch('run_probe.subprocess.run', side_effect=execute), patch('builtins.print'):
            status = main(['synthetic-input', '--binary', 'synthetic-probe', '--output', str(self.output),
                           '--capture-framebuffer'])
        return status, json.loads((self.output / 'execution.json').read_text())

    def test_failed_run_can_capture_without_becoming_success(self):
        pixels = bytearray(b'\0\0\0\xff' * (480 * 272))
        pixels[0:4] = b'\xff\x40\x20\0'
        code, report = self.run_fixture(pixels=pixels)
        self.assertEqual(code, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['framebuffer']['nonblack_pixels'], 1)
        preview = (self.output / 'framebuffer.ppm').read_bytes()
        self.assertTrue(preview.startswith(b'P6\n480 272\n255\n\xff\x40\x20'))
        self.assertEqual(len(preview), len(b'P6\n480 272\n255\n') + 480 * 272 * 3)

    def test_black_capture_and_import_boundary_are_separate(self):
        code, report = self.run_fixture(code=3, pixels=b'\0' * (480 * 272 * 4))
        self.assertEqual(code, 0)
        self.assertEqual(report['status'], 'research_boundary')
        self.assertEqual(report['framebuffer']['status'], 'captured')
        self.assertEqual(report['framebuffer']['nonblack_pixels'], 0)

    def test_stale_capture_removed_on_display_off(self):
        (self.output / 'framebuffer.rgba').write_bytes(b'old')
        (self.output / 'framebuffer.ppm').write_bytes(b'old')
        _, report = self.run_fixture()
        self.assertEqual(report['framebuffer']['status'], 'unavailable')
        self.assertFalse((self.output / 'framebuffer.ppm').exists())

    def test_timeout_never_claims_partial_capture(self):
        _, report = self.run_fixture(timeout=True, pixels=b'partial')
        self.assertEqual(report['status'], 'timeout')
        self.assertEqual(report['framebuffer']['status'], 'unavailable')
        self.assertFalse((self.output / 'framebuffer.ppm').exists())

    def test_wrong_size_rejects_without_preview(self):
        for size in (0, 4, 480 * 272 * 4 + 1):
            path = self.output / 'framebuffer.rgba'
            path.write_bytes(b'\0' * size)
            self.assertEqual(summarize_framebuffer(path)['status'], 'invalid')
            self.assertFalse(path.with_suffix('.ppm').exists())

    def test_capture_symlink_cannot_escape_local(self):
        with tempfile.TemporaryDirectory() as outside:
            target = Path(outside) / 'private.rgba'
            target.write_bytes(b'preserve')
            if os.name == 'nt':
                # Directory junctions exercise real resolve() confinement without
                # requiring Windows' optional symbolic-link privilege.
                import _winapi
                link = self.output / 'outside'
                _winapi.CreateJunction(str(Path(outside)), str(link))
                capture_output = link
                self.addCleanup(link.rmdir)
            else:
                (self.output / 'framebuffer.rgba').symlink_to(target)
                capture_output = self.output
            with patch('run_probe.validate'), patch('run_probe.subprocess.run') as execute:
                with self.assertRaises(ValueError):
                    main(['synthetic', '--binary', 'synthetic', '--output', str(capture_output),
                          '--capture-framebuffer'])
                execute.assert_not_called()
            self.assertEqual(target.read_bytes(), b'preserve')

    def test_default_run_does_not_request_capture(self):
        with patch('run_probe.validate'), patch('run_probe.subprocess.run',
                return_value=subprocess.CompletedProcess([], 3)) as execute, patch('builtins.print'):
            self.assertEqual(main(['synthetic', '--binary', 'synthetic', '--output', str(self.output)]), 0)
        self.assertEqual(len(execute.call_args.args[0]), 2)
        self.assertNotIn('framebuffer', json.loads((self.output / 'execution.json').read_text()))

    def test_input_validation_precedes_output_and_execution(self):
        output = self.output / 'not-created'
        with patch('run_probe.validate', side_effect=ValueError('wrong input')), \
                patch('run_probe.subprocess.run') as execute:
            with self.assertRaises(ValueError):
                main(['synthetic', '--binary', 'synthetic', '--output', str(output), '--capture-framebuffer'])
            execute.assert_not_called()
        self.assertFalse(output.exists())

    def test_media_fingerprint_precedes_output_and_execution(self):
        media = self.output / 'synthetic.iso'
        media.write_bytes(b'synthetic media')
        for size, digest in ((1, hashlib.sha256(b'synthetic media').hexdigest()),
                             (media.stat().st_size, '0' * 64)):
            with patch('run_probe.validate'), patch('run_probe.load_profile', return_value={
                    'media': {'size': size, 'sha256': digest}}), patch('run_probe.subprocess.run') as execute:
                with self.assertRaises(ValueError):
                    main(['synthetic', '--binary', 'synthetic', '--iso', str(media),
                          '--output', str(self.output / 'not-created')])
                execute.assert_not_called()
            self.assertFalse((self.output / 'not-created').exists())

    def test_explicit_media_forwarded_and_fingerprint_reported(self):
        media = self.output / 'synthetic.iso'
        media.write_bytes(b'synthetic media')
        digest = hashlib.sha256(media.read_bytes()).hexdigest()
        with patch('run_probe.validate'), patch('run_probe.load_profile', return_value={
                'media': {'size': media.stat().st_size, 'sha256': digest}}), \
                patch('run_probe.run_mounted', return_value=(3, None)) as execute, \
                patch('builtins.print'):
            self.assertEqual(main(['synthetic', '--binary', 'synthetic', '--iso', str(media),
                                   '--output', str(self.output)]), 0)
        self.assertEqual(execute.call_args.args[0][-2:], ['--iso', str(media.resolve())])
        self.assertEqual(json.loads((self.output / 'execution.json').read_text())['media_sha256'], digest)

    def test_mount_gate_bounds_and_grant(self):
        ready = "print('validated_media_waiting', flush=True); assert sys.stdin.read(1) == 'G'; "
        for body, setup, expected in [
                ("time.sleep(20)", .2, (None, "media_validation")),
                (ready + "time.sleep(20)", 5, (None, "execution")),
                (ready + "sys.exit(3)", 5, (3, None)),
                ("sys.exit(3)", 5, (1, None))]:
            with self.subTest(body=body), (self.output / "gate.log").open("w") as log:
                code = run_mounted([sys.executable, "-c", "import time, sys; " + body],
                                   log, time.monotonic() + setup, .2)
                self.assertEqual(code, expected)


if __name__ == '__main__':
    unittest.main()
