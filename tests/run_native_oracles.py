#!/usr/bin/env python3
"""Run the existing behavioural oracles (../tests/check.py) against the
transliterated game.

tests/translit_scenarios.c drives the translated PlayState through the same
scenarios as ../tests/core_test.c and writes the same CSV traces; the
compare_* functions of check.py then recompute the expected values from the
pinned ActionScript rules, unchanged.

check.py writes its evidence under ROOT/logs/parity and reads the pinned source
under ROOT/work/kingdom-source. To keep the prototype's own evidence intact,
ROOT is pointed at translit/parity_root, which gets a copy of the pinned
ActionScript/level files (no media) on first run.

  python tests/run_native_oracles.py [--cc gcc]
"""
import argparse, importlib.util, os, shutil, subprocess, sys
from pathlib import Path

HERE = Path(os.path.abspath(__file__)).parents[1]          # translit/
PORT = HERE.parent                                   # ports/kingdom/
PIN = "56d4e622461b72df8fa8f69d09fbd63dab7a1346"
SOURCE = PORT / "work/kingdom-source" / PIN / f"kingdom-{PIN}"
SHADOW = HERE / "parity_root"
WORK = SHADOW / "work"  # check.py records evidence paths relative to ROOT

# oracle name in check.py -> CSV(s) the scenario driver writes
ORACLES = [
    ("compare_player_motion_to_pinned_flixel", "V006 player motion (Player.as + FlxU.computeVelocity)"),
    ("compare_player_food_to_pinned_source", "V007 food / rest on grass (Player.as + FlxTilemap.getTile)"),
    ("compare_v011_troll_damage_matrix", "V011 Troll x Citizen damage matrix + Farmer hack cooldown"),
    ("compare_v011_farmer_then_poor_same_troll", "V011 Farmer then Poor on one Troll"),
    ("compare_v011_farmer_poor_player_order_to_pinned_source", "V011 Farmer, Poor and Player on one Troll"),
    ("compare_v014_tax_aabb_and_recipient_order_to_pinned_source", "V014 tax AABB edges + recipient order"),
]


def shadow_root():
    dst = SHADOW / "work/kingdom-source" / PIN / f"kingdom-{PIN}"
    if not dst.exists():
        for f in SOURCE.rglob("*"):
            if f.is_file() and f.suffix in (".as", ".oel", ".json", ".txt"):
                t = dst / f.relative_to(SOURCE)
                t.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, t)
    (SHADOW / "logs/parity").mkdir(parents=True, exist_ok=True)
    # some oracles also hash/inspect the native prototype's files for their
    # evidence records; give them the prototype's copies (read only)
    for d in ("source", "tests"):
        if not (SHADOW / d).exists() and (PORT / d).exists():
            shutil.copytree(PORT / d, SHADOW / d, ignore=shutil.ignore_patterns("__pycache__", "*.exe", "*.obj"))


VARIANTS = (("flixel", []), ("one_callback", ["--one-callback-per-pair"]))


def build(cc, variants=VARIANTS):
    WORK.mkdir(parents=True, exist_ok=True)
    exe = WORK / ("translit_scenarios.exe" if sys.platform == "win32" else "translit_scenarios")
    if cc == "msvc":  # Windows: the installed MSVC Build Tools, no gcc needed
        subprocess.run([sys.executable, str(HERE / "tools/build_host.py"), "--scenarios", "--out", str(exe)], check=True,
                       stdout=subprocess.DEVNULL)
    else:
        srcs = [str(p) for d in ("flash", "flixel", "game") for p in sorted((HERE / d).glob("*.c"))]
        srcs += [str(HERE / "platform/host/media.c"), str(HERE / "platform/common/session.c"), str(HERE / "platform/common/stereo.c"),
                 str(HERE / "platform/common/parallel.c"), str(HERE / "vendor/engine/core/jobs_state.c"), str(HERE / "tests/translit_scenarios.c")]
        subprocess.run([cc, "-std=gnu11", "-D_DEFAULT_SOURCE", "-DLUME_HOST_JOBS", f"-I{HERE / 'vendor'}", f"-I{HERE}", "-O2", "-w", *srcs,
                        "-lm", "-lpthread", "-o", str(exe)], check=True)
    for variant, extra in variants:
        (WORK / variant).mkdir(exist_ok=True)
        subprocess.run([str(exe), str(HERE / "translit_assets"), *extra], cwd=WORK / variant, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def direct_only(args):
    """Standalone repository: the CSV oracles need the old prototype's ../tests/check.py, which is not here.
    Run the direct source-derived checks of tests/translit_scenarios.c only."""
    if not args.no_build:
        build(args.cc, variants=(VARIANTS[0],))
    checks = WORK / "flixel/translit_checks.txt"
    if not checks.exists():
        print("no translit_checks.txt: the scenario run failed")
        return 1
    text = checks.read_text()
    print("direct source-derived checks (CSV oracles skipped: tests/check.py not available):")
    print(text)
    failed = [l for l in text.splitlines() if l.startswith("FAIL")]
    print("%d PASS, %d FAIL" % (sum(1 for l in text.splitlines() if l.startswith("PASS")), len(failed)))
    return 1 if failed else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cc", default="msvc" if sys.platform == "win32" else "gcc",
                    help="gcc/cc/clang, or msvc (tools/build_host.py; Windows default)")
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()
    if not (PORT / "tests/check.py").exists():
        return direct_only(args)
    shadow_root()
    if not args.no_build:
        build(args.cc)
    sys.path.insert(0, str(PORT / "tests"))
    spec = importlib.util.spec_from_file_location("check", PORT / "tests/check.py")
    check = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(check)
    check.ROOT = SHADOW
    rows = []
    for name, label in ORACLES:
        result = {}
        for variant in ("flixel", "one_callback"):
            try:
                getattr(check, name)(WORK / variant)
                result[variant] = ("PASS", "")
            except Exception as e:  # AssertionError or a malformed trace
                result[variant] = ("FAIL", f"{type(e).__name__}: {str(e)[:300]}")
        rows.append((label, result))
    print()
    print("oracle".ljust(64), "Flixel-exact", "one-callback")
    for label, r in rows:
        print(label.ljust(64), r["flixel"][0].ljust(12), r["one_callback"][0])
    for label, r in rows:
        if r["flixel"][0] == "FAIL":
            print(f"\n[{label}] Flixel-exact run: {r['flixel'][1]}")
    checks = WORK / "flixel/translit_checks.txt"
    if checks.exists():  # direct source-derived checks in translit_scenarios.c (no CSV oracle in check.py)
        print("\ndirect source-derived checks (Flixel-exact run):")
        print(checks.read_text())
    print("\nFlixel-exact = the game as translated (FlxQuadTree reports a pair once per shared leaf, like Flixel 2.55).")
    print("one-callback = test switch FlxQuadTree_testOneCallbackPerPair, matching oracles that assume one callback per pair.")
    passed = sum(r["flixel"][0] == "PASS" for _, r in rows)
    print(f"{passed}/{len(rows)} oracles pass on the Flixel-exact run; evidence in {SHADOW / 'logs/parity'}")
    direct_fail = [l for l in checks.read_text().splitlines() if l.startswith("FAIL")] if checks.exists() else []
    if direct_fail:  # a failed direct check must fail the command (Codex test-gate finding)
        print(f"{len(direct_fail)} direct check(s) FAILED")
    return 0 if not direct_fail and all(r["flixel"][0] == "PASS" or r["one_callback"][0] == "PASS" for _, r in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
