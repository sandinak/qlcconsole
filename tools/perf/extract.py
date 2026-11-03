#!/usr/bin/env python3
"""Turn a perf campaign's tables into the small CSVs behind docs/performance.md.

  python3 -I tools/perf/extract.py CAMPAIGN_DIR [OUT_DIR]

CAMPAIGN_DIR is what the campaign's collect.sh / analyze.py leave behind:
CAMPAIGN_DIR/tables/{sweep_<host>.csv, wire.csv, io.csv, breaking_points.csv,
soak_hourly.csv, soak_summary.csv} and CAMPAIGN_DIR/<host>/soak/samples.csv.
OUT_DIR defaults to docs/perf-data. Only the numbers the page and its charts
use are kept, under neutral host ids (pi, linux, m4); docs/perf-data/hosts.csv
(written by hand) says what each host is. Then run tools/perf/plot.py.

Stdlib only. Rows come out in a fixed order, so the same input gives the
same files.

  python3 -I tools/perf/extract.py precise HOST PRIORITY SWEEP_DIR [OUT_DIR]
  python3 -I tools/perf/extract.py --self-test

`precise` takes one perf_sweep.py --refine output folder (results.csv,
edges.csv) of one host (pi, linux, m4max, ...) at one timer priority
(normal, rt) and merges it into OUT_DIR/edges.csv (each limit as lo, hi,
value +- half the bracket) and OUT_DIR/jitter.csv (every run: the frame
interval distribution, late frames by cause, the timer's wake-up lateness).
Rows of the same host, priority and sweep are replaced; the rest are kept.
"""
import csv
import os
import sys

# campaign folder name -> published host id
HOSTS = [("pi", "pi"), ("linux", "linux"), ("dink", "m4")]


def fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def step(name):
    """'u0128' / 'n0256' / 'f170' -> 128 / 256 / 170; anything else -> None."""
    if name and name[0] in "unf" and name[1:].isdigit():
        return int(name[1:])
    return None


def fmt(v):
    if v is None:
        return ""
    if isinstance(v, float):
        return ("%.2f" % v).rstrip("0").rstrip(".")
    return str(v)


