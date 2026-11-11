#!/usr/bin/env python3
"""Copy the 3DSX and its data to an SD card / emulator sdmc folder.

  python tools/deploy_sd.py --sd-root "%APPDATA%/Azahar/sdmc"

The game's converted assets are embedded in the .3dsx (romfs), so only the
.3dsx is copied. progress.sav, settings.cfg and the logs written by the game
stay in sdmc:/3ds/<product>/ and are never touched.

Sound needs the DSP firmware: libctru's ndspInit fails with 0xD880A7FA without
sdmc:/3ds/dspfirm.cdc and the game then runs silent. A real console needs a
dump of its own firmware (DSP1 homebrew). Azahar's HLE audio accepts any
file, so for an emulator sdmc (path contains "Azahar"/"Citra", or
--dsp-stub) a 32 KB placeholder is created when no dspfirm.cdc exists. A real
SD card is never given the placeholder.
"""
import argparse, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KEEP = {"progress.sav", "settings.cfg", "runtime.log", "session.log"}

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--sd-root", required=True, type=Path)
ap.add_argument("--product", default="kingdom_translit", help="base name of the .3dsx (kingdom for the release build)")
ap.add_argument("--dsp-stub", action="store_true", help="create a placeholder dspfirm.cdc (emulators only)")
ap.add_argument("--with-sd-data", action="store_true",
                help="legacy: also copy translit_assets next to the .3dsx (the game uses its embedded romfs by default)")
ap.add_argument("--clean-old-data", action="store_true",
                help="delete asset files an older build left on the SD card (saves, settings and logs are kept)")
args = ap.parse_args()
dest = args.sd_root / "3ds"
dest.mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT / f"out/{args.product}.3dsx", dest / f"{args.product}.3dsx")
src = ROOT / "translit_assets"
data = dest / args.product
copied = removed = 0
if args.with_sd_data:
    wanted = set()
    for f in src.rglob("*"):
        if f.is_file():
            rel = f.relative_to(src)
            wanted.add(rel)
            t = data / rel
            t.parent.mkdir(parents=True, exist_ok=True)
            if not t.exists() or t.stat().st_size != f.stat().st_size or t.stat().st_mtime < f.stat().st_mtime:
                shutil.copy2(f, t)
                copied += 1
if args.clean_old_data and data.exists() and not args.with_sd_data:
    for t in sorted(data.rglob("*")):
        if t.is_file() and t.name not in KEEP and not t.name.endswith(".tmp") and t.suffix in (".argb", ".pcm", ".font"):
            t.unlink()
            removed += 1
print(f"deployed {args.product}.3dsx to {dest} (assets are embedded in it); SD data files updated {copied}, old ones removed {removed}; saves/settings/logs kept")
dsp = dest / "dspfirm.cdc"
emulator = args.dsp_stub or any(k in str(args.sd_root).lower() for k in ("azahar", "citra"))
if not dsp.exists():
    if emulator:
        dsp.write_bytes(bytes(0x8000))
        print(f"created placeholder {dsp} (emulator HLE audio)")
    else:
        print(f"NOTE: {dsp} is missing: the game will have no sound until you copy your console's DSP firmware dump there")
