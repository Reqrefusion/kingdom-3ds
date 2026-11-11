#!/usr/bin/env python3
"""ONE entry point for the whole Flash -> 3DS port workflow. Read PORTING_GUIDE.md first.

  python tools/port.py doctor                  is this machine ready? (tools, paths)
  python tools/port.py check [--quick]         run every verification step, PASS/FAIL table, exit code 1 on FAIL
  python tools/port.py check --only boundary,oracles
  python tools/port.py swf-diff                SWF bytecode vs the AS3 source (new differences only fail)
  python tools/port.py abc <Class> <method>    show the SHIPPED bytecode of one method (or: abc <Class> --consts)
  python tools/port.py build-3ds               build the .3dsx
  python tools/port.py deploy                  copy the .3dsx + data to the Azahar SD card
  python tools/port.py azahar "seed 0.5; freeze 600; progress D2 A7 X1000 ..." --out shot.png [--stereo]
  python tools/port.py ruffle "wait 6; click; wait 30; shot a" --out ruffle_out

Everything is plain subprocess calls to the scripts in tools/ and tests/, so you can also run those directly.
A step PASSes only when its command exits 0 and (where stated) its output has the expected summary.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PY = sys.executable
ENV = dict(os.environ, PYTHONIOENCODING="utf-8")


def load_config():
    cfg = json.loads((ROOT / "port.json").read_text(encoding="utf-8"))
    for k in ("source_dir", "swf", "assets", "host_exe"):
        cfg[k + "_abs"] = str((ROOT / os.path.expandvars(cfg[k])).resolve()) if cfg.get(k) else ""
    for k in ("devkitpro", "azahar_exe", "azahar_sdmc", "ruffle_exe", "msvc_vcvars"):
        cfg[k] = os.path.expandvars(cfg[k])
    return cfg


def run(cmd, timeout=3600, cwd=ROOT):
    p = subprocess.run([str(c) for c in cmd], cwd=cwd, env=ENV, capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=timeout)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def tail(text, n=6):
    lines = [l for l in text.splitlines() if l.strip()]
    return "\n".join("      " + l[:200] for l in lines[-n:])


class Result:
    def __init__(self):
        self.rows = []

    def add(self, name, status, detail=""):
        self.rows.append((name, status, detail))
        print("[%-4s] %-14s %s" % (status, name, detail.splitlines()[0] if detail else ""))
        if status in ("FAIL",) and detail:
            print("\n".join("        " + l for l in detail.splitlines()[1:8]))

    def code(self):
        return 1 if any(s == "FAIL" for _, s, _ in self.rows) else 0

    def table(self):
        print("\n=== summary ===")
        for name, status, detail in self.rows:
            print("%-5s %s" % (status, name))
        print("RESULT:", "FAILED" if self.code() else "OK")


def known_diffs():
    f = ROOT / "tools/known_diffs.json"
    return json.loads(f.read_text(encoding="utf-8")) if f.exists() else {"swf_vs_source": {}, "parity_ops": {}}


def keys_of(output):
    """'Class.method: ...' lines -> {'Class.method'}"""
    return {l.split(":")[0].strip() for l in output.splitlines() if re.match(r"^[A-Za-z_]\w*\.\w+:", l)}


# ---------------------------------------------------------------------------------------------------------------
def step_boundary(cfg, res):
    rc, out = run([PY, "tools/check_parity_boundary.py"])
    res.add("boundary", "PASS" if rc == 0 else "FAIL", out.strip().splitlines()[0] if out.strip() else "")


def step_build(cfg, res):
    rc, out = run([PY, "tools/build_host.py"])
    ok = rc == 0 and Path(cfg["host_exe_abs"]).exists()
    res.add("host-build", "PASS" if ok else "FAIL", "built " + cfg["host_exe"] if ok else "build_host.py failed\n" + tail(out, 8))


def step_regress(cfg, res):
    rc, out = run([PY, "tests/regress.py", "--exe", cfg["host_exe_abs"]])
    ok = rc == 0 and out.strip().splitlines()[-1].strip() == "OK"
    res.add("regress", "PASS" if ok else "FAIL", "CPU output identical to baseline, GPU path within limit" if ok else tail(out, 8))


def step_oracles(cfg, res):
    rc, out = run([PY, "tests/run_native_oracles.py"])
    npass = len(re.findall(r"^PASS:", out, re.M))
    nfail = len(re.findall(r"^FAIL:", out, re.M))
    ok = rc == 0 and nfail == 0 and npass > 0
    detail = "%d direct checks PASS, %d FAIL" % (npass, nfail)
    if not ok:
        detail += "\n" + "\n".join(l for l in out.splitlines() if l.startswith("FAIL:"))[:900] + "\n" + tail(out, 4)
    res.add("oracles", "PASS" if ok else "FAIL", detail)


def step_static(cfg, res):
    if not Path(cfg["source_dir_abs"]).exists():
        return res.add("static", "SKIP", "source_dir missing: " + cfg["source_dir"])
    rc, out = run([PY, "tools/side_by_side.py", "--source", cfg["source_dir_abs"], "--out", "STATIC_COMPARISON.md"])
    m = re.search(r"(\d+)/(\d+) statements identical \(([\d.]+)%\), (\d+) functions, (\d+) without C counterpart", out)
    if rc != 0 or not m:
        return res.add("static", "FAIL", "side_by_side.py failed\n" + tail(out, 6))
    pct = float(m.group(3))
    status = "PASS" if pct >= cfg["min_static_match_percent"] else "FAIL"
    res.add("static", status, "%s/%s statements identical (%.1f%%), %s functions, %s source functions without C" %
            (m.group(1), m.group(2), pct, m.group(4), m.group(5)))


def step_ops(cfg, res):
    if not Path(cfg["source_dir_abs"]).exists():
        return res.add("parity_ops", "SKIP", "source_dir missing")
    rc, out = run([PY, "tools/parity_ops.py", cfg["source_dir_abs"]])
    if rc != 0:
        return res.add("parity_ops", "FAIL", tail(out, 6))
    new = sorted(keys_of(out) - set(known_diffs()["parity_ops"]))
    if new:
        lines = [l for l in out.splitlines() if l.split(":")[0].strip() in new]
        res.add("parity_ops", "FAIL", "%d NEW difference(s) between C and the AS3 (comparison operators / constants)\n%s\n"
                "      Read the function on both sides. Fix the C, or add the reason to tools/known_diffs.json." %
                (len(new), "\n".join("      " + l[:220] for l in lines)))
    else:
        res.add("parity_ops", "PASS", "no unreviewed differences (%d reviewed ones are listed in known_diffs.json)" %
                len(known_diffs()["parity_ops"]))


def step_swfdiff(cfg, res):
    if not cfg["swf_abs"] or not Path(cfg["swf_abs"]).exists() or not Path(cfg["source_dir_abs"]).exists():
        return res.add("swf-diff", "SKIP", "no SWF configured/found (optional: only needed to compare against the shipped game)")
    rc, out = run([PY, "tools/swf_vs_source.py", cfg["swf_abs"], cfg["source_dir_abs"]])
    if rc != 0:
        return res.add("swf-diff", "FAIL", tail(out, 6))
    new = sorted(keys_of(out) - set(known_diffs()["swf_vs_source"]))
    if new:
        lines = [l for l in out.splitlines() if l.split(":")[0].strip() in new]
        res.add("swf-diff", "FAIL", "%d NEW difference(s) between the SHIPPED SWF and the AS3 snapshot\n%s\n"
                "      Read it with: python tools/port.py abc <Class> <method>. The SWF is the truth." %
                (len(new), "\n".join("      " + l[:220] for l in lines)))
    else:
        res.add("swf-diff", "PASS", "no unreviewed SWF/source differences")


STEPS = [("boundary", step_boundary), ("host-build", step_build), ("regress", step_regress), ("oracles", step_oracles),
         ("static", step_static), ("parity_ops", step_ops), ("swf-diff", step_swfdiff)]
QUICK = ("boundary", "parity_ops", "swf-diff", "oracles")


def cmd_check(cfg, args):
    names = [n for n, _ in STEPS]
    if args.only:
        wanted = args.only.split(",")
        bad = [w for w in wanted if w not in names]
        if bad:
            sys.exit("unknown step %s; steps are: %s" % (bad, ", ".join(names)))
    else:
        wanted = list(QUICK) if args.quick else names
    res = Result()
    for name, fn in STEPS:
        if name in wanted:
            try:
                fn(cfg, res)
            except subprocess.TimeoutExpired:
                res.add(name, "FAIL", "timed out")
    res.table()
    return res.code()


def cmd_doctor(cfg, args):
    res = Result()

    def chk(name, ok, hint, warn=False):
        res.add(name, "PASS" if ok else ("WARN" if warn else "FAIL"), "" if ok else hint)

    chk("python", sys.version_info >= (3, 9), "need Python 3.9+")
    for mod in ("PIL", "numpy"):
        try:
            __import__(mod)
            chk(mod, True, "")
        except ImportError:
            chk(mod, False, "pip install pillow numpy")
    chk("source_dir", Path(cfg["source_dir_abs"]).exists() and any(Path(cfg["source_dir_abs"]).glob("*.as")),
        "AS3 source folder not found: " + cfg["source_dir"])
    chk("swf", bool(cfg["swf_abs"]) and Path(cfg["swf_abs"]).exists(), "original SWF not found (needed by swf-diff/abc): " + cfg["swf"], warn=True)
    chk("assets", Path(cfg["assets_abs"]).exists(), "converted assets missing: run tools/convert_assets.py")
    vs = Path(cfg["msvc_vcvars"]).exists() or bool(shutil.which("cl")) or bool(shutil.which("gcc"))
    chk("host compiler", vs, "install MSVC Build Tools (or gcc); see README 'Windows host build'")
    chk("devkitpro", (Path(cfg["devkitpro"]) / "devkitARM/bin").exists(), "install devkitPro (3DS) at " + cfg["devkitpro"], warn=True)
    chk("azahar", Path(cfg["azahar_exe"]).exists(), "Azahar emulator not found (3DS tests): " + cfg["azahar_exe"], warn=True)
    chk("ruffle", Path(cfg["ruffle_exe"]).exists(), "Ruffle not found (original SWF screenshots): " + cfg["ruffle_exe"], warn=True)
    busy = subprocess.run(["powershell", "-NoProfile", "-Command", "@(Get-Process azahar -ErrorAction SilentlyContinue).Count"],
                          capture_output=True, text=True).stdout.strip() if os.name == "nt" else "0"
    chk("azahar free", busy in ("0", ""), "an Azahar is running (maybe another job); do not kill it, wait or ask", warn=True)
    res.table()
    return res.code()


def cmd_abc(cfg, args):
    rc, out = run([PY, "tools/abc_dump.py", cfg["swf_abs"], args.cls, args.method])
    print(out)
    return rc


def cmd_swfdiff(cfg, args):
    res = Result()
    step_swfdiff(cfg, res)
    return res.code()


def product(cfg):
    return cfg.get("product", "kingdom_translit")


def cmd_build3ds(cfg, args):
    extra = ["--product", product(cfg), "--title", cfg.get("title", "Kingdom (translit)")]
    if cfg.get("data_root"):
        extra += ["--data-root", cfg["data_root"]]
    rc, out = run([PY, "tools/build_3ds.py", "--sdk", cfg["devkitpro"]] + extra)
    print(tail(out, 8))
    return rc


def cmd_deploy(cfg, args):
    rc, out = run([PY, "tools/deploy_sd.py", "--sd-root", cfg["azahar_sdmc"], "--product", product(cfg)])
    print(tail(out, 4))
    return rc


def ps_file(script, params):
    return ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools" / script)] + params


def cmd_azahar(cfg, args):
    text = "\n".join(s.strip() for s in args.script.split(";") if s.strip())
    data_dir = Path((cfg.get("data_root") or "sdmc:/3ds/" + product(cfg)).split("/3ds/")[-1]).name
    params = ["-Script", text, "-Wait", str(args.wait), "-Out", args.out, "-Product", product(cfg), "-DataDir", data_dir]
    params += ["-Stereo"] if args.stereo else []
    if cfg.get("azahar_exe"):
        params += ["-Azahar", cfg["azahar_exe"]]
    rc, out = run(ps_file("azahar_autotest.ps1", params), timeout=600)
    print(out[-1500:])
    return rc


def cmd_ruffle(cfg, args):
    rc, out = run(ps_file("ruffle_shot.ps1", ["-Plan", args.plan, "-OutDir", args.out]), timeout=900)
    print(out[-800:])
    return rc


def main():
    cfg = load_config()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("doctor")
    c = sub.add_parser("check")
    c.add_argument("--quick", action="store_true", help="boundary + parity_ops + swf-diff + oracles")
    c.add_argument("--only", help="comma separated step names")
    sub.add_parser("swf-diff")
    a = sub.add_parser("abc")
    a.add_argument("cls")
    a.add_argument("method", help="a method name, or --consts")
    sub.add_parser("build-3ds")
    sub.add_parser("deploy")
    z = sub.add_parser("azahar")
    z.add_argument("script", help="autotest lines separated by ';' e.g. \"seed 0.5; freeze 600; progress D2 A7 X1000 B2 P0 F1 H2 W000011 C0 G4 S00\"")
    z.add_argument("--out", default="azahar_shot.png")
    z.add_argument("--wait", type=int, default=45)
    z.add_argument("--stereo", action="store_true")
    r = sub.add_parser("ruffle")
    r.add_argument("plan", help="e.g. \"wait 6; click; wait 30; shot a\"")
    r.add_argument("--out", default="ruffle_out")
    args = ap.parse_args()
    fn = {"doctor": cmd_doctor, "check": cmd_check, "swf-diff": cmd_swfdiff, "abc": cmd_abc, "build-3ds": cmd_build3ds,
          "deploy": cmd_deploy, "azahar": cmd_azahar, "ruffle": cmd_ruffle}[args.cmd]
    sys.exit(fn(cfg, args))


if __name__ == "__main__":
    main()
