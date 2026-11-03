#!/usr/bin/env python3
"""
Performance sweeps with qlcstress --mode realtime: the real MasterTimer at
50 Hz, the functions really running, every statistic after warm-up.

For each configuration: per-tick compute time (p50/p95/p99/max), ticks over
the 20 ms budget (and per hour), cadence (gap p99, late ticks), process CPU%,
RSS, and the jitter split by cause (frame lateness, the timer's wake-up
lateness, time descheduled inside a tick; late frames counted as scheduling
or load). A dimension stops one step after its breaking point (compute p99 >
the tick period, or the cadence lost: gap p99 > 1.5 x period), or at
--max-sec of wall time for one configuration (show build + run).

  python3 perf_sweep.py --qlcstress Q --resources R --out DIR [--sweeps a1,a2,b,c,r,real]
                        [--seconds 40] [--warmup 10] [--only-first N]
                        [--refine [--resolution 0.05] [--repeats 3]] [--estimate]
  python3 perf_sweep.py --self-test

--refine: the doubling ladder only brackets each limit (last pass, first
fail: up to 2x apart). After a sweep's ladder, each edge is bisected between
them down to --resolution (5%, or 1 unit when small); every count the edge
rests on is decided by the majority of --repeats runs. Two edges per sweep:
"break" (tick p99 > period, or universe threads behind in >= 5% of samples)
and "jitter" (frame interval p99 > 1.5 x period, or broken). Each is written
to DIR/edges.csv as lo (the largest count that passed most of its runs) and
hi (the smallest above it that failed most), with the counts that had both
outcomes (the noisy band) as "mixed".

Writes DIR/<sweep>/<name>.{json,timeline.csv,log} and DIR/results.csv (one row
per configuration, appended as it goes; a rerun skips configurations whose
JSON is already there). A repeat of a configuration is named <name>-r2, -r3.
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
          "broken", "brokenBy", "rc", "wall_s",
          # jitter, split by cause (qlcstress: lateness = gap - period, wake = the
          # timer woke past its deadline, stall = wall - CPU inside a tick)
          "n", "run", "jitter", "gap_p90", "gap_p999", "late_pct", "late_sched", "late_load",
          "lateness_p99", "lateness_p999", "lateness_max", "wake_p50", "wake_p99", "wake_p999",
          "wake_max", "stall_p99", "stall_p999", "stall_max", "timer_policy", "timer_prio",
          "nice", "qt", "os", "kernel", "rev", "when"]


def mixed(u, fpu=170):
    """The 'mixed' load for u universes: a look per universe, and per 4
    universes a chaser, an RGB matrix (script algorithm) and an EFX; a Show
    Tree-style cue list per 8."""
    q = max(1, math.ceil(u / 4))
    return dict(universes=u, fpu=fpu, scenes=u, chasers=q, matrices=q, efx=q,
                cuelists=max(1, math.ceil(u / 8)), collections=0, scene_fix=min(170, fpu))


def mixed_rig(u):
    """The 'mixed' load on the mixed rig (qlcstress --rig mixed: movers, LED
    pars, pixel bars and strips, dimmers across a stage): a group look per
    universe; per 4 universes a chaser, a matrix over a stage group and a
    pan/tilt EFX on the movers; a cue list of group-look layers per 8."""
    return dict(mixed(u), rig="mixed", perstep=True, scene_fix=0)


UNI_LADDER = [1, 8, 16, 32, 64, 80, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048]


def sweep_table():
    """Every sweep: (key, sweep, name format, ladder, n -> configuration, types
    started, largest n worth refining to). Names are the format of n; the
    ladder is the coarse first pass."""
    rbase = dict(universes=32, fpu=0, scenes=32, chasers=0, matrices=0, efx=0, cuelists=0,
                 collections=0, scene_fix=0, rig="mixed", perstep=True)
    cpbase = dict(universes=32, fpu=170, scenes=64, chasers=0, matrices=0, efx=0, cuelists=0,
                  collections=0, scene_fix=170, perstep=True)
    cbase = dict(universes=32, fpu=170, scenes=0, chasers=0, matrices=0, efx=0, cuelists=0,
                 collections=0, scene_fix=170)
    return [
        # The mixed rig (movers, pars, pixels, dimmers across a stage)
        ("r", "r-rig-mixed-universes", "u%04d", [8, 16, 32, 64, 128, 256, 512], mixed_rig, None, 1024),
        ("r", "r-efx-pantilt", "n%04d", [16, 64, 256, 1024, 4096], lambda n: dict(rbase, efx=n), "efx", None),
        ("r", "r-matrix-stage-groups", "n%04d", [8, 16, 32, 64, 128, 256],
         lambda n: dict(rbase, matrices=n), "matrix", None),
        # Realistic chasers (per-step fade/hold): the same sweeps again, suffixed "p"
        ("a1p", "a1p-universes-mixed-perstep", "u%04d", UNI_LADDER,
         lambda u: dict(mixed(u), perstep=True), None, 1024),
        ("bp", "bp-fixtures-per-universe-perstep", "f%03d", [16, 32, 64, 128, 170],
         lambda f: dict(mixed(32, f), perstep=True), None, 170),
        ("cp", "cp-chasers-perstep", "n%04d", [8, 16, 32, 64, 128, 256, 512, 1024, 2048],
         lambda n: dict(cpbase, chasers=n, scenes=max(64, 4 * n)), "chaser", None),
        # universes, mixed load scaled with them
        ("a1", "a1-universes-mixed", "u%04d", UNI_LADDER, mixed, None, 1024),
        # universes, one static full-universe look each
        ("a2", "a2-universes-static", "u%04d", UNI_LADDER,
         lambda u: dict(universes=u, fpu=170, scenes=u, chasers=0, matrices=0, efx=0,
                        cuelists=0, collections=0, scene_fix=170), "scene", 1024),
        # fixtures per universe, 32 universes, mixed load
        ("b", "b-fixtures-per-universe", "f%03d", [16, 32, 64, 128, 170], lambda f: mixed(32, f), None, 170),
        # one function type at a time on 32 x 170 RGB
        ("c", "c-scenes", "n%04d", [16, 32, 64, 128, 256, 512, 1024, 2048],
         lambda n: dict(cbase, scenes=n), "scene", None),
        ("c", "c-chasers", "n%04d", [8, 16, 32, 64, 128, 256, 512, 1024],
         lambda n: dict(cbase, scenes=64, chasers=n), "chaser", None),
        ("c", "c-rgbmatrix-script", "n%04d", [8, 16, 32, 64, 128, 256, 512, 1024],
         lambda n: dict(cbase, matrices=n), "matrix", None),
        ("c", "c-efx", "n%04d", [16, 32, 64, 128, 256, 512, 1024, 2048, 4096],
         lambda n: dict(cbase, efx=n), "efx", None),
        ("c", "c-cuelists-fades", "n%04d", [2, 4, 8, 16, 32, 64, 128, 256],
         lambda n: dict(cbase, cuelists=n), "cuelist", None),
    ]


def selected(sweeps):
    """The table rows for a --sweeps set: a key ("c") or a sweep's own name."""
    return [t for t in sweep_table() if t[0] in sweeps or t[1] in sweeps]


