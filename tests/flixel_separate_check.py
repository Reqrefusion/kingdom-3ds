#!/usr/bin/env python3
"""Runs the translated FlxObject separation against the existing project's
independent Python transcription (../tests/flixel_parity.py: axis())."""
import csv, importlib.util, subprocess, sys, types
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.modules["check"] = types.SimpleNamespace(compiler=lambda: None)  # flixel_parity imports it
spec = importlib.util.spec_from_file_location("flixel_parity", HERE.parent / "tests" / "flixel_parity.py")
ref = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)

exe = HERE / "tests" / "flixel_separate_trace"
srcs = [str(p) for p in (HERE / "flixel").glob("*.c")] + [str(p) for p in (HERE / "flash").glob("*.c")] + [str(HERE / "tests/flixel_separate_trace.c"), str(HERE / "platform/host/media.c")]
subprocess.run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O1", "-w", "-o", str(exe)] + srcs + ["-lm"], check=True)
out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.splitlines()
rows = list(csv.reader(out))[1:]
fails = 0
for row in rows:
    mode = int(row[1])
    a = dict(zip(ref.FIELDS, map(float, row[2:16]))); b = dict(zip(ref.FIELDS, map(float, row[16:30])))
    a["touching"] = b["touching"] = 0
    hit = (ref.axis(a, b, False) or False) | (ref.axis(a, b, True) or False) if mode == 2 else ref.axis(a, b, mode == 1)
    if mode == 2:  # separate() evaluates both axes
        pass
    expected = [int(bool(hit)), a["x"], a["y"], a["vx"], a["vy"], a["touching"], b["x"], b["y"], b["vx"], b["vy"], b["touching"]]
    actual = list(map(float, row[30:]))
    if any(abs(x - y) > 1e-9 for x, y in zip(expected, actual)):
        fails += 1
        if fails <= 3:
            print("mismatch", row[0], expected, actual)
print(f"{len(rows) - fails}/{len(rows)} cases identical to the reference")
sys.exit(1 if fails else 0)
