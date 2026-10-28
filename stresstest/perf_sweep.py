#!/usr/bin/env python3
"""
Performance sweeps with qlcstress --mode realtime: the real MasterTimer at
50 Hz, the functions really running, every statistic after warm-up.

For each configuration: per-tick compute time (p50/p95/p99/max), ticks over
the 20 ms budget (and per hour), cadence (gap p99, late ticks), process CPU%,
RSS. A dimension stops one step after its breaking point (compute p99 > the
tick period, or the cadence lost: gap p99 > 1.5 x period), or at --max-sec of
wall time for one configuration (show build + run).

  python3 perf_sweep.py --qlcstress Q --resources R --out DIR [--sweeps a1,a2,b,c,real]
                        [--seconds 40] [--warmup 10] [--only-first N]

Writes DIR/<sweep>/<name>.{json,timeline.csv,log} and DIR/results.csv (one row
per configuration, appended as it goes; a rerun skips configurations whose
JSON is already there).
"""
import argparse
import csv
import json
import math
import os
import subprocess
import sys
import time

FIELDS = ["sweep", "name", "host", "universes", "fixturesPerUni", "fixtures", "channels",
          "scenes", "chasers", "matrices", "efx", "cuelists", "sceneFixtures", "functionsStarted",
          "runningMean", "runningMax", "ticks", "compute_p50", "compute_p95", "compute_p99",
          "compute_p999", "compute_max", "compute_mean", "tickcpu_p99", "overBudget", "overPerHour",
          "gap_p50", "gap_p99", "gap_max", "late", "latePerHour", "cpuPct", "cpuPctP95",
          "rssStart", "rssPeak", "rssEnd", "uniBacklogMax", "uniLaggingSampleFrac", "withinBudget",
          "broken", "brokenBy", "rc", "wall_s"]


def mixed(u, fpu=170):
    """The 'mixed' load for u universes: a look per universe, and per 4
    universes a chaser, an RGB matrix (script algorithm) and an EFX; a Show
    Tree-style cue list per 8."""
    q = max(1, math.ceil(u / 4))
    return dict(universes=u, fpu=fpu, scenes=u, chasers=q, matrices=q, efx=q,
                cuelists=max(1, math.ceil(u / 8)), collections=0, scene_fix=min(170, fpu))


