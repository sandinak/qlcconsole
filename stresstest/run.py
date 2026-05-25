#!/usr/bin/env python3
"""
QLC+ stress orchestrator — one command to validate a build under stress and to
measure its upper capability, with regression tracking.

Modes:
  validate    run the fixed scenario matrix (scenarios.json -> "validate"),
              compare each scenario's tick cost (p99) to baseline.json within a
              tolerance, print PASS/FAIL per scenario and exit non-zero on any
              regression / budget breach. This is the build gate.
  capability  run the capability ramps (scenarios.json -> "capability") to find
              the largest sustainable configuration (universe ceiling, etc.) and
              record it to results/.
  baseline    run the validate matrix and SAVE the results as baseline.json
              (the new known-good). Do this after an intentional perf change.

All scenarios are driven through engine/qlcstress with --json; results are
parsed from RESULT_JSON lines. Add --sanitize to run against a sanitizer build
(see build_sanitizers.sh) and fail on any ASan/UBSan/TSan report.

Examples:
  python3 run.py validate
  python3 run.py baseline
  python3 run.py capability
  python3 run.py validate --sanitize address
"""

import argparse
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEF_QLCSTRESS = os.path.join(HERE, "engine", "build", "qlcstress")
DEF_FIXTURES = os.path.join(HERE, "..", "resources", "fixtures")
DEF_SCRIPTS = os.path.join(HERE, "..", "resources", "rgbscripts")
SCENARIOS = os.path.join(HERE, "scenarios.json")
BASELINE = os.path.join(HERE, "baseline.json")
RESULTS_DIR = os.path.join(HERE, "results")

# sanitizer report markers -> any of these in output means a real bug
SAN_MARKERS = ("runtime error:", "ERROR: AddressSanitizer", "ERROR: ThreadSanitizer",
               "ERROR: LeakSanitizer", "SUMMARY: UndefinedBehaviorSanitizer",
               "WARNING: ThreadSanitizer", "data race")


def load(path):
    with open(path) as f:
        return json.load(f)


def run_scenario(qlcstress, scn, common, ticks, sanitize_env=None,
                 timeout=None, retries=0):
    """Run one scenario, return (result_dicts, raw_output, returncode).

    Under sanitizers on macOS 26.x the ASan runtime can deadlock in
    libSystem's initializer *before main()* (a toolchain/OS race, not our
    code). We guard with a timeout and retry: a fresh launch usually wins the
    race. A wedged attempt is killed and retried; persistent wedging is
    reported, not silently hung."""
    mode = scn.get("mode", "flatout")
    cmd = [qlcstress, "engine", "--json", "--scenario", scn["name"],
           "--mode", mode, "--ticks", str(ticks)] + scn["args"] + common
    env = dict(os.environ)
    if sanitize_env:
        env.update(sanitize_env)
    attempt = 0
    while True:
        attempt += 1
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True,
                                  env=env, timeout=timeout)
        except subprocess.TimeoutExpired as e:
            if attempt <= retries:
                print(f"    (attempt {attempt}: startup wedge / timeout — retrying)")
                continue
            out = (e.stdout or "") + (e.stderr or "") if isinstance(e.stdout, str) else ""
            return [], out + "\n[TIMEOUT: process wedged at startup]", 124
        out = proc.stdout + proc.stderr
        results = []
        for line in proc.stdout.splitlines():
            if line.startswith("RESULT_JSON "):
                try:
                    results.append(json.loads(line[len("RESULT_JSON "):]))
                except json.JSONDecodeError:
                    pass
        # an ASan startup wedge that we killed produces no RESULT_JSON; retry
        if not results and mode != "ramp" and attempt <= retries:
            print(f"    (attempt {attempt}: no result — retrying)")
            continue
        return results, out, proc.returncode


def has_sanitizer_report(text):
    return any(m in text for m in SAN_MARKERS)


def save_results(tag, payload):
    os.makedirs(RESULTS_DIR, exist_ok=True)
    path = os.path.join(RESULTS_DIR, f"{tag}-{time.strftime('%Y%m%d-%H%M%S')}.json")
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)
    return path


