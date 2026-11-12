#!/usr/bin/env python3
"""Copy the publishable part of this workspace into the GitHub repository folder.

  python tools/export_repo.py --dest <path to your clone of kingdom-3ds>

What goes in: the sources (as3/ flash/ flixel/ game/ platform/ tools/ tests/ vendor/), the docs, and the repository
files from tools/repo_template/ (README, COPYRIGHT, .gitignore, .gitattributes, BUILD.cmd, port.json).
The LICENSE is the ORIGINAL Kingdom license (from the downloaded original repo in work/).
What stays out: build output, captures, downloaded or converted media, executables, the old prototype's files.

It never touches .git and only deletes files that an earlier export wrote (listed in .export-manifest). It does not
commit or push: review `git status` in the destination yourself.
"""
import argparse
import json
import os
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "port.json").read_text(encoding="utf-8"))
TEMPLATE = ROOT / "tools/repo_template"

SOURCE_DIRS = ["as3", "flash", "flixel", "game", "platform", "tests", "tools", "vendor"]
SKIP_NAMES = {"__pycache__", "repo_template", "parity_root", "captures", "build_host", "build3ds", "out", "work"}
SKIP_SUFFIX = {".exe", ".obj", ".o", ".pyc", ".elf", ".3dsx", ".smdh", ".zip", ".log"}
ROOT_FILES = {  # source -> destination (relative to the repository root)
    "assets.sha256.json": "assets.sha256.json",
    "PORTING_GUIDE.md": "docs/PORTING_GUIDE.md",
    "host.mk": "host.mk",
    "PARITY_REPORT.md": "docs/PARITY_REPORT.md",
}
TEMPLATE_FILES = {
    "README.md": "README.md", "COPYRIGHT.md": "COPYRIGHT.md", "BUILD.cmd": "BUILD.cmd", "port.json": "port.json",
    "dot-gitignore": ".gitignore", "dot-gitattributes": ".gitattributes",
    "workflows/build-3dsx.yml": ".github/workflows/build-3dsx.yml",
    "workflows/build-cia.yml": ".github/workflows/build-cia.yml", "docs/BUILDING.md": "docs/BUILDING.md",
}
NOTE = ("> Internal working notes of the development (in Turkish). They mention paths of the original workspace "
        "(`../source`, `../work`, `captures/`); the repository itself needs none of them.\n\n")


def want(path: Path) -> bool:
    return not (set(path.parts) & SKIP_NAMES) and path.suffix not in SKIP_SUFFIX


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dest", required=True, type=Path)
    args = ap.parse_args()
    dest = args.dest.resolve()
    if not (dest / ".git").exists():
        raise SystemExit("%s is not a git repository (no .git)" % dest)
    commit = CFG["original_commit"]
    license_src = ROOT / "work" / f"kingdom-{commit}" / "LICENSE.txt"
    if not license_src.exists():
        raise SystemExit("run `python tools/fetch_assets.py` first: the original LICENSE.txt is needed (%s)" % license_src)

    plan = {}  # destination relative path -> (source path, transform)
    for d in SOURCE_DIRS:
        for f in sorted((ROOT / d).rglob("*")):
            if f.is_file() and want(f.relative_to(ROOT)):
                plan[f.relative_to(ROOT).as_posix()] = (f, None)
    for src, dst in ROOT_FILES.items():
        if (ROOT / src).exists():
            plan[dst] = (ROOT / src, "note" if src == "CLAUDE.md" else None)
    for src, dst in TEMPLATE_FILES.items():
        plan[dst] = (TEMPLATE / src, "crlf" if dst.endswith(".cmd") else None)
    plan["LICENSE"] = (license_src, None)

    manifest = dest / ".export-manifest"
    old = set(manifest.read_text(encoding="utf-8").split()) if manifest.exists() else set()
    removed = 0
    for rel in sorted(old - set(plan)):
        p = dest / rel
        if p.is_file():
            p.unlink()
            removed += 1
    total = 0
    for rel, (src, transform) in sorted(plan.items()):
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        if transform == "note":  # also hide the local Windows user name in paths
            text = src.read_text(encoding="utf-8")
            text = re.sub(r"([A-Za-z]:[/\\]+Users[/\\]+)[^/\\\s`'\"]+", r"\1<you>", text)
            out.write_bytes(NOTE.encode("utf-8") + text.encode("utf-8"))
        elif transform == "crlf":
            out.write_bytes(src.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))
        else:
            shutil.copy2(src, out)
        total += out.stat().st_size
    manifest.write_text("\n".join(sorted(plan)) + "\n", encoding="utf-8")
    big = [(rel, (dest / rel).stat().st_size) for rel in plan if (dest / rel).stat().st_size > 2_000_000]
    print("exported %d files (%.1f MB) to %s; %d old files removed" % (len(plan), total / 1e6, dest, removed))
    for rel, n in big:
        print("  large file: %s (%.1f MB)" % (rel, n / 1e6))
    print("Not committed. Next: cd %s && git status" % dest)


if __name__ == "__main__":
    main()
