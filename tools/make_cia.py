#!/usr/bin/env python3
"""Make release/kingdom.cia from the ELF built by tools/build_3ds.py.

  python tools/make_cia.py --makerom <makerom> --bannertool <bannertool>

Needs the ELF (build3ds/<product>.elf) and SMDH from a finished build, and the converted media (translit_assets/),
which become the CIA's RomFS. The banner is the king galloping at dusk (tools/make_banner.py, drawn from the game's sprites) with a silent sound.
"""
import argparse
import subprocess
import wave
from pathlib import Path

import make_banner

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--makerom", required=True)
    ap.add_argument("--bannertool", required=True)
    ap.add_argument("--product", default="kingdom")
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    work = ROOT / "build3ds"
    elf, smdh = work / f"{a.product}.elf", work / f"{a.product}.smdh"
    if not elf.exists() or not smdh.exists():
        raise SystemExit("build first: python tools/build_release.py (missing %s or %s)" % (elf, smdh))
    banner_png = work / "banner.png"
    make_banner.make(banner_png)
    wav = work / "banner.wav"
    with wave.open(str(wav), "wb") as w:
        w.setnchannels(2), w.setsampwidth(2), w.setframerate(32728)
        w.writeframes(b"\0" * (32728 * 4 * 2))
    bnr = work / "banner.bnr"
    subprocess.run([a.bannertool, "makebanner", "-i", str(banner_png), "-a", str(wav), "-o", str(bnr)], check=True)
    out = a.out or ROOT / f"release/{a.product}.cia"
    out.parent.mkdir(exist_ok=True)
    subprocess.run([a.makerom, "-f", "cia", "-o", str(out), "-elf", str(elf), "-rsf", str(ROOT / "tools/kingdom.rsf"),
                    "-icon", str(smdh), "-banner", str(bnr), "-romfs", str(ROOT / "translit_assets"),
                    "-target", "t", "-exefslogo"], check=True)
    print("BUILT", out, out.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