def plan(sweeps):
    out = []
    # Realistic chasers (per-step fade/hold): the same sweeps again, suffixed "p"
    if "a1p" in sweeps:
        for u in [1, 8, 16, 32, 64, 80, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048]:
            out.append(("a1p-universes-mixed-perstep", "u%04d" % u, dict(mixed(u), perstep=True), None))
    if "bp" in sweeps:
        for f in [16, 32, 64, 128, 170]:
            out.append(("bp-fixtures-per-universe-perstep", "f%03d" % f, dict(mixed(32, f), perstep=True), None))
    if "cp" in sweeps:
        base = dict(universes=32, fpu=170, scenes=64, chasers=0, matrices=0, efx=0, cuelists=0,
                    collections=0, scene_fix=170, perstep=True)
        for n in [8, 16, 32, 64, 128, 256, 512, 1024, 2048]:
            out.append(("cp-chasers-perstep", "n%04d" % n, dict(base, chasers=n, scenes=max(64, 4 * n)), "chaser"))
    if "a1" in sweeps:      # universes, mixed load scaled with them
        for u in [1, 8, 16, 32, 64, 80, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048]:
            out.append(("a1-universes-mixed", "u%04d" % u, mixed(u), None))
    if "a2" in sweeps:      # universes, one static full-universe look each
        for u in [1, 8, 16, 32, 64, 80, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048]:
            out.append(("a2-universes-static", "u%04d" % u,
                        dict(universes=u, fpu=170, scenes=u, chasers=0, matrices=0, efx=0,
                             cuelists=0, collections=0, scene_fix=170), "scene"))
    if "b" in sweeps:       # fixtures per universe, 32 universes, mixed load
        for f in [16, 32, 64, 128, 170]:
            out.append(("b-fixtures-per-universe", "f%03d" % f, mixed(32, f), None))
    if "c" in sweeps:       # one function type at a time on 32 x 170 RGB
        base = dict(universes=32, fpu=170, scenes=0, chasers=0, matrices=0, efx=0, cuelists=0,
                    collections=0, scene_fix=170)
        for n in [16, 32, 64, 128, 256, 512, 1024, 2048]:
            out.append(("c-scenes", "n%04d" % n, dict(base, scenes=n), "scene"))
        for n in [8, 16, 32, 64, 128, 256, 512, 1024]:
            out.append(("c-chasers", "n%04d" % n, dict(base, scenes=64, chasers=n), "chaser"))
        for n in [8, 16, 32, 64, 128, 256, 512, 1024]:
            out.append(("c-rgbmatrix-script", "n%04d" % n, dict(base, matrices=n), "matrix"))
        for n in [16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
            out.append(("c-efx", "n%04d" % n, dict(base, efx=n), "efx"))
        for n in [2, 4, 8, 16, 32, 64, 128, 256]:
            out.append(("c-cuelists-fades", "n%04d" % n, dict(base, cuelists=n), "cuelist"))
    return out


def run_one(a, sweep, name, cfg, types, outdir):
    os.makedirs(os.path.join(outdir, sweep), exist_ok=True)
    prefix = os.path.join(outdir, sweep, name)
    if os.path.exists(prefix + ".json"):
        return json.load(open(prefix + ".json")), 0, 0.0, True
    cmd = [a.qlcstress, "engine", "--mode", "realtime", "--json",
           "--seconds", str(a.seconds), "--warmup-seconds", str(a.warmup),
           "--universes", str(cfg["universes"]), "--fixtures-per-uni", str(cfg["fpu"]),
           "--scenes", str(cfg["scenes"]), "--chasers", str(cfg["chasers"]),
           "--matrices", str(cfg["matrices"]), "--efx", str(cfg["efx"]),
           "--collections", str(cfg["collections"]), "--cuelists", str(cfg["cuelists"]),
           "--scene-fixtures", str(cfg["scene_fix"]),
           "--fixtures-dir", os.path.join(a.resources, "fixtures"),
           "--scripts-dir", os.path.join(a.resources, "rgbscripts"),
           "--scenario", "%s/%s" % (sweep, name), "--out", prefix]
    if types:
        cmd += ["--start-types", types]
    if cfg.get("perstep"):
        cmd += ["--chaser-perstep"]
    if a.tick_csv:
        cmd += ["--tick-csv"]
    t0 = time.time()
    with open(prefix + ".log", "w") as log:
        try:
            rc = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                                timeout=a.max_sec).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    wall = time.time() - t0
    res = json.load(open(prefix + ".json")) if os.path.exists(prefix + ".json") else None
    return res, rc, wall, False


