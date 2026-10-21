"""Install and exercise the synthetic host probe on an explicitly selected AVD.

Requires Pillow for screenshot pixel checks. All evidence stays under local/.
This does not execute guest code or validate a physical Android GPU/controller.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

from analyze import ROOT, local_output

PACKAGE = "org.azazel6.probe"
COMPONENT = PACKAGE + "/.HostActivity"


class Smoke:
    def __init__(self, adb, serial, output):
        if not re.fullmatch(r"emulator-[0-9]+", serial):
            raise ValueError("This input/lifecycle smoke test requires an explicit emulator serial")
        self.adb, self.serial, self.output = adb, serial, output
        self.report = {"kind": "synthetic_android_runtime", "game_execution": False,
                       "status": "running", "checks": [], "screenshots": {}}

    def call(self, *args, timeout=30):
        result = subprocess.run([str(self.adb), "-s", self.serial, *args],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            raise RuntimeError(f"adb {' '.join(args[:3])}: {result.stderr.decode(errors='replace').strip()}")
        return result.stdout

    def shell(self, *args, timeout=30):
        return self.call("shell", *args, timeout=timeout).decode(errors="replace").strip()

    def logs(self):
        return self.call("logcat", "-d", "-v", "threadtime", "-s", "SDL/APP", "SDL", "AndroidRuntime", "libc").decode(errors="replace")

    def wait(self, predicate, label, seconds=45):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(1)
        raise RuntimeError(f"Timed out: {label}")

    def passed(self, label):
        self.report["checks"].append(label)
        print("PASS " + label, flush=True)

    def capture(self, name, color):
        from PIL import Image
        data = self.call("exec-out", "screencap", "-p")
        (self.output / (name + ".png")).write_bytes(data)
        with Image.open(io.BytesIO(data)) as source:
            rgb = source.convert("RGB")
            w, h = rgb.size
            r, g, b = rgb.getpixel((w // 2, h // 2))
        self.report["screenshots"][name] = {"size": [w, h], "center_rgb": [r, g, b]}
        if w <= h:
            raise RuntimeError(f"{name}: expected landscape, got {w}x{h}")
        matches = {"blue": b > r + 15 and b > g + 10,
                   "green": g > 70 and g > r + 40,
                   "red": r > 70 and r > g + 40}
        if not matches[color]:
            raise RuntimeError(f"{name}: expected {color} diagnostic, got RGB {(r, g, b)}")
        return w, h

    def run(self, apk, stability_seconds):
        manifest = json.loads(apk.with_name("build-report.json").read_text())
        if (manifest.get("kind") != "synthetic_host_probe" or manifest.get("game_execution") is not False or
                hashlib.sha256(apk.read_bytes()).hexdigest() != manifest.get("sha256")):
            raise ValueError("APK does not match the synthetic build report")
        self.wait(lambda: self.shell("getprop", "sys.boot_completed") == "1", "Android boot", 180)
        abis = self.shell("getprop", "ro.product.cpu.abilist").split(",")
        if manifest.get("abi") != abis[0]:
            raise ValueError("Use an APK matching the AVD's primary ABI; translation is not native ABI evidence")
        self.report.update({"apk_sha256": manifest["sha256"], "abi": abis[0],
                            "supported_abis": abis, "page_size": int(self.shell("getconf", "PAGE_SIZE")),
                            "api_level": self.shell("getprop", "ro.build.version.sdk"),
                            "build_fingerprint": self.shell("getprop", "ro.build.fingerprint")})
        installed = self.call("install", "-r", str(apk), timeout=120).decode(errors="replace")
        (self.output / "install.txt").write_text(installed)
        if "Success" not in installed:
            raise RuntimeError("APK installation did not report success")
        self.passed("matching ABI APK installed")
        self.shell("am", "force-stop", PACKAGE)
        self.call("logcat", "-c")
        self.shell("input", "keyevent", "KEYCODE_WAKEUP")
        self.shell("wm", "dismiss-keyguard")
        self.shell("am", "start", "-W", "-n", COMPONENT, timeout=90)
        self.wait(lambda: "First synthetic frame presented" in self.logs(), "first Vulkan frame", 90)
        pid = self.shell("pidof", PACKAGE)
        if not pid:
            raise RuntimeError("App process missing")
        self.report["pid"] = pid
        w, h = self.capture("launch", "blue")
        self.passed("Vulkan frame presented and captured pixels verified")
        x, y = str(w // 2), str(h // 2)
        self.shell("input", "touchscreen", "motionevent", "DOWN", x, y)
        try:
            self.wait(lambda: "contacts=1" in self.logs(), "touch contact")
            self.capture("touch-held", "green")
        finally:
            self.shell("input", "touchscreen", "motionevent", "UP", x, y)
        time.sleep(1)
        self.capture("touch-released", "blue")
        self.passed("Android touch down/up changed rendered output")
        # Android's uinput command creates a virtual kernel input device.
        # Numeric ioctl codes also work before symbolic names were supported.
        with (self.output / "uinput.txt").open("wb") as diagnostics:
            pad = subprocess.Popen([str(self.adb), "-s", self.serial, "shell", "uinput", "-"],
                                   stdin=subprocess.PIPE, stdout=diagnostics, stderr=diagnostics)
            def send(command, **fields):
                pad.stdin.write((json.dumps(dict(id=1, command=command, **fields)) + "\n").encode())
                pad.stdin.flush()
            try:
                send("register", name="Azazel6 Virtual Pad", vid=0x18d2, pid=0x2c42,
                     bus="usb", configuration=[{"type": 100, "data": [1]},
                         {"type": 101, "data": [304, 305, 307, 308, 314, 315, 544, 545, 546, 547]}])
                self.wait(lambda: "Gamepad selected: Azazel6 Virtual Pad" in self.logs(), "virtual pad connected")
                send("inject", events=[1, 304, 1, 0, 0, 0])
                self.wait(lambda: "buttons=0x00004000" in self.logs(), "virtual Cross button")
                self.capture("virtual-button-held", "red")
                send("inject", events=[1, 304, 0, 0, 0, 0])
                time.sleep(1)
                self.capture("virtual-button-released", "blue")
                send("inject", events=[1, 304, 1, 0, 0, 0])
                time.sleep(1)
                self.capture("virtual-button-disconnecting", "red")
                cleared_before_disconnect = self.logs().count("Host input: buttons=0x00000000 contacts=0")
            finally:
                pad.stdin.close()
                try:
                    pad.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pad.kill()
                    pad.wait(timeout=5)
            if pad.returncode:
                raise RuntimeError("Android uinput command failed; see uinput.txt")
        # SDL's Android joystick detector polls removals every three seconds.
        # Observe a fresh state transition rather than assuming a fixed delay.
        self.wait(lambda: self.logs().count("Host input: buttons=0x00000000 contacts=0") >
                  cleared_before_disconnect, "controller state cleared after removal", 15)
        self.capture("virtual-button-disconnected", "blue")
        self.passed("Android virtual controller connect, press, release and held disconnect")
        for cycle in range(3):
            before = self.logs().count("Host probe suspended")
            foreground = self.logs().count("Host probe foreground")
            self.shell("input", "touchscreen", "motionevent", "DOWN", x, y)
            self.shell("input", "keyevent", "KEYCODE_HOME")
            self.wait(lambda: self.logs().count("Host probe suspended") > before, "background suspend")
            self.shell("am", "start", "-W", "-n", COMPONENT, timeout=60)
            self.wait(lambda: self.logs().count("Host probe foreground") > foreground, "foreground resume")
            time.sleep(2)
            if self.shell("pidof", PACKAGE) != pid:
                raise RuntimeError("App restarted during lifecycle cycle")
            self.capture(f"resume-{cycle + 1}", "blue")
        self.passed("three Home/resume cycles retained process and cleared contacts")
        start_frames = [int(n) for n in re.findall(r"Synthetic progress: frames=(\d+)", self.logs())]
        (self.output / "memory-before.txt").write_text(self.shell("dumpsys", "meminfo", PACKAGE))
        deadline = time.monotonic() + stability_seconds
        while time.monotonic() < deadline:
            if self.shell("pidof", PACKAGE) != pid:
                raise RuntimeError("App exited during stability interval")
            time.sleep(2)
        end_logs = self.logs()
        end_frames = [int(n) for n in re.findall(r"Synthetic progress: frames=(\d+)", end_logs)]
        if not end_frames or end_frames[-1] <= (start_frames[-1] if start_frames else 0):
            raise RuntimeError("No advancing synthetic frame evidence during stability interval")
        if any(marker in end_logs for marker in ("Host probe failure:", "Lifecycle failure:", "FATAL EXCEPTION", "Fatal signal")):
            raise RuntimeError("Runtime failure found in logcat")
        self.report["stability_seconds"] = stability_seconds
        self.report["last_synthetic_frame"] = end_frames[-1]
        (self.output / "memory-after.txt").write_text(self.shell("dumpsys", "meminfo", PACKAGE))
        self.capture("stability", "blue")
        self.passed("bounded stability interval with advancing synthetic frames")
        self.report["status"] = "passed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--adb", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--stability-seconds", type=int, default=30)
    args = parser.parse_args()
    if not 10 <= args.stability_seconds <= 300:
        raise ValueError("Stability interval must be 10..300 seconds")
    root = local_output(ROOT / "local/android-runtime")
    root.mkdir(parents=True, exist_ok=True)
    output = local_output(args.output) if args.output else Path(tempfile.mkdtemp(prefix="run-", dir=root))
    if output.exists() and any(output.iterdir()):
        raise ValueError("Use a fresh evidence directory to avoid mixing test runs")
    output.mkdir(parents=True, exist_ok=True)
    sdk = os.environ.get("ANDROID_HOME")
    adb = args.adb or (Path(sdk) / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb") if sdk else None)
    if not adb or not adb.is_file():
        raise ValueError("Set ANDROID_HOME or --adb")
    smoke = Smoke(adb, args.serial, output)
    try:
        smoke.run(args.apk.resolve(), args.stability_seconds)
    except Exception as error:
        smoke.report.update({"status": "failed", "error": str(error)})
        raise
    finally:
        try:
            (output / "logcat.txt").write_text(smoke.logs(), encoding="utf-8")
        except (OSError, RuntimeError, subprocess.TimeoutExpired):
            pass
        (output / "result.json").write_text(json.dumps(smoke.report, indent=2) + "\n")
        print(f"Runtime evidence: {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"android_runtime_smoke: {error}", file=sys.stderr)
        sys.exit(1)
