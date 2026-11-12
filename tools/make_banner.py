#!/usr/bin/env python3
"""Banner image (256x128) for the CIA: the king galloping across the hills at dusk, drawn from the game's own sprites.

  python tools/make_banner.py [out.png]

Reads the original graphics that tools/fetch_assets.py downloaded (work/kingdom-<commit>/assets/gfx).
"""
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "port.json").read_text(encoding="utf-8"))
GFX = ROOT / "work" / f"kingdom-{CFG['original_commit']}" / "assets" / "gfx"


def img(name):
    return Image.open(GFX / name).convert("RGBA")


def tint(sil, rgb):
    """The skylines are white silhouettes (the game colors them); give them a color."""
    out = Image.new("RGBA", sil.size, rgb + (255,))
    out.putalpha(sil.getchannel("A"))
    return out


def make(path):
    W, H = 256, 128
    top, bottom = (36, 52, 112), (246, 170, 116)  # dusk sky, dark above, warm at the horizon
    banner = Image.new("RGBA", (W, H))
    px = banner.load()
    for y in range(H):
        t = min(1.0, y / 96)
        c = tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for x in range(W):
            px[x, y] = c + (255,)

    sun = tint(img("sunmoon.png").crop((0, 0, 32, 32)), (255, 226, 150)).resize((64, 64), Image.NEAREST)
    banner.alpha_composite(sun, (168, 8))

    hills = tint(img("skyline_hills.png"), (122, 96, 150))
    banner.alpha_composite(hills.crop((60, 84, 60 + W, 84 + 70)), (0, 52))
    trees = tint(img("skyline_trees.png"), (52, 78, 82))
    banner.alpha_composite(trees.crop((300, 50, 300 + W, 50 + 80)), (0, 56))

    tiles = img("tiles.png").crop((240, 0, 240 + 32, 32))  # grass and soil
    ground = tiles.resize((64, 64), Image.NEAREST)
    for x in range(0, W, 64):
        banner.alpha_composite(ground, (x, 100))

    king = img("king.png").crop((3 * 64, 0, 4 * 64, 64)).resize((128, 128), Image.NEAREST)  # a stretched gallop frame
    banner.alpha_composite(king, (88, -12))

    dust = tint(img("dust.png").crop((0, 0, 16, 16)), (232, 205, 170)) if (GFX / "dust.png").exists() else None
    if dust is not None:
        for i, (x, y) in enumerate([(70, 100), (52, 104), (38, 98)]):
            banner.alpha_composite(dust.resize((16 + 4 * i, 16 + 4 * i), Image.NEAREST), (x, y))
    banner.convert("RGB").save(path)


if __name__ == "__main__":
    make(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build3ds/banner.png")
