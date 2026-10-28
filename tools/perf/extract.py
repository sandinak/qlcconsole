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


def main():
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
