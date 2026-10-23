"""Exact-input, local-only Allegrex evidence; never authorizes AOT coverage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from analyze import ROOT, local_output, execute
from elf import HEADER, PROGRAM, SECTION, ValidationError, load_profile, validate
from inspect_root import inspect_root, reviewed_transfers
from psp_module import module_report

REA_REVISION = "bc411c24befceced5a9ea1ca1c1ce123fd02bf48"


def rea_preflight(elf, system=None, machine=None):
    """Necessary-condition check, not a replacement for REA's full admission gate."""
    header = HEADER.unpack_from(elf.data)
    reasons = []
    if (system or platform.system()) != "Linux" or (machine or platform.machine()).lower() not in ("x86_64", "amd64"):
        reasons.append("PSP provider requires Linux x64")
    if elf.flags != 0x10A23001:
        reasons.append("PSP provider requires e_flags=0x10a23001")
    # Inspect declarations, never scan arbitrary payload for a plausible record.
    programs = [PROGRAM.unpack_from(elf.data, header[5] + i * header[9]) for i in range(header[10])]
    sections = [SECTION.unpack_from(elf.data, header[6] + i * header[11]) for i in range(header[12])]
    abi_declared = any(p[0] == 0x70000003 for p in programs) or any(s[1] == 0x7000002A for s in sections)
    if not abi_declared:
        reasons.append("No declared PT_MIPS_ABIFLAGS or SHT_MIPS_ABIFLAGS record; missing ABI declarations are not inferred")
    return {"source_revision": REA_REVISION, "status": "REJECTED_PREREQUISITES" if reasons else "REQUIRES_REA_VALIDATION",
            "elf_flags": f"0x{elf.flags:08X}", "section_count": header[12],
            "abi_record_declared": abi_declared, "reasons": reasons,
            "limitations": "Static necessary conditions only; REA was not executed and no metadata was changed"}


def addresses(values):
    result = [int(v, 16) for v in values]
    if not 1 <= len(result) <= 16 or len(set(result)) != len(result) or any(
            not 0 <= v <= 0xFFFFFFFC or v & 3 for v in result):
        raise ValidationError("Require 1..16 unique aligned 32-bit guest addresses")
    return result


def verify_observations(elf, result, roots):
    """Reject wrong images/rebases and never upgrade decompilation into truth."""
    if result.get("input_sha256") != elf.report()["sha256"] or result.get("language") != "Allegrex:LE:32:default":
        raise ValidationError("Ghidra image fingerprint/language mismatch")
    rows = result.get("functions", [])
    if [int(row["requested"], 16) for row in rows] != roots:
        raise ValidationError("Ghidra requested-address mismatch")
    for row in rows:
        instructions = row.get("instructions", [])
        if not 1 <= len(instructions) <= 4096:
            raise ValidationError("Ghidra instruction count outside bounds")
        for instruction in instructions:
            pc = int(instruction["address"], 16)
            if pc & 3 or not 0 <= pc <= 0xFFFFFFFC:
                raise ValidationError("Invalid Ghidra guest instruction address")
            raw = bytes.fromhex(instruction["bytes"])
            offset = elf.guest_to_offset(pc, 4)
            if len(raw) != 4 or raw != elf.data[offset:offset + 4]:
                raise ValidationError("Ghidra instruction bytes differ from verified ELF")
    return result