def plan(sweeps):
    """The coarse ladder: (sweep, name, configuration, types) in run order."""
    out = []
    for key, sweep, fmt, ladder, cfg, types, cap in selected(sweeps):
        for n in ladder:
            out.append((sweep, fmt % n, cfg(n), types))
    return out


# ---- the edge: bisection between the ladder's last pass and first fail ----

def close_enough(lo, hi, resolution):
    """Bracket narrow enough: hi - lo within resolution x lo, or 1 unit."""
    return hi - lo <= max(1, int(math.floor(lo * resolution)))


def summarize(runs, lo=None, hi=None):
    """runs: {n: [passed, ...]} -> the edge. lo/hi: the bracket refine()
    found (the largest count that passed most of its runs, the smallest
    above it that failed most); without them, from the runs alone: hi the
    smallest count that failed every run, lo the largest below it that
    passed every run. mixed: counts with both outcomes (the noisy band);
    inverted: counts above hi that passed most runs."""
    def major(v):
        return sum(v) * 2 > len(v)
    if hi is None and lo is None:
        all_fail = sorted(n for n, v in runs.items() if v and not any(v))
        hi = all_fail[0] if all_fail else None
        below = [n for n, v in runs.items() if v and all(v) and (hi is None or n < hi)]
        lo = max(below) if below else None
    mixed = sorted(n for n, v in runs.items() if v and any(v) and not all(v))
    inverted = sorted(n for n, v in runs.items() if v and hi is not None and n > hi and major(v))
    return dict(lo=lo, hi=hi, mixed=mixed, inverted=inverted, runs=sum(len(v) for v in runs.values()))