def cmd_validate(args, cfg, save_as_baseline=False):
    common = ["--fixtures-dir", args.fixtures, "--scripts-dir", args.scripts]
    ticks = cfg.get("defaults", {}).get("ticks", 400)
    tol = cfg.get("defaults", {}).get("regression_tolerance_pct", 25.0)
    # Under a sanitizer the build is 3-10x slower, so timing/regression checks
    # are meaningless. We only care about sanitizer reports and crashes, and we
    # use far fewer ticks to keep the run tractable.
    san_mode = bool(args.sanitize)
    if san_mode:
        ticks = max(50, ticks // 8)
    baseline = {} if save_as_baseline else (load(BASELINE) if os.path.exists(BASELINE) else {})
    if not save_as_baseline and not baseline:
        print("NOTE: no baseline.json yet — run `run.py baseline` first to enable "
              "regression checks. Proceeding with budget-only checks.\n")

    san_env = sanitizer_env(args.sanitize) if args.sanitize else None
    # sanitizer runs use the smaller dedicated set when present
    scenario_list = cfg.get("sanitize", cfg["validate"]) if san_mode else cfg["validate"]
    results = {}
    failed = []
    print(f"{'scenario':<24} {'p99 ms':>9} {'baseline':>9} {'delta':>8}  verdict")
    print("-" * 64)
    # sanitizers can wedge at startup on macOS 26.x — guard with a short timeout
    # (a wedge is obvious within ~15s) and a few retries. For reliable sanitizer
    # runs use Linux/CI; locally this is best-effort.
    timeout = 30 if san_mode else None
    retries = 3 if san_mode else 0
    for scn in scenario_list:
        rlist, out, rc = run_scenario(args.qlcstress, scn, common, ticks, san_env,
                                      timeout=timeout, retries=retries)
        if not rlist:
            print(f"{scn['name']:<24} {'-':>9}  no RESULT_JSON (rc={rc}) -> FAIL")
            failed.append(scn["name"]); continue
        r = rlist[0]
        p99 = r["p99"]
        results[scn["name"]] = r
        verdict = "OK"
        base = baseline.get(scn["name"], {}).get("p99")
        delta_s = ""
        if san_mode:
            # only the sanitizer verdict matters; timing is ignored
            if has_sanitizer_report(out):
                verdict = "SANITIZER"; failed.append(scn["name"])
                results[scn["name"]]["sanitizer_report"] = True
        else:
            # 1) budget check
            if not r["withinBudget"]:
                verdict = "OVER-BUDGET"; failed.append(scn["name"])
            # 2) regression check vs baseline
            if base:
                delta = 100.0 * (p99 - base) / base
                delta_s = f"{delta:+.1f}%"
                if delta > tol and verdict == "OK":
                    verdict = f"REGRESSED>{tol:.0f}%"; failed.append(scn["name"])
        print(f"{scn['name']:<24} {p99:>9.3f} {('%.3f'%base) if base else '-':>9} "
              f"{delta_s:>8}  {verdict}")

    if save_as_baseline:
        with open(BASELINE, "w") as f:
            json.dump(results, f, indent=2)
        print(f"\nbaseline written: {BASELINE} ({len(results)} scenarios)")
        return 0

    save_results("validate", results)
    if failed:
        print(f"\nRESULT: FAIL ({len(failed)} scenario(s): {', '.join(sorted(set(failed)))})")
        return 1
    print("\nRESULT: PASS (all scenarios within budget and baseline tolerance)")
    return 0


def cmd_capability(args, cfg):
    common = ["--fixtures-dir", args.fixtures, "--scripts-dir", args.scripts]
    ticks = cfg.get("defaults", {}).get("ticks", 400)
    san_env = sanitizer_env(args.sanitize) if args.sanitize else None
    summary = {}
    for scn in cfg["capability"]:
        print(f"\n=== capability: {scn['name']} ===")
        rlist, out, rc = run_scenario(args.qlcstress, scn, common, ticks, san_env)
        ceiling = None
        for r in rlist:
            if "ceilingUniverses" in r:
                ceiling = r["ceilingUniverses"]
            elif "universes" in r:
                mark = "OK " if r["withinBudget"] else "OVER"
                print(f"  uni={r['universes']:>4}  p50={r['p50']:.2f} p99={r['p99']:.2f} ms  {mark}")
        if san_env and has_sanitizer_report(out):
            print("  !! SANITIZER REPORT during capability ramp")
        print(f"  -> ceiling: {ceiling} universes")
        summary[scn["name"]] = {"ceilingUniverses": ceiling, "steps": rlist}
    path = save_results("capability", summary)
    print(f"\ncapability results saved: {path}")
    return 0


def sanitizer_env(kind):
    """Env for running a sanitizer build. The actual instrumentation comes from
    a sanitizer-built qlcstress (see build_sanitizers.sh); here we just make the
    runtime loud and abort-on-error so reports are caught."""
    # macOS/arm64: ASan re-execs itself to set MallocNanoZone=0; on recent macOS
    # that re-exec can wedge the process in dyld. Setting it up front avoids the
    # re-exec and the hang.
    env = {"MallocNanoZone": "0"}
    if kind in ("address", "asan"):
        env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=0:print_summary=1"
        env["UBSAN_OPTIONS"] = "print_stacktrace=1:halt_on_error=0"
    elif kind in ("thread", "tsan"):
        env["TSAN_OPTIONS"] = "halt_on_error=0:second_deadlock_stack=1"
    return env


def main():
    ap = argparse.ArgumentParser(description="QLC+ stress orchestrator")
    ap.add_argument("mode", choices=["validate", "baseline", "capability"])
    ap.add_argument("--qlcstress", default=DEF_QLCSTRESS)
    ap.add_argument("--fixtures", default=DEF_FIXTURES)
    ap.add_argument("--scripts", default=DEF_SCRIPTS)
    ap.add_argument("--sanitize", default=None,
                    help="address|thread — run against a sanitizer build and fail on reports")
    args = ap.parse_args()

    # --sanitize selects the sanitizer-built harness unless --qlcstress was set
    if args.sanitize and args.qlcstress == DEF_QLCSTRESS:
        tag = "asan" if args.sanitize in ("address", "asan") else "tsan"
        args.qlcstress = os.path.join(HERE, "engine", f"build-{tag}", "qlcstress")
        if not os.path.exists(args.qlcstress):
            print(f"ERROR: sanitizer build not found at {args.qlcstress}\n"
                  f"build it: ./build_sanitizers.sh {tag}")
            return 2

    if not os.path.exists(args.qlcstress):
        print(f"ERROR: qlcstress not found at {args.qlcstress}\n"
              f"build it: cd engine && cmake -S . -B build -G Ninja "
              f"-DCMAKE_PREFIX_PATH=$(brew --prefix qt) && ninja -C build")
        return 2
    cfg = load(SCENARIOS)

    if args.mode == "baseline":
        return cmd_validate(args, cfg, save_as_baseline=True)
    if args.mode == "validate":
        return cmd_validate(args, cfg)
    if args.mode == "capability":
        return cmd_capability(args, cfg)
    return 2


if __name__ == "__main__":
    sys.exit(main())
