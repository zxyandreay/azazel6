"""Package the pinned PortableKit Tekken hybrid as a signed Android ARM64 APK.

No game input or generated translation is placed in the APK. The Android
installer asks the player for the disc image on the device.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

from analyze import ROOT, local_output
from android_host import elf_alignment, run


APP_ID = "org.azazel6.game"
ABI = "arm64-v8a"
FONT_SHA256 = "68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5"
FONT_LICENSE_SHA256 = "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2"


def require_file(path):
    if not path.is_file():
        raise ValueError(f"Required build input missing: {path}")
    return path


def validate_build_cache(cache):
    settings = set(cache.splitlines())
    for setting in ("AZAZEL6_PORTABLEKIT_FFMPEG:STRING=bundled",
                    "PORTABLEKIT_ANDROID_APP:BOOL=ON",
                    "AZAZEL6_PORTABLEKIT_RENDERER:BOOL=ON"):
        if setting not in settings:
            raise ValueError(f"Android hybrid build lacks required setting: {setting}")
    if not any(line.startswith("AZAZEL6_PORTABLEKIT_AOT_SOURCE:FILEPATH=") and
               line.partition("=")[2] for line in settings):
        raise ValueError("Android hybrid build lacks the verified AOT source")


def verify_android_imports(nm, sdk, native):
    """Require every strong dynamic import in the packed libs at Android 29."""
    stubs = sdk / "ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/29"
    if os.name != "nt":
        stubs = sdk / "ndk/27.1.12297006/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/29"
    system = ("libc", "libm", "libdl", "liblog", "libandroid", "libvulkan",
              "libOpenSLES", "libGLESv1_CM", "libGLESv2", "libEGL")
    exports = set()
    for path in [require_file(stubs / (name + ".so")) for name in system] + list(native.values()):
        output = subprocess.check_output([str(nm), "-D", "--defined-only", str(path)], text=True)
        exports.update(line.split()[-1].split("@")[0] for line in output.splitlines()
                       if len(line.split()) >= 3)
    missing = set()
    for path in native.values():
        output = subprocess.check_output([str(nm), "-D", "--undefined-only", str(path)], text=True)
        missing.update(fields[1].split("@")[0] for line in output.splitlines()
                       if len(fields := line.split()) >= 2 and fields[0] == "U" and
                       fields[1].split("@")[0] not in exports)
    if missing:
        raise ValueError("Android 29 has unresolved native imports: " + ", ".join(sorted(missing)))


def verify_file_hash(path, expected):
    actual = hashlib.sha256(require_file(path).read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"Unexpected SHA-256 for {path.name}: {actual}")
    return path


def package_apk(sdk, java_home, build, sdl, output, font_dir,
                build_tools_version="36.0.0", compile_sdk="36"):
    sdk, java_home, build, sdl = (Path(p).resolve() for p in (sdk, java_home, build, sdl))
    output = local_output(output)
    output.mkdir(parents=True, exist_ok=True)
    cache = require_file(build / "CMakeCache.txt").read_text(encoding="utf-8")
    validate_build_cache(cache)
    aot_value = next(line.partition("=")[2] for line in cache.splitlines()
                     if line.startswith("AZAZEL6_PORTABLEKIT_AOT_SOURCE:FILEPATH="))
    aot_source = local_output(Path(aot_value))
    aot_report = json.loads(require_file(aot_source.parent / "report.json").read_text(encoding="utf-8"))
    aot_hash = hashlib.sha256(require_file(aot_source).read_bytes()).hexdigest()
    profile = json.loads((ROOT / "profiles/tekken6/config/profile.json").read_text(encoding="utf-8"))
    if (aot_report.get("generated_source_sha256") != aot_hash or
            aot_report.get("executable_sha256") != profile["executable"]["sha256"]):
        raise ValueError("AOT source/report no longer matches the pinned Tekken executable")
    suffix = ".exe" if os.name == "nt" else ""
    java, javac, keytool = [require_file(java_home / "bin" / (name + suffix))
                             for name in ("java", "javac", "keytool")]
    tools = sdk / "build-tools" / build_tools_version
    android_jar = require_file(sdk / "platforms" / ("android-" + compile_sdk) / "android.jar")
    aapt = require_file(tools / ("aapt2" + suffix))
    zipalign = require_file(tools / ("zipalign" + suffix))
    d8 = require_file(tools / "lib/d8.jar")
    signer = require_file(tools / "lib/apksigner.jar")
    ndk_bin = sdk / "ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/bin"
    if os.name != "nt":
        ndk_bin = sdk / "ndk/27.1.12297006/toolchains/llvm/prebuilt/linux-x86_64/bin"
    strip = require_file(ndk_bin / ("llvm-strip" + suffix))
    nm = require_file(ndk_bin / ("llvm-nm" + suffix))
    kit = ROOT / "local/deps/portablekit"
    manifest = require_file(kit / "packaging/android/AndroidManifest.xml")
    base_res = kit / "packaging/android/res"
    game_res = ROOT / "platform/android/game-res"
    native = {
        "libmain.so": require_file(build / "bin/libmain.so"),
        "libSDL3.so": require_file(sdl / "libSDL3.so"),
        "libavcodec.so": require_file(build / "bin/lib/libavcodec.so"),
        "libavutil.so": require_file(build / "bin/lib/libavutil.so"),
    }
    # The APK must carry the complete hybrid runtime and working movie/audio
    # decoders. This also rejects the FFmpeg-disabled cross-build experiment.
    for name, path in native.items():
        elf_alignment(path.read_bytes(), ABI)
    symbols = subprocess.check_output([str(nm), "-C", str(native["libmain.so"])], text=True)
    if "psprecomp::Tekken6Entry(" not in symbols or "counted_entry(" not in symbols:
        raise ValueError("ARM64 libmain.so lacks the native Tekken AOT entry/counter")
    verify_android_imports(nm, sdk, native)
    notices = {
        "Azazel6-LICENSE.txt": require_file(ROOT / "LICENSE"),
        "PortableKit-LICENSE.txt": require_file(kit / "LICENSE"),
        "DearImGui-LICENSE.txt": require_file(kit / "third_party/imgui/LICENSE.txt"),
        "tiny-AES-c-UNLICENSE.txt": require_file(kit / "third_party/tiny_aes/UNLICENSE"),
        "xxHash-LICENSE.txt": require_file(kit / "third_party/xxhash/LICENSE"),
        "SDL3-LICENSE.txt": require_file(ROOT / "local/deps/sdl/LICENSE.txt"),
        "FFmpeg-COPYING.LGPLv2.1.txt": require_file(build / "bin/lib/FFmpeg-COPYING.LGPLv2.1.txt"),
        "FFmpeg-SOURCE.txt": require_file(build / "bin/lib/FFmpeg-SOURCE.txt"),
    }
    font_dir = local_output(Path(font_dir))
    font = verify_file_hash(font_dir / "NotoSansCJKjp-Regular.otf", FONT_SHA256)
    notices["NotoSansCJK-OFL.txt"] = verify_file_hash(
        font_dir / "NotoSansCJK-LICENSE.txt", FONT_LICENSE_SHA256)
    package = Path(tempfile.mkdtemp(prefix="game-apk-", dir=output))
    classes, dex = package / "classes", package / "dex"
    classes.mkdir(); dex.mkdir()
    sources = sorted((ROOT / "local/deps/sdl/android-project/app/src/main/java").rglob("*.java"))
    sources += sorted((kit / "packaging/android/java").rglob("*.java"))
    if not sources:
        raise ValueError("Pinned SDL/PortableKit Java sources are missing")
    arguments = ["-nowarn", "-encoding", "UTF-8", "--release", "11",
                 "-classpath", str(android_jar), "-d", str(classes)]
    arguments += [str(path) for path in sources]
    argfile = package / "javac.args"
    argfile.write_text("\n".join('"' + item.replace("\\", "/").replace('"', '\\"') + '"'
                                   for item in arguments), encoding="utf-8")
    run(javac, "@" + str(argfile))
    classjar = package / "classes.jar"
    with zipfile.ZipFile(classjar, "w") as archive:
        for path in sorted(classes.rglob("*.class")):
            archive.write(path, path.relative_to(classes).as_posix())
    run(java, "-Xmx1g", "-cp", d8, "com.android.tools.r8.D8", "--release",
        "--lib", android_jar, "--min-api", "29", "--output", dex, classjar)
    compiled = []
    for label, res in (("base", base_res), ("game", game_res)):
        target = package / (label + "-res.zip")
        run(aapt, "compile", "--dir", res, "-o", target)
        compiled.append(target)
    assets = package / "assets/licenses"
    assets.mkdir(parents=True)
    for name, path in notices.items():
        shutil.copyfile(path, assets / name)
    fonts = package / "assets/fonts"
    fonts.mkdir()
    shutil.copyfile(font, fonts / font.name)
    unsigned = package / "unsigned.apk"
    version_code = subprocess.check_output(["git", "rev-list", "--count", "HEAD"], cwd=ROOT, text=True).strip()
    version_name = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    run(aapt, "link", "-I", android_jar, "--manifest", manifest,
        "--min-sdk-version", "29", "--target-sdk-version", "35",
        "--version-name", version_name, "--version-code", version_code,
        "--debug-mode", "-A", package / "assets", "-o", unsigned,
        "--rename-manifest-package", APP_ID, compiled[0], "-R", compiled[1], "--auto-add-overlay")
    packed = {}
    for name, path in native.items():
        packed_path = package / name
        shutil.copyfile(path, packed_path)
        run(strip, "--strip-unneeded", packed_path)
        elf_alignment(packed_path.read_bytes(), ABI)
        packed[name] = packed_path
    with zipfile.ZipFile(unsigned, "a", compression=zipfile.ZIP_STORED) as archive:
        for name, path in packed.items():
            archive.write(path, f"lib/{ABI}/{name}")
        dex_files = sorted(dex.glob("classes*.dex"))
        if not dex_files:
            raise ValueError("D8 produced no dex files")
        for path in dex_files:
            archive.write(path, path.name)
    aligned = package / "aligned.apk"
    run(zipalign, "-P", "16", "-f", "4", unsigned, aligned)
    key = output / "debug.keystore"
    if not key.exists():
        run(keytool, "-genkeypair", "-keystore", key, "-storepass", "android",
            "-keypass", "android", "-alias", "androiddebugkey",
            "-dname", "CN=Android Debug,O=Android,C=US", "-keyalg", "RSA",
            "-keysize", "2048", "-validity", "10000", "-noprompt")
    target = output / "azazel6-tekken6-hybrid-arm64-debug.apk"
    run(java, "-jar", signer, "sign", "--ks", key, "--ks-pass", "pass:android",
        "--key-pass", "pass:android", "--ks-key-alias", "androiddebugkey",
        "--out", target, aligned)
    run(java, "-jar", signer, "verify", "--verbose", target)
    run(zipalign, "-c", "-P", "16", "4", target)
    report = {"kind": "tekken6_hybrid_aot_interpreter", "abi": ABI,
              "apk_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
              "aot_source_sha256": aot_hash,
              "aot_executable_sha256": aot_report["executable_sha256"],
              "font_sha256": FONT_SHA256,
              "libraries": {name: hashlib.sha256(path.read_bytes()).hexdigest()
                            for name, path in packed.items()},
              "verified": ["elf_abi", "16k_load_alignment", "android29_symbol_imports",
                           "apk_signature", "16k_zip_alignment"],
              "device_execution": "NOT TESTED", "game_rendering": "NOT TESTED"}
    (output / "build-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Signed hybrid APK: {target}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=os.environ.get("ANDROID_HOME"))
    parser.add_argument("--java-home", type=Path, default=os.environ.get("JAVA_HOME"))
    parser.add_argument("--build", type=Path, default=ROOT / "local/build/portablekit-android-hybrid")
    parser.add_argument("--sdl-lib", type=Path, default=ROOT / "local/android-host/native/sdl")
    parser.add_argument("--output", type=Path, default=ROOT / "local/android-game")
    parser.add_argument("--font-dir", type=Path, default=ROOT / "local/android-game/fonts")
    args = parser.parse_args()
    if not args.sdk or not args.java_home:
        raise ValueError("Set --sdk/--java-home or ANDROID_HOME/JAVA_HOME")
    package_apk(args.sdk, args.java_home, args.build, args.sdl_lib, args.output, args.font_dir)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"android_game: {error}", file=sys.stderr)
        sys.exit(1)