def refine(outcome, runs, resolution=0.05, repeats=3, floor=0, max_runs=45):
    """Narrow one edge. outcome(n, k) -> True when the k-th run (1-based) of
    n passed. runs: {n: [passed, ...]} already known (the ladder's), changed
    in place. floor: the count that passes by definition (0 functions) when
    the ladder failed at its first step. Every count the edge rests on is
    decided by the majority of `repeats` runs (stopping early once they
    agree): the ladder's bracket first, then a bisection to `resolution`.
    The edge is where a run fails as often as not; the mixed counts show
    how wide the noisy band around it is. At most `max_runs` new runs."""
    count = [0]
    need = repeats // 2 + 1

    def run(n):
        if count[0] >= max_runs:
            raise StopIteration
        count[0] += 1
        runs.setdefault(n, []).append(bool(outcome(n, len(runs.get(n, [])) + 1)))

    def probe(n):
        if n == floor and n not in runs:
            return True          # passes by definition
        while True:
            v = runs.get(n, [])
            p, f = sum(v), len(v) - sum(v)
            if p >= need or f >= need or len(v) >= repeats:
                return p > f
            run(n)

    lo = hi = None
    try:
        # the ladder's first fail that holds up, and the last pass below it
        for n in sorted(x for x, v in runs.items() if v and not all(v)):
            if not probe(n):
                hi = n
                break
        if hi is None:          # nothing failed for real: a lower bound
            return summarize(runs, max(runs) if runs else None, None)
        for n in sorted((x for x in runs if x < hi), reverse=True) + [floor]:
            if probe(n):
                lo = n
                break
        if lo is None:
            lo = floor
        while not close_enough(lo, hi, resolution):
            mid = (lo + hi) // 2
            if probe(mid):
                lo = mid
            else:
                hi = mid
    except StopIteration:
        pass
    if hi is None:
        return summarize(runs)
    return summarize(runs, lo if lo is not None else floor, hi)


def edge_value(e, floor=0):
    """(value, plus/minus) of an edge: the middle of lo..hi and half its
    width; (lo, None) when nothing failed (a lower bound)."""
    if e["hi"] is None:
        return e["lo"], None
    lo = e["lo"] if e["lo"] is not None else floor
    return (lo + e["hi"]) / 2.0, (e["hi"] - lo) / 2.0


def passes(row, edge):
    """Did this run pass the edge's test? None (no result) fails."""
    if row is None or row.get("compute_p99") in (None, ""):
        return False
    if edge == "break":
        return not any(w in (row.get("brokenBy") or "") for w in ("compute", "universes"))
    return not row.get("broken")       # jitter: any of compute, cadence, universes


