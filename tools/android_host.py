"""Build/sign the synthetic Android Host Probe; no game inputs or guest execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

from analyze import ROOT, local_output


def run(*command):
    subprocess.run([str(part) for part in command], check=True, cwd=ROOT)


def elf_alignment(data, abi="arm64-v8a"):
    """Require the requested ELF64 architecture and 16 KiB load compatibility."""
    machines = {"arm64-v8a": 183, "x86_64": 62}
    if abi not in machines:
        raise ValueError("Unsupported host probe ABI")
    if (len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01" or
            struct.unpack_from("<HH", data, 16) != (3, machines[abi]) or
            struct.unpack_from("<H", data, 52)[0] != 64):
        raise ValueError(f"Expected little-endian {abi} ELF64 library")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    if size != 56 or not count or offset + size * count > len(data):
        raise ValueError("Invalid ELF program header range")
    loads = 0
    for index in range(count):
        header = offset + index * size
        kind, _, file_offset, address, _, file_size, memory_size, alignment = struct.unpack_from("<IIQQQQQQ", data, header)
        if kind != 1:
            continue
        loads += 1
        if (alignment < 16384 or alignment & (alignment - 1) or
                file_offset % alignment != address % alignment or file_size > memory_size or
                file_offset + file_size > len(data)):
            raise ValueError("ELF load segment is invalid or not 16 KiB compatible")
    if not loads:
        raise ValueError("ELF has no loadable segments")


def dependency(source, fetch=False):
    lock = json.loads((ROOT / "dependencies.json").read_text())["sdl"]
    if not source.exists():
        if not fetch:
            raise ValueError("SDL missing; use --fetch-sdl for the explicit pinned checkout")
        source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--depth", "1", "--branch", lock["tag"], lock["url"], source)
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    changes = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip()
    if revision != lock["revision"] or changes:
        raise ValueError("SDL must match the lock and have no modifications/untracked files")
    return lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=os.environ.get("ANDROID_HOME"))
    parser.add_argument("--java-home", type=Path, default=os.environ.get("JAVA_HOME"))
    parser.add_argument("--ndk-version", default="27.1.12297006")
    parser.add_argument("--build-tools", default="36.0.0")
    parser.add_argument("--compile-sdk", default="36")
    parser.add_argument("--abi", choices=["arm64-v8a", "x86_64"], default="arm64-v8a")
    parser.add_argument("--fetch-sdl", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    default_output = "android-host" if args.abi == "arm64-v8a" else "android-host-x86_64"
    output = local_output(args.output or ROOT / "local" / default_output)
    output.mkdir(parents=True, exist_ok=True)
    if not args.sdk or not args.java_home:
        raise ValueError("Set ANDROID_HOME/JAVA_HOME or --sdk/--java-home (JDK 17)")
    sdk, java_home = args.sdk.resolve(), args.java_home.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    java, javac, keytool = [java_home / "bin" / (name + suffix) for name in ("java", "javac", "keytool")]
    build_tools = sdk / "build-tools" / args.build_tools
    android_jar = sdk / "platforms" / ("android-" + args.compile_sdk) / "android.jar"
    ndk = sdk / "ndk" / args.ndk_version
    ninja = sdk / "cmake/3.22.1/bin" / ("ninja" + suffix)
    aapt = build_tools / ("aapt2" + suffix)
    zipalign = build_tools / ("zipalign" + suffix)
    d8 = build_tools / "lib/d8.jar"
    signer = build_tools / "lib/apksigner.jar"
    for path in (java, javac, keytool, android_jar, ninja, aapt, zipalign, d8, signer,
                 ndk / "build/cmake/android.toolchain.cmake"):
        if not path.is_file():
            raise ValueError(f"Required installed tool missing: {path.name}")
    sdl = local_output(ROOT / "local/deps/sdl")
    lock = dependency(sdl, args.fetch_sdl)
    native = local_output(output / "native")
    run("cmake", "-S", ROOT / "platform/android", "-B", native, "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}", f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build/cmake/android.toolchain.cmake'}",
        f"-DANDROID_ABI={args.abi}", "-DANDROID_PLATFORM=android-29", "-DANDROID_STL=c++_static",
        "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON", "-DCMAKE_BUILD_TYPE=Release",
        f"-DAZAZEL6_SDL_SOURCE={sdl}")
    run("cmake", "--build", native, "--parallel", "2")
    # A fresh packaging directory prevents stale .class/.dex files from leaking
    # into a later APK. Retain it locally for reproducible diagnosis.
    package = Path(tempfile.mkdtemp(prefix="package-", dir=output))
    classes, dex = package / "classes", package / "dex"
    classes.mkdir(); dex.mkdir()
    sources = sorted((sdl / "android-project/app/src/main/java").rglob("*.java"))
    sources += sorted((ROOT / "platform/android/java").rglob("*.java"))
    if not sources:
        raise ValueError("No Java activity sources")
    # javac response-file quoting supports spaces in SDK/repository paths.
    arguments = ["-encoding", "UTF-8", "-source", "8", "-target", "8",
                 "-bootclasspath", str(android_jar), "-d", str(classes)]
    arguments += [str(path) for path in sources]
    argfile = package / "javac.args"
    argfile.write_text("\n".join('"' + part.replace("\\", "/").replace('"', '\\"') + '"' for part in arguments), encoding="utf-8")
    run(javac, "@" + str(argfile))
    classjar = package / "classes.jar"
    with zipfile.ZipFile(classjar, "w") as archive:
        for path in sorted(classes.rglob("*.class")):
            archive.write(path, path.relative_to(classes).as_posix())
    run(java, "-Xmx1g", "-cp", d8, "com.android.tools.r8.D8", "--lib", android_jar,
        "--min-api", "29", "--output", dex, classjar)
    unsigned = package / "unsigned.apk"
    run(aapt, "link", "-I", android_jar, "--manifest", ROOT / "platform/android/AndroidManifest.xml", "-o", unsigned)
    libraries = {"libmain.so": native / "libmain.so", "libSDL3.so": native / "sdl/libSDL3.so"}
    with zipfile.ZipFile(unsigned, "a", compression=zipfile.ZIP_STORED) as archive:
        for name, path in libraries.items():
            data = path.read_bytes()
            elf_alignment(data, args.abi)
            archive.writestr(f"lib/{args.abi}/" + name, data)
        dex_files = sorted(dex.glob("classes*.dex"))
        if not dex_files:
            raise ValueError("D8 produced no dex files")
        for path in dex_files:
            archive.write(path, path.name)
        archive.write(sdl / "LICENSE.txt", "assets/licenses/SDL.txt")
        archive.write(ROOT / "LICENSE", "assets/licenses/Azazel6.txt")
    aligned = package / "aligned.apk"
    run(zipalign, "-P", "16", "-f", "4", unsigned, aligned)
    key = output / "debug.keystore"
    if not key.exists():
        run(keytool, "-genkeypair", "-keystore", key, "-storepass", "android", "-keypass", "android",
            "-alias", "androiddebugkey", "-dname", "CN=Android Debug,O=Android,C=US",
            "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000", "-noprompt")
    signed = package / "signed.apk"
    run(java, "-jar", signer, "sign", "--ks", key, "--ks-pass", "pass:android",
        "--key-pass", "pass:android", "--ks-key-alias", "androiddebugkey", "--out", signed, aligned)
    run(java, "-jar", signer, "verify", "--verbose", signed)
    run(zipalign, "-c", "-P", "16", "4", signed)
    target = output / "azazel6-host-probe.apk"
    shutil.copyfile(signed, target)
    report = {"kind": "synthetic_host_probe", "sdl_revision": lock["revision"],
              "abi": args.abi,
              "min_sdk": 29, "target_sdk": 35, "compile_sdk": args.compile_sdk,
              "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
              "verified": ["elf_abi", "16k_load_alignment", "apk_signature", "16k_zip_alignment"],
              "device_execution": "NOT TESTED", "game_execution": False}
    (output / "build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Verified synthetic APK: {target}\nDevice lifecycle/input/presentation remain unverified.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"android_host: {error}", file=sys.stderr)
        sys.exit(1)
