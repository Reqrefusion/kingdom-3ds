#!/usr/bin/env python3
"""Get the game's images, sounds, fonts and level data from the ORIGINAL open-source repo and convert them.

  python tools/fetch_assets.py              download noio/kingdom at the pinned commit, verify, convert
  python tools/fetch_assets.py --source D   use an already extracted copy of the original repo instead
  python tools/fetch_assets.py --make-manifest --source D    (maintainers) rewrite assets.sha256.json

Steps: 1. download https://github.com/<original_repo> at <original_commit> (port.json) into work/ (cached),
2. check the SHA-256 of every file the converter reads against assets.sha256.json (so a changed or wrong
copy is refused), 3. tools/convert_assets.py -> translit_assets/ (PNG -> .argb, MP3 -> .pcm via ffmpeg, fonts,
level data), 4. copy the original LICENSE.txt into translit_assets/ (the license requires it in every copy,
and translit_assets/ is what gets embedded in the .3dsx as romfs:/).

Needs: Python 3.9+, Pillow, ffmpeg on PATH (audio). Nothing is uploaded anywhere.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "port.json").read_text(encoding="utf-8"))
REPO = CFG["original_repo"]
COMMIT = CFG["original_commit"]
MANIFEST = ROOT / "assets.sha256.json"
OUT = ROOT / CFG.get("assets", "translit_assets")


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def inputs(src: Path):
    """every file of the original repo that tools/convert_assets.py reads, plus the license"""
    files = sorted((src / "assets" / "gfx").glob("*.png")) + sorted((src / "assets").rglob("*.mp3"))
    files += [src / "org/flixel/data/default.png", src / "org/flixel/data/nokiafc22.ttf", src / "assets/04b03.ttf",
              src / "assets/levels/compiled/fields.oel", src / "WeatherPresets.as", src / "LICENSE.txt"]
    return files


def download() -> Path:
    work = ROOT / "work"
    work.mkdir(exist_ok=True)
    src = work / f"kingdom-{COMMIT}"
    if (src / "LICENSE.txt").exists():
        return src
    zpath = work / f"kingdom-{COMMIT}.zip"
    if not zpath.exists():
        url = f"https://codeload.github.com/{REPO}/zip/{COMMIT}"
        print("downloading", url)
        req = urllib.request.Request(url, headers={"User-Agent": "kingdom-3ds-build"})
        with urllib.request.urlopen(req, timeout=120) as r, open(zpath, "wb") as f:
            shutil.copyfileobj(r, f)
    print("extracting", zpath.name)
    tmp = work / f".extract-{COMMIT}"
    shutil.rmtree(tmp, ignore_errors=True)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(tmp)
    roots = [d for d in tmp.iterdir() if d.is_dir()]
    if len(roots) != 1:
        sys.exit("unexpected archive layout")
    shutil.rmtree(src, ignore_errors=True)
    roots[0].rename(src)
    shutil.rmtree(tmp, ignore_errors=True)
    return src


def verify(src: Path):
    expected = json.loads(MANIFEST.read_text(encoding="utf-8"))["files"]
    bad = []
    for rel, digest in expected.items():
        f = src / rel
        if not f.exists() or sha256(f) != digest:
            bad.append(rel)
    extra = [str(f.relative_to(src)).replace("\\", "/") for f in inputs(src) if str(f.relative_to(src)).replace("\\", "/") not in expected]
    if bad or extra:
        sys.exit("original files do not match assets.sha256.json (wrong commit or changed repo):\n  changed/missing: %s\n  not in manifest: %s"
                 % (bad[:8], extra[:8]))
    print("verified %d original files against assets.sha256.json" % len(expected))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", type=Path, help="extracted original repo (default: download the pinned commit)")
    ap.add_argument("--make-manifest", action="store_true")
    args = ap.parse_args()
    src = args.source.resolve() if args.source else download()
    if args.make_manifest:
        files = {str(f.relative_to(src)).replace("\\", "/"): sha256(f) for f in inputs(src)}
        MANIFEST.write_text(json.dumps({"repo": REPO, "commit": COMMIT, "files": files}, indent=1) + "\n", encoding="utf-8")
        print("wrote", MANIFEST, len(files), "files")
        return
    verify(src)
    before = {p: sha256(p) for p in (ROOT / "game/level_fields.c", ROOT / "game/WeatherPresets.c", ROOT / "game/WeatherPresets.h") if p.exists()}
    rc = subprocess.run([sys.executable, str(ROOT / "tools/convert_assets.py"), "--source", str(src), "--out", str(OUT), "--audio"]).returncode
    if rc != 0:
        sys.exit("convert_assets.py failed")
    for p, h in before.items():
        if sha256(p) != h:
            print("NOTE: %s was regenerated and differs from the committed copy: review it" % p.relative_to(ROOT))
    shutil.copy2(src / "LICENSE.txt", OUT / "LICENSE.txt")
    n = sum(1 for _ in OUT.rglob("*") if _.is_file())
    print("assets ready: %s (%d files, original LICENSE.txt included)" % (OUT, n))


if __name__ == "__main__":
    main()
