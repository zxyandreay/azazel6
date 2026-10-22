"""Synthetic runner rejection tests, not evidence of Android execution."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from android_runtime_smoke import Smoke
from analyze import ROOT


class RuntimeGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "local").mkdir(exist_ok=True)

    def test_physical_or_ambiguous_device_rejected(self):
        for serial in ("", "USB1234", "emulator-5554 extra"):
            with self.assertRaises(ValueError):
                Smoke(Path("adb"), serial, Path("unused"))

    def test_unverified_apk_rejected_before_device_access(self):
        for change in ({"sha256": "incorrect"}, {"game_execution": True}, {"kind": "game"}):
            with self.subTest(change=change), tempfile.TemporaryDirectory(dir=ROOT / "local") as directory:
                apk = Path(directory) / "probe.apk"
                apk.write_bytes(b"synthetic APK fixture")
                manifest = dict(kind="synthetic_host_probe", game_execution=False,
                                abi="arm64-v8a", sha256=hashlib.sha256(apk.read_bytes()).hexdigest())
                manifest.update(change)
                apk.with_name("build-report.json").write_text(json.dumps(manifest))
                smoke = Smoke(Path("adb"), "emulator-5554", Path(directory))
                smoke.call = Mock(side_effect=AssertionError("must not contact device"))
                with self.assertRaises(ValueError):
                    smoke.run(apk, 30)
                smoke.call.assert_not_called()

    def test_translation_abi_rejected_before_install(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "local") as directory:
            apk = Path(directory) / "probe.apk"
            apk.write_bytes(b"synthetic APK fixture")
            apk.with_name("build-report.json").write_text(json.dumps(dict(
                kind="synthetic_host_probe", game_execution=False, abi="arm64-v8a",
                sha256=hashlib.sha256(apk.read_bytes()).hexdigest())))
            smoke = Smoke(Path("adb"), "emulator-5554", Path(directory))
            smoke.shell = Mock(side_effect=["1", "x86_64,arm64-v8a"])
            smoke.call = Mock(return_value=b"")
            with self.assertRaisesRegex(ValueError, "primary ABI"):
                smoke.run(apk, 30)
            smoke.call.assert_called_once_with("wait-for-device", timeout=180)
