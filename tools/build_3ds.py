#!/usr/bin/env python3
"""Build kingdom_translit.3dsx with an existing devkitPro installation
(same approach as ../scripts/build.py: no make needed).

  python tools/build_3ds.py --sdk C:/devkitPro
  python tools/deploy_sd.py --sd-root E:/          (copies app + data)
"""
import argparse, os, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def build(sdk: Path, product="kingdom_translit", title="Kingdom (translit)", data_root=None, romfs=None):
    arm = sdk / "devkitARM/bin"
    suffix = ".exe" if os.name == "nt" else ""
    cc = arm / ("arm-none-eabi-gcc" + suffix)
    work = ROOT / "build3ds"
    work.mkdir(exist_ok=True)
    arch = ["-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft"]
    engine = ROOT / "vendor"  # the few shared engine files (jobs_state.c, numeric.h, ...)
    flags = arch + ["-O3", "-g", "-Wall", "-Wno-unused-parameter", "-Wno-misleading-indentation", "-Wno-override-init",
                    "-ffunction-sections", "-fdata-sections", "-D__3DS__", "-std=gnu11",
                    *([f'-DKINGDOM_DATA_ROOT="{data_root}"'] if data_root else []),
                    f"-I{engine}", f"-I{sdk / 'libctru/include'}", f"-I{sdk / 'portlibs/3ds/include'}"]
    sources = sorted((ROOT / "flash").glob("*.c")) + sorted((ROOT / "flixel").glob("*.c")) + \
        sorted((ROOT / "game").glob("*.c")) + sorted((ROOT / "platform/3ds").glob("*.c")) + \
        sorted((ROOT / "platform/common").glob("*.c")) + [engine / "engine/core/jobs_state.c"]
    objects = []
    for i, src in enumerate(sources):
        obj = work / f"{i:03d}_{src.stem}.o"
        subprocess.run([str(cc), *flags, "-c", str(src), "-o", str(obj)], check=True)
        objects.append(obj)
    elf = work / f"{product}.elf"
    subprocess.run([str(cc), *arch, "-specs=3dsx.specs", "-Wl,--gc-sections", *map(str, objects),
                    f"-L{sdk / 'portlibs/3ds/lib'}", f"-L{sdk / 'libctru/lib'}",
                    "-lcitro2d", "-lcitro3d", "-lctru", "-lm", "-o", str(elf)], check=True)
    from PIL import Image
    icon = work / "icon.png"
    Image.open(ROOT / "tools/icon.png").resize((48, 48), Image.NEAREST).save(icon)
    smdh = work / f"{product}.smdh"
    subprocess.run([str(sdk / ("tools/bin/smdhtool" + suffix)), "--create", title,
                    "Line-by-line C translation of noio's Kingdom", "noio / port", str(icon), str(smdh)], check=True)
    out = ROOT / f"out/{product}.3dsx"
    out.parent.mkdir(exist_ok=True)
    subprocess.run([str(sdk / ("tools/bin/3dsxtool" + suffix)), str(elf), str(out), f"--smdh={smdh}"] +
                   ([f"--romfs={romfs}"] if romfs else []), check=True)
    print("BUILT", out, out.stat().st_size, "bytes")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sdk", default=os.environ.get("DEVKITPRO", "C:/devkitPro"))
    ap.add_argument("--product", default="kingdom_translit", help="base name of the .3dsx (kingdom for the release)")
    ap.add_argument("--title", default="Kingdom (translit)", help="name shown in the Homebrew Launcher")
    ap.add_argument("--data-root", help="SD folder for saves/settings/log, e.g. sdmc:/3ds/kingdom-3ds")
    ap.add_argument("--romfs", default=str(ROOT / "translit_assets"),
                    help="folder embedded in the .3dsx as romfs:/ (the converted original assets); '' = none")
    a = ap.parse_args()
    build(Path(a.sdk), a.product, a.title, a.data_root, a.romfs or None)
