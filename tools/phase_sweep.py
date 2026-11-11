#!/usr/bin/env python3
"""Compare the 3DS GPU path with the CPU compositor (= the translation as
written) for several weather phases, in Azahar.

  python tools/phase_sweep.py [--phases 1 3 5 11 ...] [--out sweep_out]

For each phase A it starts new PlayState("D2 A<A> X1920 ...") with the fixed
seed, freezes at frame 90 and captures the top screen twice (cpu 0 / cpu 1)
with tools/azahar_autotest.ps1, then prints the mean |difference| (0..255,
tools/compare_shot.py). Day/night after the 6 Nov blend fix: 3.8 / 8.8.
A value well above ~10 means the GPU path (platform/3ds/gpu.c) renders that
weather differently from the original. CPU captures take about a minute each.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PS = ROOT / "tools/azahar_autotest.ps1"
SAVE = "D2 A{a} X1920 B2 P2 F2 H2 W000011 C1 S00 G4"


def capture(a, cpu, out):
    script = f"seed 0.5\nfreeze 90\ncpu {cpu}\nprogress {SAVE.format(a=a)}"
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(PS), "-Script", script,
                    "-Wait", "75" if cpu else "30", "-Out", str(out)], check=True, stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--phases", type=int, nargs="*", default=[1, 3, 5, 11, 12, 16, 21, 24, 29, 33, 38])
    ap.add_argument("--out", default=str(ROOT / "sweep_out"))
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(exist_ok=True)
    for a in args.phases:
        g, c = out / f"p{a}_gpu.png", out / f"p{a}_cpu.png"
        capture(a, 0, g)
        capture(a, 1, c)
        r = subprocess.run([sys.executable, str(ROOT / "tools/compare_shot.py"), str(g), str(c), str(out / f"p{a}_cmp.png")],
                           capture_output=True, text=True)
        print(f"phase A{a}: {r.stdout.splitlines()[0] if r.stdout else r.stderr.strip()[-200:]}", flush=True)


if __name__ == "__main__":
    main()
