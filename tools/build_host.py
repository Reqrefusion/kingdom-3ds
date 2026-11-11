#!/usr/bin/env python3
"""Build the host runner (kingdom_host.exe) with the MSVC Build Tools that are
already installed (no gcc/make/libpng needed).

  python tools/build_host.py [--out kingdom_host.exe] [--scenarios]

Finds Visual Studio through vswhere (-all, so BuildTools installs outside
Program Files are found too), loads vcvars64.bat in a child cmd and compiles
the same sources as host.mk (without LUME_HOST_JOBS: no pthreads on MSVC, the
worker slot falls back to inline execution, results are identical). platform/host/main.c writes PNGs with its own
zlib-stored writer unless KINGDOM_HOST_LIBPNG is defined (host.mk path).
--scenarios builds tests/translit_scenarios.c (the oracle runner's binary)
instead of the PNG runner.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    out = subprocess.run([str(vswhere), "-all", "-products", "*", "-property", "installationPath"],
                         capture_output=True, text=True).stdout.split("\n")
    for line in out:
        p = Path(line.strip()) / "VC/Auxiliary/Build/vcvars64.bat"
        if line.strip() and p.is_file():
            return p
    sys.exit("no Visual Studio / Build Tools with vcvars64.bat found")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "kingdom_host.exe"))
    ap.add_argument("--scenarios", action="store_true")
    ap.add_argument("--main", help="build this C file (with its own main) instead of platform/host/main.c")
    args = ap.parse_args()
    engine = ROOT / "vendor"  # the few shared engine files (jobs_state.c, numeric.h, ...)
    files = [str(f) for d in ("flash", "flixel", "game") for f in sorted((ROOT / d).glob("*.c"))]
    if args.main:
        files.append(str(Path(args.main).resolve()))
    elif args.scenarios:
        files.append(str(ROOT / "tests/translit_scenarios.c"))
    else:
        files.append(str(ROOT / "platform/host/main.c"))
    files += [str(ROOT / "platform/host/media.c"), str(ROOT / "platform/common/session.c"),
              str(ROOT / "platform/common/stereo.c"), str(ROOT / "platform/common/parallel.c"),
              str(engine / "engine/core/jobs_state.c")]
    flags = ["/nologo", "/std:c11", "/O2", "/W1", "/wd4244", "/wd4267", "/wd4996", "/D_CRT_SECURE_NO_WARNINGS",
             f"/I{engine}", f"/I{ROOT}"]
    obj = ROOT / "build_host"
    obj.mkdir(exist_ok=True)
    lines = ["@echo off", f'call "{vcvars()}" >nul']
    objs = []
    for f in files:  # unique object names: flash/media.c and platform/host/media.c share a stem
        rel = Path(f).resolve()
        try:
            name = "_".join(rel.relative_to(ROOT).with_suffix("").parts)
        except ValueError:
            name = "engine_" + rel.stem
        o = obj / (name + ".obj")
        objs.append(str(o))
        lines.append(f'cl {" ".join(flags)} /c "{f}" /Fo"{o}" || exit /b 1')
    rsp = obj / "link.rsp"  # response file: the command line would exceed cmd's 8191 characters
    rsp.write_text(chr(10).join(chr(34) + o + chr(34) for o in objs) + chr(10))
    lines.append(f'link /nologo @"{rsp}" /OUT:"{args.out}" || exit /b 1')
    bat = obj / "build.bat"
    bat.write_text(chr(10).join(lines) + chr(10), newline=None)
    r = subprocess.run([str(bat)], cwd=ROOT)
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