def headless_launcher(installation, java_home, output):
    """Invoke the distribution's Java entry point without Windows batch expansion.

    Upstream .bat launchers reject ! in paths (including this checkout's path).
    Read their VM options; leave the distribution and host Java configuration intact.
    """
    java = java_home / 'bin' / ('java.exe' if sys.platform == 'win32' else 'java') if java_home else shutil.which('java')
    if not java or not Path(java).is_file():
        raise ValidationError("Supply a compatible JDK using --java-home")
    utility = installation / 'Ghidra/Framework/Utility/lib/Utility.jar'
    if not utility.is_file():
        raise ValidationError("Ghidra Utility.jar is missing")
    suffix = 'WINDOWS' if sys.platform == 'win32' else 'MACOS' if sys.platform == 'darwin' else 'LINUX'
    options = []
    for line in (installation / 'support/launch.properties').read_text().splitlines():
        key, sep, value = line.partition('=')
        if sep and key in ('VMARGS', 'VMARGS_' + suffix):
            options.append(value)
    home = output / 'home'
    home.mkdir()
    return [java, *options, '-Xmx2G', '-Djava.awt.headless=true', f'-Duser.home={home}',
            f'-Djava.io.tmpdir={home}', '-cp', utility, 'ghidra.Ghidra', 'ghidra.app.util.headless.AnalyzeHeadless']


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--root", action="append", required=True)
    parser.add_argument("--output", type=Path, required=True, help="Fresh directory beneath local/")
    parser.add_argument("--ghidra", type=Path, help="Optional Ghidra installation; omit for Python evidence only")
    parser.add_argument("--java-home", type=Path)
    args = parser.parse_args(argv)
    elf = validate(args.input, load_profile(ROOT / "profiles/tekken6/config/profile.json"))
    roots = addresses(args.root)
    startup = json.loads((ROOT / "profiles/tekken6/config/startup.json").read_text())
    module = module_report(elf)
    known = reviewed_transfers(elf, startup)
    reports = [inspect_root(elf, root, [int(r, 16) for r in startup["roots"]],
                            [int(i["stub"], 16) for i in module["imports"]],
                            known_transfers=known, inspect_existing_root=True) for root in roots]
    local_output(args.output)  # Resolve aliases for the ownership check.
    out = args.output.absolute()  # Preserve a caller-supplied Windows drive alias.
    if out.exists() and any(out.iterdir()):
        raise ValidationError("Use a fresh evidence directory; stale Ghidra output is refused")
    out.mkdir(parents=True, exist_ok=True)
    report = {"schema_version": 1, "input_sha256": elf.report()["sha256"],
              "status": "STATIC_RESEARCH_ONLY", "rea": rea_preflight(elf),
              "roots": reports, "imports": module["imports"],
              "limitations": ["No AOT authorization or guest execution", "CFG boundaries are conservative",
                              "Decompiled types, indirect targets and VFPU prefix semantics require independent verification"]}
    (out / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.ghidra:
        launcher = headless_launcher(args.ghidra.absolute(), args.java_home.resolve() if args.java_home else None, out)
        extension = args.ghidra / 'Ghidra/Extensions/ghidra-allegrex'
        identity = {}
        for path in sorted(extension.rglob('*')):
            if path.is_file():
                with path.open('rb') as stream:
                    identity[path.relative_to(extension).as_posix()] = hashlib.file_digest(stream, 'sha256').hexdigest()
        if not identity:
            raise ValidationError('Install the matching Allegrex extension inside the selected distribution')
        report['extension_sha256'] = identity
        # Ghidra uses Windows application-data variables before Java user.home.
        os.environ['APPDATA'] = str(out / 'home')
        os.environ['LOCALAPPDATA'] = str(out / 'home')
        command = [*launcher, out, "azazel6", "-import", args.input.absolute(),
                   "-processor", "Allegrex:LE:32:default", "-cspec", "default",
                   "-noanalysis", "-scriptPath", Path(__file__).absolute().parent / "ghidra", "-postScript",
                   "AzazelEvidence.java", out / "ghidra.json", report["input_sha256"],
                   *[f"0x{r:08X}" for r in roots], "-deleteProject"]
        execute(command, out / "ghidra.log", 240)
        observations = verify_observations(elf, json.loads((out / "ghidra.json").read_text()), roots)
        report["ghidra"] = observations
        (out / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Static evidence: {out / 'evidence.json'}; REA: {report['rea']['status']}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"ghidra_probe: {error}", file=sys.stderr)
        sys.exit(1)