def estimate(sweeps, seconds, warmup, refine_on, resolution, repeats, build_s=15):
    """Rough wall time: the whole ladder (a sweep stops earlier past its
    break), plus per edge the bisection probes and the bracket's ends."""
    per = seconds + warmup + build_s
    total = 0
    for key, sweep, fmt, ladder, cfg, types, cap in selected(sweeps):
        steps = len(ladder)
        total += steps
        if refine_on:
            bis = int(math.ceil(math.log(1.0 / resolution, 2)))     # 2x bracket -> resolution
            total += 2 * (bis * (repeats // 2 + 1) + 2 * (repeats - 1))   # probes of 2-3 runs
    return total, total * per


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
    if cfg.get("rig"):
        cmd += ["--rig", cfg["rig"]]
    if a.tick_csv:
        cmd += ["--tick-csv"]
    wrap = getattr(a, "wrap", None)
    if wrap:
        cmd = wrap.split() + cmd
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


def jitter_of(r):
    """The jitter fields of a qlcstress JSON (absent from older builds: empty)."""
    g = r.get("gap", {})
    lt, wk, st = r.get("lateness"), r.get("wake"), r.get("stall")
    ticks = r.get("ticks") or 0
    period = r.get("periodMs", 20.0)
    out = dict(gap_p90=g.get("p90"), gap_p999=g.get("p999"),
               late_pct=round(r.get("latePct", 100.0 * r.get("late", 0) / ticks if ticks else 0), 3),
               jitter=bool(g.get("p99", 0) > 1.5 * period),
               timer_policy=r.get("timerPolicy", ""), timer_prio=r.get("timerPriority", ""),
               qt=r.get("qt", ""), os=r.get("os", ""), kernel=r.get("kernel", ""), when=r.get("when", ""))
    if lt is not None:
        out.update(late_sched=int(r.get("lateSched", 0)), late_load=int(r.get("lateLoad", 0)),
                   lateness_p99=lt["p99"], lateness_p999=lt["p999"], lateness_max=round(lt["max"], 3),
                   wake_p50=wk["p50"], wake_p99=wk["p99"], wake_p999=wk["p999"], wake_max=round(wk["max"], 3),
                   stall_p99=st["p99"], stall_p999=st["p999"], stall_max=round(st["max"], 3))
    return out


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
                rc=rc, wall_s=round(wall, 1), **jitter_of(r))


EDGE_FIELDS = ["sweep", "edge", "lo", "hi", "value", "pm", "pm_pct", "lower_bound", "mixed", "inverted",
               "runs", "resolution", "repeats", "host", "timer_policy", "rev", "date"]


def edge_row(sweep, edge, e, rows, resolution, repeats, rev, floor=0):
    """One line of edges.csv from refine()'s result and the runs' rows."""
    value, pm = edge_value(e, floor)
    mixed = " ".join("%d:%d/%d" % (n, sum(1 for r in rows.get(n, []) if passes(r, edge)), len(rows.get(n, [])))
                     for n in e["mixed"])
    any_row = next((r for v in rows.values() for r in v if r is not None and r.get("host")), {})
    return dict(sweep=sweep, edge=edge, lo=e["lo"], hi=e["hi"],
                value=None if value is None else round(value, 1), pm=None if pm is None else round(pm, 1),
                pm_pct=None if pm is None or not value else round(100.0 * pm / value, 1),
                lower_bound=e["hi"] is None, mixed=mixed, inverted=" ".join(map(str, e["inverted"])),
                runs=e["runs"], resolution=resolution, repeats=repeats, host=any_row.get("host", ""),
                timer_policy=any_row.get("timer_policy", ""), rev=rev, date=time.strftime("%Y-%m-%d"))


def self_test():
    """The edge logic on made-up outcomes: no qlcstress, under a second."""
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)
            print("self-test FAILED:", what)

    check(close_enough(100, 105, 0.05) and not close_enough(100, 106, 0.05), "5 percent of 100")
    check(close_enough(8, 9, 0.05) and not close_enough(8, 10, 0.05), "1 unit when small")
    # a clean threshold at 100 (100 passes, 101 fails), ladder 64 pass / 128 fail
    calls = []

    def at(t):
        def f(n, k):
            calls.append((n, k))
            return n <= t
        return f
    runs = {64: [True], 128: [False]}
    e = refine(at(100), runs, 0.05, 3)
    check((e["lo"], e["hi"]) == (100, 104) or (e["lo"] <= 100 < e["hi"] and e["hi"] - e["lo"] <= 5),
          "bracket holds 100 within 5%%: %r" % e)
    check(all(len(runs[n]) >= 2 for n in (e["lo"], e["hi"])), "each end decided by 2 of 3 runs")
    check(len(calls) <= 20, "bisection is short (%d runs)" % len(calls))
    v, pm = edge_value(e)
    check(abs(v - 100.5) <= 3 and pm <= 3, "value %.1f +- %.1f" % (v, pm))
    # small counts: resolution 1 unit; the ladder failed at its first step (8)
    runs = {8: [False]}
    e = refine(at(2), runs, 0.05, 3)
    check((e["lo"], e["hi"]) == (2, 3), "fails at the first step: bisect from 0 -> %r" % e)
    runs = {8: [False]}
    e = refine(at(0), runs, 0.05, 3)
    check((e["lo"], e["hi"]) == (0, 1), "nothing passes: 0..1: %r" % e)
    check(edge_value(e) == (0.5, 0.5), "value from the floor")
    # nothing failed: a lower bound, no runs
    calls[:] = []
    runs = {1024: [True], 2048: [True]}
    e = refine(at(10 ** 6), runs)
    check(e["hi"] is None and e["lo"] == 2048 and edge_value(e) == (2048, None) and not calls,
          "lower bound: %r" % e)
    # a ladder fail that does not hold up (one bad run at 128; 256 fails)
    runs = {64: [True], 128: [False], 256: [False]}
    e = refine(lambda n, k: n <= 200 if n != 128 else k != 1, runs, 0.05, 3)
    check(e["lo"] is not None and 190 <= e["lo"] <= 200 < e["hi"] and 128 in e["mixed"],
          "a noisy ladder fail is outvoted: %r" % e)
    # a noisy band (the Pi with SCHED_FIFO, 2026-11-03): runs at 120..148
    # fail one time in three -- most pass, so the edge is above the band
    def band(n, k):
        return n <= 110 or (n <= 148 and k != 2)
    runs = {80: [True], 128: [True], 132: [True], 136: [True], 152: [False]}
    e = refine(band, runs, 0.05, 3)
    check(e["lo"] is not None and 144 <= e["lo"] <= 148 < e["hi"] <= 152, "majority edge above the band: %r" % e)
    check(e["mixed"] and e["mixed"][0] > 110, "the band is in mixed: %r" % e["mixed"])
    # and one where most runs fail: the edge is below the band
    runs = {80: [True], 128: [True], 152: [False]}
    e = refine(lambda n, k: n <= 110 or (n <= 148 and k == 1), runs, 0.05, 3)
    check(e["lo"] is not None and 105 <= e["lo"] <= 110 < e["hi"] <= 116, "majority edge below the band: %r" % e)
    calls[:] = []
    runs = {64: [True], 128: [False]}
    e = refine(lambda n, k: calls.append(n) or n <= 100, runs, 0.05, 3, max_runs=4)
    check(len(calls) == 4 and e["hi"] is not None, "max_runs caps the new runs (%d)" % len(calls))
    # summarize from runs alone: inverted and mixed
    s = summarize({10: [True], 12: [False], 14: [True], 16: [False]})
    check(s["lo"] == 10 and s["hi"] == 12 and s["inverted"] == [14], "inverted noted: %r" % s)
    s = summarize({10: [True], 11: [True, False], 12: [False, False]})
    check(s["lo"] == 10 and s["hi"] == 12 and s["mixed"] == [11], "mixed: %r" % s)
    # passes(): the two edges
    check(passes(dict(compute_p99=5, brokenBy="cadence", broken=True), "break"), "cadence is not a break")
    check(not passes(dict(compute_p99=5, brokenBy="cadence", broken=True), "jitter"), "cadence is jitter")
    check(not passes(dict(compute_p99=25, brokenBy="compute", broken=True), "break"), "compute breaks")
    check(not passes(None, "break") and not passes(dict(compute_p99=None), "jitter"), "no result fails")
    # the sweep table: the ladders guard.py and the old plan used are unchanged
    names = [x[0] for x in plan({"a1p", "c", "cp"})]
    check(names.count("c-efx") == 9 and names.count("a1p-universes-mixed-perstep") == 15, "ladders kept")
    check(plan({"a1p"})[5][1] == "u0080", "names kept")
    # jitter_of: an old JSON without the split, and a new one
    old = dict(gap=dict(p99=25, p999=30), ticks=1000, late=5, periodMs=20.0)
    j = jitter_of(old)
    check(j["late_pct"] == 0.5 and j["jitter"] is False and "wake_p99" not in j, "old JSON: %r" % j)
    h = dict(p50=0, p90=0, p99=1, p999=2, max=3)
    new = dict(old, gap=dict(p99=31, p999=40), latePct=1.5, lateness=h, wake=h, stall=h, lateSched=3,
               lateLoad=2, timerPolicy="fifo", timerPriority=80)
    j = jitter_of(new)
    check(j["jitter"] is True and j["late_pct"] == 1.5 and j["wake_p99"] == 1 and j["timer_policy"] == "fifo",
          "new JSON: %r" % j)
    n, sec = estimate({"a1p"}, 40, 10, True, 0.05, 3)
    check(n == 15 + 2 * (5 * 2 + 4) and sec == n * 65, "estimate %d runs %d s" % (n, sec))
    print("perf_sweep self-test:", "ok" if not fails else "FAILED (%d)" % len(fails))
    return 0 if not fails else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qlcstress")
    ap.add_argument("--resources", help="the source tree's resources/")
    ap.add_argument("--out")
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
    ap.add_argument("--refine", action="store_true", help="bisect each edge after the ladder (see above)")
    ap.add_argument("--resolution", type=float, default=0.05, help="--refine: bracket width, of the count")
    ap.add_argument("--repeats", type=int, default=3, help="--refine: runs of each end of the bracket")
    ap.add_argument("--wrap", default="", help="a command prefix for every run (prlimit ... for real-time)")
    ap.add_argument("--rev", default=os.environ.get("QLC_PERF_REV", ""), help="the source's SHA, recorded")
    ap.add_argument("--quiet-cmd", default="",
                    help="a shell command that exits 0 when the host is quiet enough to measure: checked "
                         "before each run (waits) and after it (a run that overlapped other work is redone)")
    ap.add_argument("--estimate", action="store_true", help="print the runs and hours this would take; run nothing")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        sys.exit(self_test())
    sweeps = set(a.sweeps.split(","))
    if a.estimate:
        n, sec = estimate(sweeps, a.seconds, a.warmup, a.refine, a.resolution, a.repeats)
        print("about %d runs, %.1f h (ladder to its end; a sweep stops earlier past its break)" % (n, sec / 3600.0))
        return
    if not (a.qlcstress and a.resources and a.out):
        ap.error("--qlcstress, --resources and --out are needed")
    os.makedirs(a.out, exist_ok=True)
    res_path = os.path.join(a.out, "results.csv")
    new = not os.path.exists(res_path)
    f = open(res_path, "a", newline="", buffering=1)
    w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
    if new:
        w.writeheader()
    edges_path = os.path.join(a.out, "edges.csv")
    ef = None
    if a.refine:
        enew = not os.path.exists(edges_path)
        ef = open(edges_path, "a", newline="", buffering=1)
        ew = csv.DictWriter(ef, fieldnames=EDGE_FIELDS, extrasaction="ignore")
        if enew:
            ew.writeheader()

    def quiet():
        return not a.quiet_cmd or subprocess.run(a.quiet_cmd, shell=True).returncode == 0

    def measure(sweep, fmt, n, k, cfg, types):
        name = fmt % n + ("" if k == 1 else "-r%d" % k)
        prefix = os.path.join(a.out, sweep, name)
        for attempt in range(6):
            waited = 0
            while not os.path.exists(prefix + ".json") and not quiet():
                if waited % 600 == 0:
                    print("%s waiting: the host is busy (%s)" % (time.strftime("%H:%M:%S"), a.quiet_cmd), flush=True)
                time.sleep(30)
                waited += 30
            r, rc, wall, cached = run_one(a, sweep, name, cfg(n), types, a.out)
            if cached or quiet():
                break
            # something else started during the run: it does not count
            print("%s %s: the host got busy during the run; running it again" % (time.strftime("%H:%M:%S"), name),
                  flush=True)
            for ext in (".json", ".timeline.csv", ".log", ".ticks.csv"):
                if os.path.exists(prefix + ext):
                    os.rename(prefix + ext, prefix + ext + ".busy%d" % attempt)
        row = row_of(sweep, name, r, rc, wall)
        row.update(n=n, run=k, rev=a.rev, nice=os.getpriority(os.PRIO_PROCESS, 0))
        if not cached:
            w.writerow(row)
        print("%s %-26s %-9s %s" % (time.strftime("%H:%M:%S"), sweep, name,
              "FAILED rc=%s" % rc if r is None else
              "p99 %.2f max %.2f ms over %d gap99 %.2f gap999 %s late %.2f%% wake99 %s cpu %.0f%% rss %.0f MB "
              "run %.0f lag %.2f%s" % (
                  row["compute_p99"], row["compute_max"], row["overBudget"], row["gap_p99"],
                  row.get("gap_p999"), row.get("late_pct") or 0, row.get("wake_p99"),
                  row["cpuPct"], row["rssPeak"], row["runningMean"], row["uniLaggingSampleFrac"],
                  "  BROKEN(%s)" % row["brokenBy"] if row["broken"] else "")), flush=True)
        return r, row

    for key, sweep, fmt, ladder, cfg, types, cap in selected(sweeps):
        broken_count = compute_broken = 0
        rows = {}               # n -> [row of run 1, run 2, ...]
        for i, n in enumerate(ladder):
            if a.only_first and i >= a.only_first:
                break
            if compute_broken > a.past_break or broken_count > a.past_any_break:
                break
            r, row = measure(sweep, fmt, n, 1, cfg, types)
            rows[n] = [row]
            if row["broken"]:
                broken_count += 1
            if r is None or "compute" in row.get("brokenBy", ""):
                compute_broken += 1
            if a.max_rss_mb and r is not None and r["rssPeak"] > a.max_rss_mb:
                compute_broken = a.past_break + 1
        if not a.refine:
            continue
        for edge in ("jitter", "break"):
            runs = {n: [passes(r, edge) for r in v] for n, v in rows.items() if cap is None or n <= cap}

            def outcome(n, k, edge=edge):
                while len(rows.setdefault(n, [])) < k:
                    rows[n].append(measure(sweep, fmt, n, len(rows[n]) + 1, cfg, types)[1])
                return passes(rows[n][k - 1], edge)
            e = refine(outcome, runs, a.resolution, a.repeats)
            er = edge_row(sweep, edge, e, rows, a.resolution, a.repeats, a.rev)
            ew.writerow(er)
            print("%s %-26s EDGE %-6s %s" % (time.strftime("%H:%M:%S"), sweep, edge,
                  ">= %s" % er["lo"] if er["lower_bound"] else
                  "%s +- %s (pass %s, fail %s%s; %d runs)" % (er["value"], er["pm"], er["lo"], er["hi"],
                                                            ", mixed " + er["mixed"] if er["mixed"] else "",
                                                            er["runs"])), flush=True)
    print("sweeps done", flush=True)


if __name__ == "__main__":
    main()
