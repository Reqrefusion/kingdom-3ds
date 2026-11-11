#!/usr/bin/env python3
"""Compare the top screen of an Azahar capture (tools/azahar_autotest.ps1)
with a reference: a 288x160 PNG (tests/baseline/frame_*.png, CPU translation)
or another Azahar capture (e.g. the same autotest with "cpu 1").

  python tools/compare_shot.py capture.png tests/baseline/frame_00900.png side_by_side.png

Prints the mean |difference| (0..255) and per-band means; writes reference
(top) and capture (bottom) stacked. Day frame 900 after the 6 Nov fixes: 4.7;
night (progress D1 A4 X1920 ..., freeze 600) GPU vs cpu 1: 8.8.
"""
import sys
import numpy as np
from PIL import Image


def top_screen(path):
    im = Image.open(path).convert("RGB")
    if im.size == (288, 160):
        return im
    a = np.asarray(im).astype(int)
    # the top screen is the non-black box in the upper part, inside the window chrome
    sub = a[70:530, 300:im.width - 300, :].sum(axis=2)
    ys, xs = np.where(sub > 30)
    box = (xs.min() + 300, ys.min() + 70, xs.max() + 301, ys.max() + 71)
    return im.crop(box).resize((288, 160), Image.NEAREST)


def main():
    cap, ref, out = sys.argv[1:4]
    a, b = top_screen(cap), top_screen(ref)
    A, B = np.asarray(a).astype(float), np.asarray(b).astype(float)
    print("mean diff", round(float(np.abs(A - B).mean()), 2))
    for name, (r0, r1) in {"rows 0-60": (0, 60), "rows 60-116": (60, 116), "water 116-160": (116, 160)}.items():
        print(f"{name}: capture {A[r0:r1].mean(axis=(0, 1)).round(1)}  reference {B[r0:r1].mean(axis=(0, 1)).round(1)}")
    w = Image.new("RGB", (864, 964))
    w.paste(b.resize((864, 480), Image.NEAREST), (0, 0))
    w.paste(a.resize((864, 480), Image.NEAREST), (0, 484))
    w.save(out)


if __name__ == "__main__":
    main()