def write(path, cols, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(cols)
        for r in rows:
            w.writerow([fmt(r.get(c)) for c in cols])
    print("wrote %s (%d rows)" % (path, len(rows)))


def read(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


EDGE_COLS = ["host", "priority", "sweep", "edge", "lo", "hi", "value", "pm", "pm_pct", "lower_bound",
             "band_lo", "band_hi", "mixed", "runs", "timer_policy", "qt", "os", "rev", "date"]
JITTER_COLS = ["host", "priority", "sweep", "n", "run", "tick_p99_ms", "frame_p50_ms", "frame_p99_ms",
               "frame_p999_ms", "frame_max_ms", "late_pct", "late_sched", "late_load", "wake_p99_ms",
               "wake_p999_ms", "wake_max_ms", "stall_p99_ms", "stall_max_ms", "cpu_pct", "broken",
               "timer_policy"]


def merge(path, cols, key, new_rows, sort_key):
    """Replace the rows of path whose key() is among new_rows' keys; keep the
    rest; sort; write."""
    keys = {key(r) for r in new_rows}
    kept = [r for r in read(path) if key(r) not in keys]
    rows = kept + [{c: fmt(r.get(c)) for c in cols} for r in new_rows]
    rows.sort(key=sort_key)
    write(path, cols, rows)


def num_sort(x):
    v = fnum(x)
    return v if v is not None else -1.0


def precise(host, priority, sweep_dir, out):
    results = read(os.path.join(sweep_dir, "results.csv"))
    edges = read(os.path.join(sweep_dir, "edges.csv"))
    if not results:
        sys.exit("extract: no results.csv in %s" % sweep_dir)
    first = {}
    for r in results:
        first.setdefault(r["sweep"], r)
    # the last edge line of a sweep and kind wins (a resumed run appends)
    last = {}
    for e in edges:
        last[(e["sweep"], e["edge"])] = e
    erows = []
    for (sweep, edge), e in sorted(last.items()):
        r0 = first.get(sweep, {})
        erows.append(dict(host=host, priority=priority, sweep=sweep, edge=edge,
                          lo=e["lo"], hi=e["hi"], value=fnum(e["value"]), pm=fnum(e["pm"]),
                          pm_pct=fnum(e["pm_pct"]), lower_bound=e["lower_bound"], mixed=e["mixed"],
                          runs=e["runs"], timer_policy=e.get("timer_policy") or r0.get("timer_policy", ""),
                          qt=r0.get("qt", ""), os=r0.get("os", ""), rev=e.get("rev", "") or r0.get("rev", ""),
                          date=e.get("date", "")))
    jrows = []
    for r in results:
        n = fnum(r.get("n"))
        if n is None:           # an old run without the step's count: from the name
            n = step(r["name"].split("-")[0])
        if n is None or r.get("compute_p99") in (None, ""):
            continue
        jrows.append(dict(host=host, priority=priority, sweep=r["sweep"], n=int(n),
                          run=int(fnum(r.get("run")) or 1),
                          tick_p99_ms=fnum(r["compute_p99"]), frame_p50_ms=fnum(r["gap_p50"]),
                          frame_p99_ms=fnum(r["gap_p99"]), frame_p999_ms=fnum(r.get("gap_p999")),
                          frame_max_ms=fnum(r["gap_max"]), late_pct=fnum(r.get("late_pct")),
                          late_sched=r.get("late_sched", ""), late_load=r.get("late_load", ""),
                          wake_p99_ms=fnum(r.get("wake_p99")), wake_p999_ms=fnum(r.get("wake_p999")),
                          wake_max_ms=fnum(r.get("wake_max")), stall_p99_ms=fnum(r.get("stall_p99")),
                          stall_max_ms=fnum(r.get("stall_max")), cpu_pct=fnum(r["cpuPct"]),
                          broken=r.get("brokenBy", ""), timer_policy=r.get("timer_policy", "")))
    # the noisy band, from every run of the sweep: the smallest count that
    # failed the edge's test in any run, the largest that passed in any
    for er in erows:
        if er["edge"] == "break":
            bad = lambda j: any(w in j["broken"] for w in ("compute", "universes"))
        else:
            bad = lambda j: bool(j["broken"])
        js = [j for j in jrows if j["sweep"] == er["sweep"]]
        fails = [j["n"] for j in js if bad(j)]
        oks = [j["n"] for j in js if not bad(j)]
        er["band_lo"] = min(fails) if fails else None
        er["band_hi"] = max((n for n in oks if fails and n > min(fails)), default=None)
    hkey = lambda r: (r["host"], r["priority"], r["sweep"])
    merge(os.path.join(out, "edges.csv"), EDGE_COLS, hkey, erows,
          lambda r: (r["host"], r["priority"], r["sweep"], r["edge"]))
    merge(os.path.join(out, "jitter.csv"), JITTER_COLS, hkey, jrows,
          lambda r: (r["host"], r["priority"], r["sweep"], num_sort(r["n"]), num_sort(r["run"])))


def self_test():
    import tempfile
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)
            print("self-test FAILED:", what)
    with tempfile.TemporaryDirectory() as d:
        sd, out = os.path.join(d, "sweep"), os.path.join(d, "out")
        os.makedirs(sd)
        os.makedirs(out)
        rc = ["sweep", "name", "n", "run", "compute_p99", "gap_p50", "gap_p99", "gap_p999", "gap_max",
              "late_pct", "late_sched", "late_load", "wake_p99", "wake_p999", "wake_max", "stall_p99",
              "stall_max", "cpuPct", "brokenBy", "timer_policy", "qt", "os", "rev"]
        rows = [dict(sweep="c-efx", name="n0016", n=16, run=1, compute_p99=1.0, gap_p50=20, gap_p99=21,
                     gap_p999=22.5, gap_max=30, late_pct=0, late_sched=0, late_load=0, wake_p99=0.1,
                     wake_p999=0.2, wake_max=1.5, stall_p99=0.01, stall_max=0.3, cpuPct=50, brokenBy="",
                     timer_policy="other", qt="6.8.2", os="Debian 13", rev="abc"),
                dict(sweep="c-efx", name="n0100-r2", n=100, run=2, compute_p99=25, gap_p50=25, gap_p99=40,
                     gap_p999="", gap_max=60, late_pct=12.5, late_sched=1, late_load=99, wake_p99="",
                     wake_p999="", wake_max="", stall_p99="", stall_max="", cpuPct=300,
                     brokenBy="compute+cadence", timer_policy="other", qt="6.8.2", os="Debian 13", rev="abc"),
                # an old row: no n/run columns, the count from the name
                dict(sweep="c-scenes", name="n0512", compute_p99=0.5, gap_p50=20, gap_p99=21, gap_max=25,
                     cpuPct=20)]
        write(os.path.join(sd, "results.csv"), rc, rows)
        ec = ["sweep", "edge", "lo", "hi", "value", "pm", "pm_pct", "lower_bound", "mixed", "runs",
              "timer_policy", "rev", "date"]
        write(os.path.join(sd, "edges.csv"), ec, [
            dict(sweep="c-efx", edge="break", lo=96, hi=100, value=98.0, pm=2.0, pm_pct=2.0,
                 lower_bound=False, mixed="", runs=9, timer_policy="other", rev="abc", date="2026-11-03"),
            dict(sweep="c-efx", edge="break", lo=97, hi=100, value=98.5, pm=1.5, pm_pct=1.5,
                 lower_bound=False, mixed="98:2/3", runs=12, timer_policy="other", rev="abc", date="2026-11-03"),
            dict(sweep="c-scenes", edge="jitter", lo=2048, hi="", value=2048, pm="", pm_pct="",
                 lower_bound=True, mixed="", runs=8, rev="abc", date="2026-11-03")])
        # an existing row of another host stays; one of this host and sweep is replaced
        write(os.path.join(out, "edges.csv"), EDGE_COLS, [
            dict(host="pi", priority="normal", sweep="c-efx", edge="break", lo=1, hi=2),
            dict(host="linux", priority="normal", sweep="c-efx", edge="break", lo=5, hi=6)])
        precise("linux", "normal", sd, out)
        precise("linux", "normal", sd, out)          # idempotent
        e = read(os.path.join(out, "edges.csv"))
        check(len(e) == 3, "3 edge rows, got %d" % len(e))
        lin = [r for r in e if r["host"] == "linux" and r["sweep"] == "c-efx"]
        check(len(lin) == 1 and lin[0]["lo"] == "97" and lin[0]["mixed"] == "98:2/3", "last edge wins: %r" % lin)
        check(lin[0]["qt"] == "6.8.2" and lin[0]["rev"] == "abc", "provenance carried")
        check(any(r["host"] == "pi" for r in e), "other hosts kept")
        sc = [r for r in e if r["sweep"] == "c-scenes"]
        check(sc and sc[0]["lower_bound"] == "True" and sc[0]["hi"] == "", "lower bound: %r" % sc)
        j = read(os.path.join(out, "jitter.csv"))
        check(len(j) == 3, "3 jitter rows, got %d" % len(j))
        check([(r["sweep"], r["n"], r["run"]) for r in j] ==
              [("c-efx", "16", "1"), ("c-efx", "100", "2"), ("c-scenes", "512", "1")], "order: %r" %
              [(r["sweep"], r["n"], r["run"]) for r in j])
        check(j[0]["frame_p999_ms"] == "22.5" and j[0]["wake_max_ms"] == "1.5", "jitter fields")
        lin = [r for r in read(os.path.join(out, "edges.csv")) if r["host"] == "linux" and r["sweep"] == "c-efx"]
        check(lin[0]["band_lo"] == "100" and lin[0]["band_hi"] == "", "band from the runs: %r" % lin)
        precise("linux", "rt", sd, out)
        check(len(read(os.path.join(out, "edges.csv"))) == 5, "a second priority adds rows")
    print("extract self-test:", "ok" if not fails else "FAILED (%d)" % len(fails))
    return 0 if not fails else 1


def main():
    if sys.argv[1:2] == ["--self-test"]:
        sys.exit(self_test())
    if sys.argv[1:2] == ["precise"]:
        if len(sys.argv) < 5:
            sys.exit(__doc__)
        here = os.path.dirname(os.path.abspath(__file__))
        out = sys.argv[5] if len(sys.argv) > 5 else os.path.join(here, "..", "..", "docs", "perf-data")
        precise(sys.argv[2], sys.argv[3], sys.argv[4], out)
        return
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "..", "docs", "perf-data")
    os.makedirs(out, exist_ok=True)
    t = os.path.join(root, "tables")

    # Scaling: the realistic mixed load (per-step chasers), universes 1..1024.
    # Steps past 1024 are past the engine's universe maximum: not valid.
    rows = []
    for src, hid in HOSTS:
        for r in read(os.path.join(t, "sweep_%s.csv" % src)):
            if r["sweep"] != "a1p-universes-mixed-perstep":
                continue
            u = int(r["universes"])
            if u > 1024:
                continue
            rows.append(dict(host=hid, universes=u, fixtures=int(r["fixtures"]),
                             channels=int(r["channels"]),
                             tick_p50_ms=fnum(r["compute_p50"]), tick_p99_ms=fnum(r["compute_p99"]),
                             tick_max_ms=fnum(r["compute_max"]), cpu_pct=fnum(r["cpuPct"]),
                             frame_interval_p99_ms=fnum(r["gap_p99"]), rss_peak_mb=fnum(r["rssPeak"]),
                             broken=r["brokenBy"] or ""))
    write(os.path.join(out, "scaling.csv"),
          ["host", "universes", "fixtures", "channels", "tick_p50_ms", "tick_p99_ms", "tick_max_ms",
           "cpu_pct", "frame_interval_p99_ms", "rss_peak_mb", "broken"], rows)

    # Art-Net on the wire
    rows = []
    wire = read(os.path.join(t, "wire.csv"))
    for src, hid in HOSTS:
        for r in wire:
            if r["host"] == src:
                rows.append(dict(host=hid, universes=int(r["universes"]), fps_min=fnum(r["fps_min"]),
                                 fps_mean=fnum(r["fps_mean"]), frame_gap_p99_ms=fnum(r["frame_gap_p99_max"]),
                                 late_frames=int(r["late_frames"]), frames=int(r["frames"]),
                                 seq_gaps=int(r["seq_skips"]), cpu_pct=fnum(r["cpuPct"])))
    write(os.path.join(out, "artnet.csv"),
          ["host", "universes", "fps_min", "fps_mean", "frame_gap_p99_ms", "frames", "late_frames",
           "seq_gaps", "cpu_pct"], rows)

    # Per-function limits on 32 x 170 RGB fixtures
    funcs = [("c-scenes", "Static 170-fixture looks"),
             ("cp-chasers-perstep", "Chasers, per-step timing"),
             ("c-chasers", "Chasers stepping every tick (worst case)"),
             ("c-rgbmatrix-script", "Scripted RGB matrices"),
             ("c-efx", "EFX"),
             ("c-cuelists-fades", "Cue lists (8 cues x 4 layers, 2 s fades)")]
    bp = read(os.path.join(t, "breaking_points.csv"))
    rows = []
    for sweep, label in funcs:
        for src, hid in HOSTS:
            for r in bp:
                if r["host"] == src and r["sweep"] == sweep:
                    brk = step(r["hard_break"])
                    rows.append(dict(function=label, host=hid, last_ok=step(r["last_ok"]),
                                     hard_break=brk,
                                     # no break in range: the largest step tried passed
                                     at_least=step(r["max_tested"]) if brk is None else None,
                                     jitter_from=step(r["jitter_limit"])))
    write(os.path.join(out, "limits.csv"),
          ["function", "host", "last_ok", "hard_break", "at_least", "jitter_from"], rows)

    # Full app under input load
    rows = []
    io = read(os.path.join(t, "io.csv"))
    for src, hid in HOSTS:
        for r in io:
            if r["host"] == src:
                rows.append(dict(host=hid, phase=r["name"], cpu_pct=fnum(r["cpu_pct"]),
                                 ticks_over_per_h=fnum(r["ticks_over_per_hour"]),
                                 ws_rtt_p99_ms=fnum(r["probe_rtt_p99"]),
                                 web_req_per_s=fnum(r["web_req_per_s"]),
                                 out_fps_min=fnum(r["out_fps_min"]), out_fps_mean=fnum(r["out_fps_mean"])))
    write(os.path.join(out, "io_load.csv"),
          ["host", "phase", "cpu_pct", "ticks_over_per_h", "ws_rtt_p99_ms", "web_req_per_s",
           "out_fps_min", "out_fps_mean"], rows)

    # Long runs: per hour (the main soak only; "soak-*" folders were aborted runs)
    rows = []
    hourly = read(os.path.join(t, "soak_hourly.csv"))
    for src, hid in HOSTS:
        for r in hourly:
            if r["host"] == src and r["run"] == "soak":
                rows.append(dict(host=hid, phase=r["phase"], hour=int(r["hour"]), samples=int(r["samples"]),
                                 ticks_over=int(r["ticks_over"]), ticks_over_per_h=fnum(r["ticks_over_per_h"]),
                                 worst_ms=fnum(r["worst_ms"]), cpu_pct=fnum(r["cpu_mean"]),
                                 mem_kind=r["leak_signal"], mem_mb=fnum(r["leak_end"])))
    write(os.path.join(out, "soak_hourly.csv"),
          ["host", "phase", "hour", "samples", "ticks_over", "ticks_over_per_h", "worst_ms", "cpu_pct",
           "mem_kind", "mem_mb"], rows)

    rows = []
    summ = read(os.path.join(t, "soak_summary.csv"))
    for src, hid in HOSTS:
        for r in summ:
            if r["host"] == src and r["run"] == "soak":
                rows.append(dict(host=hid, phase=r["phase"], hours=fnum(r["hours"]),
                                 ticks_over=int(r["ticks_over"]), ticks_over_per_h=fnum(r["ticks_over_per_h"]),
                                 worst_ms=fnum(r["worst_ms"]), cpu_pct=fnum(r["cpu_mean"]),
                                 mem_kind=r["leak_signal"], mem_first_mb=fnum(r["leak_first"]),
                                 mem_last_mb=fnum(r["leak_last"]),
                                 mem_slope_mb_per_h=fnum(r["leak_slope_mb_h"])))
    write(os.path.join(out, "soak_summary.csv"),
          ["host", "phase", "hours", "ticks_over", "ticks_over_per_h", "worst_ms", "cpu_pct", "mem_kind",
           "mem_first_mb", "mem_last_mb", "mem_slope_mb_per_h"], rows)

    # Long runs: memory every 10 minutes (mean of the 30 s samples in each bin)
    rows = []
    for src, hid in HOSTS:
        samples = read(os.path.join(root, src, "soak", "samples.csv"))
        if not samples:
            continue
        kind = "footprint_mb" if "footprint_mb" in samples[0] else "private_mb"
        bins = {}
        for s in samples:
            el, mem = fnum(s["elapsed_s"]), fnum(s.get(kind))
            if el is None or mem is None:
                continue
            key = (s["phase"], int(el // 600))
            bins.setdefault(key, []).append((mem, fnum(s["rss_mb"]), fnum(s["cpu_pct"])))
        for phase in ("real", "scaled"):
            keys = sorted(k for k in bins if k[0] == phase)
            if not keys:
                continue
            first = keys[0][1]
            for k in keys:
                b = bins[k]
                rows.append(dict(host=hid, phase=phase, minutes=(k[1] - first) * 10 + 5, mem_kind=kind,
                                 mem_mb=sum(x[0] for x in b) / len(b),
                                 rss_mb=sum(x[1] for x in b if x[1] is not None) / len(b),
                                 cpu_pct=sum(x[2] for x in b if x[2] is not None) / len(b)))
    write(os.path.join(out, "soak_memory.csv"),
          ["host", "phase", "minutes", "mem_kind", "mem_mb", "rss_mb", "cpu_pct"], rows)


if __name__ == "__main__":
    main()
