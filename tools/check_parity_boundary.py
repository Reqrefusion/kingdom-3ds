#!/usr/bin/env python3
"""Keeps the 3DS-only extras (stereo 3D, water depth, GPU recorder) OUT of the parity area.

  python tools/check_parity_boundary.py

game/ is the line-by-line translation of the game: it must not mention Stereo_*, Gpu_* or Gpu3ds_*.
flixel/ is the translated engine: only the few pre-existing one-line hooks are allowed
(Stereo_dx per drawn object and the Stereo_depth pointer; GPU adapter lines are labelled).
Everything new for 3D / water depth / edge handling belongs in flash/ (adapter layer) or platform/.
Exit code 1 when a new hook shows up in game/ or flixel/.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r"\b(Stereo_\w+|Gpu_\w+|Gpu3ds_\w+)")
# allowed lines per file (token mentions in flixel/): the engine hooks that existed before the 3D work
ALLOWED = {
    "flixel/FlxSprite.c": {"Stereo_dx": 2, "Stereo_depth": 3, "Gpu_drawSprite": 1, "Gpu_recording": 1},
    "flixel/FlxText.c": {"Gpu_recording": 4},
    "flixel/FlxTilemap.c": {"Stereo_dx": 2, "Stereo_depth": 2},
}


def main():
    bad = []
    for sub in ("game", "flixel"):
        for f in sorted((ROOT / sub).glob("*.c")):
            rel = f"{sub}/{f.name}"
            counts = {}
            for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                for t in TOKEN.findall(line):
                    counts[t] = counts.get(t, 0) + 1
            allowed = ALLOWED.get(rel, {})
            for t, n in counts.items():
                if n > allowed.get(t, 0):
                    bad.append(f"{rel}: {t} appears {n}x (allowed {allowed.get(t, 0)})")
    if bad:
        print("3D / GPU hooks outside the adapter layer:")
        print("\n".join("  " + b for b in bad))
        return 1
    print("parity boundary OK: game/ has no Stereo/GPU hooks; flixel/ has only the known engine hooks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
