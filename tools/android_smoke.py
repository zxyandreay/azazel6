"""Cross-compile native components for Android ARM64; does not create an APK."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from analyze import ROOT, local_output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path, default=os.environ.get("ANDROID_NDK_HOME"))
    parser.add_argument("--ninja", type=Path, help="Ninja executable if not on PATH")
    parser.add_argument("--framework-source", type=Path, default=ROOT / "local/deps/yakumo")
    parser.add_argument("--build", type=Path, default=ROOT / "local/build/android")
    parser.add_argument("--entry-dir", type=Path, help="Optional locally generated entry probe")
    args = parser.parse_args()
    if args.ndk is None or not (args.ndk / "build/cmake/android.toolchain.cmake").is_file():
        raise ValueError("Set ANDROID_NDK_HOME or --ndk to an installed Android NDK (tested: 27.1.12297006)")
    build = local_output(args.build)
    command = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={args.ndk.resolve() / 'build/cmake/android.toolchain.cmake'}",
               "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-26", "-DBUILD_TESTING=OFF",
               "-DAZAZEL6_FRAMEWORK=ON", f"-DAZAZEL6_FRAMEWORK_SOURCE={args.framework_source.resolve()}"]
    if args.ninja:
        command.append(f"-DCMAKE_MAKE_PROGRAM={args.ninja.resolve()}")
    if args.entry_dir:
        command.append(f"-DAZAZEL6_ENTRY_DIR={args.entry_dir.resolve()}")
    subprocess.run(command, check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", "2"], check=True)
    print("Android ARM64 compilation passed. Device execution and APK packaging remain unverified.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"android_smoke: {error}", file=sys.stderr)
        sys.exit(1)
