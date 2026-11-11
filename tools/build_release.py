#!/usr/bin/env python3
"""Build the release .3dsx: original assets (fetched from the original GitHub repo) embedded in the file.

  python tools/build_release.py --sdk C:/devkitPro        (Windows)
  python3 tools/build_release.py --sdk /opt/devkitpro     (Linux / GitHub Actions)
  python tools/build_release.py --skip-fetch              (reuse translit_assets/ from an earlier run)

1. tools/fetch_assets.py  - download noio/kingdom at the pinned commit, verify, convert (needs ffmpeg + Pillow)
2. tools/build_3ds.py     - compile the C sources and pack translit_assets/ into the .3dsx as romfs:/
3. release/<product>.3dsx - the one file players copy to SD:/3ds/

Product name, Homebrew title and the SD folder for saves come from port.json.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "port.json").read_text(encoding="utf-8"))


def run(cmd):
    print("+", " ".join(str(c) for c in cmd))
    if subprocess.run([str(c) for c in cmd], cwd=ROOT).returncode != 0:
        sys.exit("step failed: " + " ".join(str(c) for c in cmd[:3]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sdk", default=os.environ.get("DEVKITPRO", CFG.get("devkitpro", "C:/devkitPro")))
    ap.add_argument("--skip-fetch", action="store_true", help="do not download/convert again if translit_assets/ exists")
    ap.add_argument("--dest", type=Path, default=ROOT / "release")
    a = ap.parse_args()
    assets = ROOT / CFG.get("assets", "translit_assets")
    if not (a.skip_fetch and (assets / "assets/gfx/title.png.argb").exists()):
        run([sys.executable, "tools/fetch_assets.py"])
    product = CFG.get("product", "kingdom_translit")
    cmd = [sys.executable, "tools/build_3ds.py", "--sdk", a.sdk, "--product", product, "--title", CFG.get("title", "Kingdom"),
           "--romfs", str(assets)]
    if CFG.get("data_root"):
        cmd += ["--data-root", CFG["data_root"]]
    run(cmd)
    a.dest.mkdir(parents=True, exist_ok=True)
    out = a.dest / (product + ".3dsx")
    shutil.copy2(ROOT / "out" / (product + ".3dsx"), out)
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    print("\nrelease file: %s  (%.1f MB)\nsha256: %s" % (out, out.stat().st_size / 1e6, digest))
    print("Copy it to SD:/3ds/%s.3dsx. Saves/settings go to %s" % (product, CFG.get("data_root", "sdmc:/3ds/" + product)))


if __name__ == "__main__":
    main()