def row_of(sweep, name, r, rc, wall):
    if r is None:
        return dict(sweep=sweep, name=name, rc=rc, wall_s=round(wall, 1), broken=True)
    c, g, tc = r["compute"], r["gap"], r["tickCpu"]
    period = r.get("periodMs", 20.0)
    lag = r.get("uniLaggingSampleFrac", 0.0)
    why = [w for w, bad in (("compute", c["p99"] > period), ("cadence", g["p99"] > 1.5 * period),
                            ("universes", lag >= 0.05)) if bad]
    broken = bool(why)
    return dict(sweep=sweep, name=name, host=r["host"], universes=r["universes"],
                fixturesPerUni=r["fixturesPerUni"], fixtures=r["fixtures"], channels=r["channels"],
                scenes=r["scenes"], chasers=r["chasers"], matrices=r["matrices"], efx=r["efx"],
                cuelists=r.get("cuelists", 0), sceneFixtures=r.get("sceneFixtures", 0),
                functionsStarted=r["functionsStarted"], runningMean=round(r["runningMean"], 1),
                runningMax=r["runningMax"], ticks=int(r["ticks"]),
                compute_p50=c["p50"], compute_p95=c["p95"], compute_p99=c["p99"],
                compute_p999=c["p999"], compute_max=round(c["max"], 3), compute_mean=round(c["mean"], 3),
                tickcpu_p99=tc["p99"], overBudget=int(r["overBudget"]),
                overPerHour=round(r["overPerHour"], 1), gap_p50=g["p50"], gap_p99=g["p99"],
                gap_max=round(g["max"], 3), late=int(r["late"]), latePerHour=round(r["latePerHour"], 1),
                cpuPct=round(r["cpuPct"], 1), cpuPctP95=round(r["cpuPctP95"], 1),
                rssStart=round(r["rssStart"], 1), rssPeak=round(r["rssPeak"], 1),
                rssEnd=round(r["rssEnd"], 1), uniBacklogMax=r.get("uniBacklogMax"),
                uniLaggingSampleFrac=round(lag, 3), withinBudget=r["withinBudget"], broken=broken,
                brokenBy="+".join(why),
                rc=rc, wall_s=round(wall, 1))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qlcstress", required=True)
    ap.add_argument("--resources", required=True, help="the source tree's resources/")
    ap.add_argument("--out", required=True)
    ap.add_argument("--sweeps", default="a1,a2,b,c")
    ap.add_argument("--seconds", type=int, default=40)
    ap.add_argument("--warmup", type=int, default=10)
    ap.add_argument("--max-sec", type=int, default=900, help="wall limit for one configuration")
    ap.add_argument("--past-break", type=int, default=1,
                    help="steps run after the first step whose compute p99 > the period (the tick budget)")
    ap.add_argument("--past-any-break", type=int, default=3,
                    help="steps run after the first step broken any way (cadence, universe threads)")
    ap.add_argument("--max-rss-mb", type=float, default=0, help="stop a dimension past this RSS (0: off)")
    ap.add_argument("--tick-csv", action="store_true")
    ap.add_argument("--only-first", type=int, default=0,
                    help="run only the first N configurations of each sweep (a smoke test; 0: all)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    res_path = os.path.join(a.out, "results.csv")
    new = not os.path.exists(res_path)
    f = open(res_path, "a", newline="", buffering=1)
    w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
    if new:
        w.writeheader()
    sweeps = set(a.sweeps.split(","))
    broken_count, compute_broken, done = {}, {}, {}
    for sweep, name, cfg, types in plan(sweeps):
        if a.only_first and done.get(sweep, 0) >= a.only_first:
            continue
        done[sweep] = done.get(sweep, 0) + 1
        if compute_broken.get(sweep, 0) > a.past_break or broken_count.get(sweep, 0) > a.past_any_break:
            continue
        r, rc, wall, cached = run_one(a, sweep, name, cfg, types, a.out)
        row = row_of(sweep, name, r, rc, wall)
        if not cached:
            w.writerow(row)
        print("%s %-26s %-6s %s" % (time.strftime("%H:%M:%S"), sweep, name,
              "FAILED rc=%s" % rc if r is None else
              "p99 %.2f max %.2f ms over %d gap99 %.2f cpu %.0f%% rss %.0f MB run %.0f lag %.2f%s" % (
                  row["compute_p99"], row["compute_max"], row["overBudget"], row["gap_p99"],
                  row["cpuPct"], row["rssPeak"], row["runningMean"], row["uniLaggingSampleFrac"],
                  "  BROKEN(%s)" % row["brokenBy"] if row["broken"] else "")), flush=True)
        if row["broken"]:
            broken_count[sweep] = broken_count.get(sweep, 0) + 1
        if r is None or "compute" in row.get("brokenBy", ""):
            compute_broken[sweep] = compute_broken.get(sweep, 0) + 1
        if a.max_rss_mb and r is not None and r["rssPeak"] > a.max_rss_mb:
            compute_broken[sweep] = a.past_break + 1
    print("sweeps done", flush=True)


if __name__ == "__main__":
    main()
