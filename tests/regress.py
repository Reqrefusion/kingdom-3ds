#!/usr/bin/env python3
"""Regression check on the host runner (python tools/build_host.py on Windows /
make -f host.mk elsewhere).

  python tests/regress.py [--exe ./kingdom_host.exe]

1. CPU mode: trace.csv and every PNG must be identical to tests/baseline
   (the pure translation; performance work must not change it). PNGs are
   compared by pixels, trace.csv ignoring line endings.
2. GPU mode (--gpu 1, display lists + CPU reference player): trace.csv must be
   identical; PNGs must stay within GPU_MAX_MEAN_DIFF (mean |difference| per
   channel, 0-255) of the baseline.
The baseline was made on x86-64 Linux with gcc. The Windows MSVC build
(tools/build_host.py) reproduces trace.csv exactly (checked 6 Nov 2026).
"""
import argparse, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "tests/baseline"

GPU_MAX_MEAN_DIFF = 1.5  # of 255; 0.2-0.7 since the copyPixels flooring fix (was 1.5-4.5)

CR = bytes([13])
LF = bytes([10])


def same_trace(a, b):
    # line endings differ between the Linux baseline and a Windows host run
    return Path(a).read_bytes().replace(CR + LF, LF) == Path(b).read_bytes().replace(CR + LF, LF)


def same_png(a, b):
    # pixel comparison: the host may write PNG without libpng (stored blocks)
    from PIL import Image
    import numpy as np
    return np.array_equal(np.asarray(Image.open(a).convert("RGB")), np.asarray(Image.open(b).convert("RGB")))


def run(exe, out, gpu):
    cmd = [exe, "--frames", "1800", "--script", str(BASE / "input.txt"), "--png-every", "300", "--out", str(out),
           "--assets", str(ROOT / "translit_assets"), "--seed", "0.5"] + (["--gpu", "1"] if gpu else [])
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(ROOT / "kingdom_host.exe"))
    exe = ap.parse_args().exe
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        cpu, gpu = Path(tmp) / "cpu", Path(tmp) / "gpu"
        cpu.mkdir(), gpu.mkdir()
        run(exe, cpu, False)
        for f in sorted(BASE.glob("*.png")) + [BASE / "trace.csv"]:
            same = same_trace(f, cpu / f.name) if f.suffix == ".csv" else same_png(f, cpu / f.name)
            ok &= same
            print(f"cpu  {f.name:18} {'same' if same else 'DIFFERENT'}")
        run(exe, gpu, True)
        same = same_trace(BASE / "trace.csv", gpu / "trace.csv")
        ok &= same
        print(f"gpu  trace.csv          {'same' if same else 'DIFFERENT'}")
        try:
            from PIL import Image
            import numpy as np
            for f in sorted(BASE.glob("frame_*.png")):
                a = np.asarray(Image.open(f).convert("RGB")).astype(int)
                b = np.asarray(Image.open(gpu / f.name).convert("RGB")).astype(int)
                diff = abs(a - b).mean()
                ok &= diff <= GPU_MAX_MEAN_DIFF
                print(f"gpu  {f.name:18} mean diff {diff:.2f}{'' if diff <= GPU_MAX_MEAN_DIFF else '  > limit'}")
        except ImportError:
            print("(PIL/numpy missing: PNG checks skipped)")
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
