"""Audit tracked/staged paths, content signatures, Markdown links and ignore rules."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN = {".iso", ".cso", ".bin", ".elf", ".prx", ".dump", ".dmp", ".png", ".jpg",
             ".dds", ".ktx", ".ktx2", ".zip", ".7z", ".mp4", ".wav", ".pem", ".key",
             ".pmf", ".at3", ".sfo", ".idx", ".ini", ".jpeg", ".webp", ".mp3", ".ogg"}
PRIVATE_DIRS = {"local", "generated", "assets", "textures", "cheats", "saves", "__pycache__"}


def check():
    errors = []
    paths = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    for name in filter(None, paths):
        path = Path(name)
        if path.suffix.lower() in FORBIDDEN or PRIVATE_DIRS.intersection(path.parts) or path.name.startswith(".env"):
            errors.append(f"Forbidden tracked path: {name}")
            continue
        # Inspect index content: catches forbidden bytes even if the working file changed.
        data = subprocess.check_output(["git", "show", f":{name}"], cwd=ROOT)
        if len(data) > 1024 * 1024 or data.startswith((b"\x7fELF", b"~PSP", b"MZ")):
            errors.append(f"Unexpected binary/large content: {name}")
        if path.suffix == ".cpp" and b"kEntryIds_" in data:
            errors.append(f"Possible game-derived generated source: {name}")
        if re.search(rb"(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)", data):
            errors.append(f"Possible credential: {name}")
        if path.suffix == ".md":
            text = data.decode("utf-8")
            for target in re.findall(r"\]\(([^)]+)\)", text):
                if "://" in target or target.startswith("#"):
                    continue
                relative = target.split("#")[0]
                if not (ROOT / path.parent / relative).exists():
                    errors.append(f"Broken Markdown link in {name}: {target}")
    forbidden_samples = ["GAME.ISO", "EBOOT.BIN", "game.elf", "local/raw.cpp", "generated/part.cpp",
                         "profiles/tekken6/generated/part.cpp", "dump.dmp", "private.key", ".env.local"]
    for sample in forbidden_samples:
        result = subprocess.run(["git", "check-ignore", "-q", "--no-index", sample], cwd=ROOT)
        if result.returncode:
            errors.append(f"Missing ignore rule: {sample}")
    return errors


if __name__ == "__main__":
    errors = check()
    print("\n".join(errors) if errors else "Repository content, links and ignore rules passed")
    sys.exit(bool(errors))
