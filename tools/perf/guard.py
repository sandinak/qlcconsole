#!/usr/bin/env python3
"""
The performance guard (make perf): a short, repeatable engine benchmark,
compared with this machine's own baseline.

    python3 tools/perf/guard.py                # check against the baseline
    python3 tools/perf/guard.py --record       # (re)record it: 3 runs, noise measured
    python3 tools/perf/guard.py --self-test    # the comparison logic (the gate runs this)

What it runs (offscreen, no I/O plugins, the engine only, Release build in
build-perf/):

  * qlcstress engine --mode realtime -- the real 50 Hz MasterTimer and
    universe threads, the same harness and loads as docs/performance.md --
    at a few fixed points: the realistic mixed load (sweep a1p) at 8, 32,
    128 and 256 universes, and one function type at a time on 32 universes
    (64 per-step chasers, 64 scripted RGB matrices, 256 EFX). 40 s each
    after 10 s of warm-up: about 5 minutes.
  * engine/test/hotpath (HotPath_Test::cpuPerOperation): CPU ns per universe
    frame, fader write, scene frame, script map and input flush.
  * The mixed rig (stresstest/engine/showbuilder.cpp, ShowSpec::MixedRig:
    moving heads with 16-bit pan/tilt, LED pars, pixel bars and strips,
    dimmers, laid out across a stage with groups over it): the realistic
    load on 32 universes, pan/tilt EFX on the movers, matrices over the
    stage groups (40 s each, as above); and HotPath_Test::cpuPerRigOperation
    ("hotpath-rig"): a rig frame, pan/tilt EFX, a stage-wide matrix, the
    power estimate and a capped universe, a Show Tree of 200 tracking cues
    (GO under load, an edit, resolving what the last cue plays).
  * ui/test/renderperf (RenderPerf_Test::cpuPerFrame, "gui"): drawing the
    rig offscreen -- process CPU per frame of the 3D/rig view (angled with
    beams, plan), the 2D plot, and an idle live tick. Its idle-repaint
    counts (nothing drawn on an idle show) are gate tests.
  * webaccess/test (WebAccess_Test::cpuPerBroadcast, "web"): one change
    sent to 50 WebSocket clients on loopback (the gate checks every client
    gets every change).

What it records per point: tick p50/p99 (the timer's compute time), the
timer thread's CPU per tick, the frame interval p99, process CPU and peak
RSS; over the a1p points, MB of RSS per universe.

Same-machine comparisons only. The baseline is docs/perf-data/
baseline-<host class>.csv; the host class is a name for the machine that
never includes its hostname: --host-class, else $QLC_PERF_HOST, else the
first line of ~/.config/qlcconsole/perf-host, else one derived from the CPU
model and core count ("macos-apple-m4-max-10p4e"). A missing baseline
switches to record mode.

Tolerances come from measured noise: recording runs every point --runs
times (3), keeps the median, and sets each metric's tolerance to
max(its floor, 2 x the spread seen). A check fails a metric only when it is
worse by more than the tolerance AND by more than the metric's absolute
floor; a point with a failing metric is run once more and only fails if
the rerun fails too.

A check writes <build dir>/guard-result.txt ("perf=N host=... rev=...
sha=... date=..."): a release needs perf=0 on its own commit (publish.sh).

Exit: 0 pass (or recorded), 1 regression, 2 build or run error, 3 the
machine is too busy to measure (--force to measure anyway).

Between 08:00 and 22:00 it runs at nice 19 (the lowest priority; --no-nice
to override), like the soaks, so a check never fights a working machine.
"""
import argparse
import csv
import datetime
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import types

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "stresstest"))

DATA = os.path.join(REPO, "docs", "perf-data")

# ---------------------------------------------------------------------------
# What runs
# ---------------------------------------------------------------------------


def all_point_names():
    return [p[0] for p in points()] + [m[0] for m in MICROS]


def points():
    """(name, config, start types) in run order: stable names, they key the
    baseline. The configurations are perf_sweep.py's own."""
    import perf_sweep
    out = []
    for u in (8, 32, 128, 256):
        out.append(("a1p-u%04d" % u, dict(perf_sweep.mixed(u), perstep=True), None))
    base = dict(universes=32, fpu=170, scenes=64, chasers=0, matrices=0, efx=0, cuelists=0,
                collections=0, scene_fix=170, perstep=True)
    out.append(("cp-chasers-n0064", dict(base, chasers=64, scenes=256), "chaser"))
    out.append(("c-matrix-n0064", dict(base, matrices=64, perstep=False), "matrix"))
    out.append(("c-efx-n0256", dict(base, efx=256, perstep=False), "efx"))
    # The mixed rig (added 2026-10-29: the points above are all Generic RGB)
    out.append(("rig-mixed-u0032", perf_sweep.mixed_rig(32), None))
    rbase = dict(universes=32, fpu=0, scenes=32, chasers=0, matrices=0, efx=0, cuelists=0,
                 collections=0, scene_fix=0, rig="mixed", perstep=True)
    out.append(("rig-efx-pantilt-n0256", dict(rbase, efx=256), "efx"))
    out.append(("rig-matrix-stage-n0064", dict(rbase, matrices=64), "matrix"))
    return out


# The report-only CPU-per-operation programs: (point, test dir in the build,
# program, test function). Each point's metrics are "<name>_ns".
MICROS = [
    ("hotpath", os.path.join("engine", "test", "hotpath"), "hotpath_test", "cpuPerOperation"),
    ("hotpath-rig", os.path.join("engine", "test", "hotpath"), "hotpath_test", "cpuPerRigOperation"),
    ("gui", os.path.join("ui", "test", "renderperf"), "renderperf_test", "cpuPerFrame"),
    ("web", os.path.join("webaccess", "test"), "webaccess_test", "cpuPerBroadcast"),
]


# metric -> (unit, relative floor, absolute floor). Every metric is "lower is
# better". The floors are the least change that counts whatever the noise:
# below them a difference is not a regression.
METRICS = {
    "tick_p50_ms": ("ms", 0.15, 0.05),
    "tick_p99_ms": ("ms", 0.25, 0.5),
    "tick_cpu_ms": ("ms", 0.15, 0.05),
    "frame_p99_ms": ("ms", 0.08, 2.0),
    "cpu_pct": ("%", 0.15, 5.0),
    "rss_mb": ("MB", 0.10, 8.0),
    "rss_mb_per_universe": ("MB", 0.10, 0.15),
    "ns": ("ns", 0.15, 0.0),
}
NOISE_K = 2.0       # tolerance = max(relative floor, NOISE_K x the recorded spread)


def metric_kind(metric):
    return "ns" if metric.endswith("_ns") else metric


def metrics_of(result):
    """qlcstress's RESULT JSON -> the guard's metrics for one point."""
    return {
        "tick_p50_ms": result["compute"]["p50"],
        "tick_p99_ms": result["compute"]["p99"],
        "tick_cpu_ms": result["tickCpu"]["mean"],
        "frame_p99_ms": result["gap"]["p99"],
        "cpu_pct": result["cpuPct"],
        "rss_mb": result["rssPeak"],
    }


def derived(rows):
    """Metrics across points: RSS per universe over the a1p points."""
    a1p = {}
    for (point, metric), v in rows.items():
        if point.startswith("a1p-u") and metric == "rss_mb":
            a1p[int(point[5:])] = v
    out = {}
    if len(a1p) >= 2:
        lo, hi = min(a1p), max(a1p)
        out[("a1p", "rss_mb_per_universe")] = (a1p[hi] - a1p[lo]) / (hi - lo)
    return out

# ---------------------------------------------------------------------------
# The machine
# ---------------------------------------------------------------------------


def slug(s):
    s = re.sub(r"\((r|tm)\)|cpu|@.*$|processor", " ", s.lower())
    return re.sub(r"[^a-z0-9]+", "-", s).strip("-")


def sh(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def cpu_model():
    if sys.platform == "darwin":
        return sh(["sysctl", "-n", "machdep.cpu.brand_string"]) or platform.machine()
    try:
        with open("/proc/device-tree/model") as f:              # a Raspberry Pi
            return f.read().strip("\0\n ")
    except OSError:
        pass
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.lower().startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def derived_host_class():
    """A name from the hardware alone: OS, CPU model, cores (macOS: perf+eff)."""
    osname = {"darwin": "macos", "linux": "linux", "win32": "windows"}.get(sys.platform, sys.platform)
    cores = str(os.cpu_count() or 0) + "t"
    if sys.platform == "darwin":
        p, e = sh(["sysctl", "-n", "hw.perflevel0.logicalcpu"]), sh(["sysctl", "-n", "hw.perflevel1.logicalcpu"])
        if p:
            cores = "%sp%se" % (p, e or "0")
    return "%s-%s-%s" % (osname, slug(cpu_model()), cores)


def host_class(arg):
    name = arg or os.environ.get("QLC_PERF_HOST", "")
    if not name:
        try:
            with open(os.path.expanduser("~/.config/qlcconsole/perf-host")) as f:
                name = f.readline().strip()
        except OSError:
            pass
    name = slug(name) if name else derived_host_class()
    return name


def busy_pct(seconds=2.0):
    """How much of the machine others use right now, 0-100 (None: unknown)."""
    if sys.platform.startswith("linux"):
        def snap():
            with open("/proc/stat") as f:
                v = [int(x) for x in f.readline().split()[1:]]
            return sum(v), v[3] + (v[4] if len(v) > 4 else 0)
        try:
            t0, i0 = snap()
            time.sleep(seconds)
            t1, i1 = snap()
            return round(100.0 * (1 - (i1 - i0) / max(1, t1 - t0)), 1)
        except (OSError, ValueError):
            return None
    if sys.platform == "darwin":
        out = sh(["top", "-l", "2", "-n", "0", "-s", str(int(max(1, seconds)))])
        idle = re.findall(r"([\d.]+)% idle", out)
        if idle:
            return round(100.0 - float(idle[-1]), 1)
    return None


def qt_version(prefix):
    for name in ("Qt6ConfigVersionImpl.cmake", "Qt6ConfigVersion.cmake"):
        for base in ([prefix] if prefix else []) + ["/usr", "/usr/lib/x86_64-linux-gnu", "/usr/lib/aarch64-linux-gnu"]:
            for sub in ("lib/cmake/Qt6", "cmake/Qt6"):
                p = os.path.join(base, sub, name)
                if os.path.exists(p):
                    m = re.search(r'PACKAGE_VERSION "([\d.]+)"', open(p).read())
                    if m:
                        return m.group(1)
    return "unknown"

# ---------------------------------------------------------------------------
# Baselines
# ---------------------------------------------------------------------------

FIELDS = ["point", "metric", "unit", "value", "runs", "spread_pct", "tolerance_pct", "floor"]


def tolerance(metric, values):
    """(median, spread %, tolerance %) of a metric's recorded runs."""
    med = statistics.median(values)
    spread = (max(values) - min(values)) / med if med > 0 else 0.0
    rel_floor = METRICS[metric_kind(metric)][1]
    return med, 100.0 * spread, 100.0 * max(rel_floor, NOISE_K * spread)


def write_baseline(path, meta, runs, keep=None, notes=()):
    """runs: a list of {(point, metric): value}, one per recorded run.
    keep: rows to carry over unchanged ({(point, metric): csv row dict}, as
    read_rows() gives them) -- adding points leaves the others as they were
    recorded. notes: extra "# added: ..." lines."""
    new = {}
    for key in sorted(set().union(*[set(r) for r in runs])):
        vals = [r[key] for r in runs if key in r]
        med, spread, tol = tolerance(key[1], vals)
        unit, _, floor = METRICS[metric_kind(key[1])]
        new[key] = dict(point=key[0], metric=key[1], unit=unit, value=round(med, 4),
                        runs=len(vals), spread_pct=round(spread, 1),
                        tolerance_pct=round(tol, 1), floor=floor)
    rows = dict(keep or {})
    rows.update(new)
    with open(path, "w", newline="") as f:
        f.write("# qlcconsole performance baseline: tools/perf/guard.py (docs/performance.md,\n")
        f.write("# \"Keeping it fast\"). Rebase only on purpose, with the reason in the commit.\n")
        for k in ("host_class", "recorded", "rev", "qt", "cpu", "os", "runs", "seconds", "busy_pct"):
            f.write("# %s: %s\n" % (k, meta.get(k, "")))
        for n in notes:
            f.write("# added: %s\n" % n)
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        for key in sorted(rows, key=row_order):
            w.writerow(rows[key])


def row_order(key):
    """Baseline rows in run order (points(), then MICROS), metrics sorted."""
    order = {name: i for i, (name, _, _) in enumerate(points())}
    for j, m in enumerate(MICROS):
        order[m[0]] = 1000 + j
    return (order.get(key[0], 2000), key[0], key[1])


def read_rows(path):
    """(the header's raw lines, {(point, metric): raw csv row}), to rewrite
    a baseline with points added and the rest exactly as they were."""
    head, lines = [], []
    with open(path) as f:
        for line in f:
            (head if line.startswith("#") else lines).append(line)
    return head, {(r["point"], r["metric"]): r for r in csv.DictReader(lines)}


def read_baseline(path):
    meta, rows = {}, {}
    with open(path) as f:
        lines = []
        for line in f:
            if line.startswith("#"):
                m = re.match(r"#\s*(\w+):\s*(.*)$", line.rstrip("\n"))
                if m:
                    meta[m.group(1)] = m.group(2)
            else:
                lines.append(line)
    for r in csv.DictReader(lines):
        rows[(r["point"], r["metric"])] = dict(value=float(r["value"]),
                                               tolerance_pct=float(r["tolerance_pct"]),
                                               floor=float(r["floor"]), unit=r["unit"],
                                               spread_pct=float(r["spread_pct"]))
    return meta, rows


def judge(base, value):
    """'regressed', 'improved' or 'ok' for one metric against its baseline row."""
    b, tol, floor = base["value"], base["tolerance_pct"] / 100.0, base["floor"]
    diff = value - b
    if diff > b * tol and diff > floor:
        return "regressed"
    if -diff > b * tol and -diff > floor:
        return "improved"
    return "ok"


def compare(baseline, current):
    """[(point, metric, base row, value, verdict)] for every baselined metric."""
    out = []
    for key in sorted(baseline):
        if key in current:
            out.append((key[0], key[1], baseline[key], current[key], judge(baseline[key], current[key])))
        else:
            out.append((key[0], key[1], baseline[key], None, "missing"))
    return out

# ---------------------------------------------------------------------------
# Building and running
# ---------------------------------------------------------------------------


def log(msg):
    print(time.strftime("%H:%M:%S ") + msg, flush=True)


def jobs():
    if sys.platform == "darwin":
        p = sh(["sysctl", "-n", "hw.perflevel0.logicalcpu"])
        if p:
            return int(p)
    return max(1, (os.cpu_count() or 2) - 1)


def qt_prefix(arg):
    if arg:
        return arg
    if os.environ.get("QT6_PREFIX"):
        return os.environ["QT6_PREFIX"]
    if shutil.which("brew"):
        p = sh(["brew", "--prefix", "qt6"]) or sh(["brew", "--prefix", "qt"])
        if p and os.path.isdir(p):
            return p
    return ""


def run(cmd, logf, **kw):
    log("$ " + " ".join(cmd))
    with open(logf, "a") as f:
        rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, **kw).returncode
    if rc != 0:
        print(open(logf).read()[-3000:], file=sys.stderr)
        raise SystemExit("guard: build step failed (%s), log %s" % (cmd[0], logf))


def build(a, qt, logdir):
    bdir = a.build_dir
    logf = os.path.join(logdir, "build.log")
    prefix = ["-DCMAKE_PREFIX_PATH=%s/lib/cmake" % qt] if qt else []
    launcher = []
    if shutil.which("ccache"):
        launcher = ["-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
                    "-DCMAKE_AUTOMOC_PATH_PREFIX=ON"]
        os.environ.setdefault("CCACHE_BASEDIR", REPO)
        os.environ.setdefault("CCACHE_NOHASHDIR", "1")
    cache = os.path.join(bdir, "CMakeCache.txt")
    if not os.path.exists(cache):
        run(["cmake", "-S", REPO, "-B", bdir, "-DCMAKE_BUILD_TYPE=Release"] + prefix + launcher, logf)
    btype = re.search(r"^CMAKE_BUILD_TYPE:\w+=(\w*)", open(cache).read(), re.M)
    if not btype or btype.group(1) != "Release":
        raise SystemExit("guard: %s is not a Release build (%s); remove it or pass --build-dir"
                         % (bdir, btype.group(1) if btype else "?"))
    run(["cmake", "--build", bdir, "-j%d" % jobs(), "--target", "qlcplusengine",
         *sorted({m[2] for m in MICROS})], logf)
    sdir = os.path.join(bdir, "qlcstress")
    if not os.path.exists(os.path.join(sdir, "CMakeCache.txt")):
        run(["cmake", "-S", os.path.join(REPO, "stresstest", "engine"), "-B", sdir,
             "-DCMAKE_BUILD_TYPE=Release", "-DQLC_BUILD=" + os.path.abspath(bdir)] + prefix + launcher, logf)
    run(["cmake", "--build", sdir, "-j%d" % jobs()], logf)
    exe = os.path.join(sdir, "qlcstress")
    if not os.path.exists(exe):
        exe += ".exe"
    return exe


def child_env(bdir):
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    lib = os.pathsep.join(os.path.join(os.path.abspath(bdir), d, "src") for d in ("engine", "ui", "webaccess"))
    for k in ("LD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH"):
        env[k] = lib + (os.pathsep + env[k] if env.get(k) else "")
    return env


def run_point(a, qlcstress, name, cfg, start, outdir):
    import perf_sweep
    ns = types.SimpleNamespace(qlcstress=qlcstress, seconds=a.seconds, warmup=a.warmup,
                               resources=os.path.join(REPO, "resources"), max_sec=a.seconds * 5 + 120,
                               tick_csv=False)
    os.makedirs(outdir, exist_ok=True)
    # perf_sweep.run_one skips a point whose JSON exists: each run its own dir.
    res, rc, wall, _ = perf_sweep.run_one(ns, "guard", name, cfg, start, outdir)
    if res is None:
        raise RuntimeError("%s: qlcstress failed (rc %s), see %s" % (name, rc,
                           os.path.join(outdir, "guard", name + ".log")))
    return metrics_of(res)


def micro_exe(tdir, prog):
    """The test program in its build dir (a macOS GUI test is a bundle)."""
    for p in (os.path.join(tdir, prog + ".app", "Contents", "MacOS", prog), os.path.join(tdir, prog),
              os.path.join(tdir, prog + ".exe")):
        if os.path.exists(p):
            return p
    return os.path.join(tdir, prog)


def run_micro(a, outdir, point="hotpath"):
    _, sub, prog, func = next(m for m in MICROS if m[0] == point)
    tdir = os.path.join(a.build_dir, sub)
    exe = micro_exe(tdir, prog)
    report = os.path.join(outdir, point + ".csv")
    if os.path.exists(report):
        os.remove(report)
    env = child_env(a.build_dir)
    env.update(QLC_PERF_REPORT=report, QLC_PERF_REPS=str(a.micro_reps))
    os.makedirs(outdir, exist_ok=True)   # a rerun of the hot paths alone runs no point to make it
    logf = os.path.join(outdir, point + ".log")
    with open(logf, "w") as f:
        rc = subprocess.run([exe, func], cwd=tdir, env=env, stdout=f,
                            stderr=subprocess.STDOUT).returncode
    if rc != 0 or not os.path.exists(report):
        raise RuntimeError("%s %s failed (rc %s), see %s" % (prog, func, rc, logf))
    out = {}
    for line in open(report):
        name, v = line.strip().split(",")
        out[(point, name + "_ns")] = float(v)
    return out


def run_all(a, qlcstress, outdir, only=None):
    """One pass: {(point, metric): value}."""
    rows = {}
    for name, cfg, start in points():
        if only is not None and name not in only:
            continue
        log("point %-18s %d universes" % (name, cfg["universes"]))
        for m, v in run_point(a, qlcstress, name, cfg, start, outdir).items():
            rows[(name, m)] = v
    for point, sub, prog, func in MICROS:
        if only is None or point in only:
            log("%-24s (%s %s)" % (point, prog, func))
            rows.update(run_micro(a, outdir, point))
    rows.update(derived(rows))
    return rows


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------


def fmt(v):
    if v is None:
        return "-"
    if abs(v) >= 1000:
        return "%.0f" % v
    if abs(v) >= 100:
        return "%.1f" % v
    return "%.2f" % v


def table(results):
    lines = ["%-18s %-26s %10s %10s %8s %7s  %s" % ("point", "metric", "baseline", "now", "change",
                                                     "tol", "verdict")]
    for point, metric, base, value, verdict in results:
        change = "-" if value is None or base["value"] == 0 else \
            "%+.0f%%" % (100.0 * (value - base["value"]) / base["value"])
        mark = {"regressed": "REGRESSED", "improved": "faster", "ok": "", "missing": "MISSING"}[verdict]
        lines.append("%-18s %-26s %10s %10s %8s %6.0f%%  %s" % (point, metric, fmt(base["value"]), fmt(value),
                                                                 change, base["tolerance_pct"], mark))
    return "\n".join(lines)


def append_trend(path, meta, results):
    """One row per metric: docs/perf-data/trend.csv's format (see FORMAT
    there); the history lives on the host that runs the nightly."""
    new = not os.path.exists(path)
    with open(path, "a", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        if new:
            w.writerow(["date", "rev", "host_class", "qt", "point", "metric", "value", "baseline", "verdict"])
        for point, metric, base, value, verdict in results:
            w.writerow([meta["recorded"], meta["rev"], meta["host_class"], meta["qt"], point, metric,
                        "" if value is None else round(value, 4), base["value"], verdict])

# ---------------------------------------------------------------------------
# Self-test (the gate runs this; no build, no measuring)
# ---------------------------------------------------------------------------


def self_test():
    ok = True

    def check(cond, what):
        nonlocal ok
        if not cond:
            ok = False
            print("FAIL:", what)

    # Tolerance: the floor for a quiet metric, twice the spread for a noisy one.
    med, spread, tol = tolerance("tick_p50_ms", [1.0, 1.01, 0.99])
    check(med == 1.0 and abs(spread - 2.0) < 1e-9 and abs(tol - 15.0) < 1e-9, "quiet tolerance %s" % tol)
    med, spread, tol = tolerance("tick_p99_ms", [4.0, 5.0, 6.0])
    check(med == 5.0 and abs(tol - 80.0) < 1e-9, "noisy tolerance %s" % tol)
    check(tolerance("hotpath_x_ns", [100, 101, 102])[2] == 15.0, "ns metrics use the ns floor")
    # Judging: needs both the relative tolerance and the absolute floor.
    row = dict(value=2.0, tolerance_pct=25.0, floor=0.5)
    check(judge(row, 2.4) == "ok", "inside tolerance")
    check(judge(row, 2.6) == "regressed", "past tolerance and floor")
    check(judge(dict(value=0.4, tolerance_pct=25.0, floor=0.5), 0.8) == "ok", "under the absolute floor")
    check(judge(row, 1.4) == "improved", "faster")
    # A baseline survives a write/read round trip, metadata included.
    d = tempfile.mkdtemp()
    try:
        p = os.path.join(d, "baseline-x.csv")
        runs = [{("a1p-u0008", "tick_p99_ms"): v, ("hotpath", "scene_frame_170_ns"): 10000 + v}
                for v in (1.0, 1.2, 1.1)]
        write_baseline(p, dict(host_class="x", rev="abc", runs=3), runs)
        meta, rows = read_baseline(p)
        check(meta.get("host_class") == "x" and meta.get("rev") == "abc", "metadata %s" % meta)
        r = rows[("a1p-u0008", "tick_p99_ms")]
        check(abs(r["value"] - 1.1) < 1e-9 and r["floor"] == 0.5 and r["unit"] == "ms", "row %s" % r)
        check(abs(r["tolerance_pct"] - 36.4) < 0.05, "tolerance from spread %s" % r["tolerance_pct"])
        res = compare(rows, {("a1p-u0008", "tick_p99_ms"): 9.0})
        check([x[4] for x in res] == ["regressed", "missing"], "compare %s" % [x[4] for x in res])
        t = os.path.join(d, "trend.csv")
        append_trend(t, dict(recorded="2026-10-29T03:00", rev="abc", host_class="x", qt="6"), res)
        check(len(open(t).read().splitlines()) == 3, "trend rows")
    finally:
        shutil.rmtree(d)
    # Derived: RSS per universe over the a1p points.
    dv = derived({("a1p-u0008", "rss_mb"): 20.0, ("a1p-u0128", "rss_mb"): 200.0})
    check(abs(dv[("a1p", "rss_mb_per_universe")] - 1.5) < 1e-9, "rss per universe %s" % dv)
    # A host class is a slug, never empty, never the hostname.
    check(slug("Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz") == "intel-core-i7-9750h", slug("Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz"))
    hc = derived_host_class()
    node = slug(platform.node().split(".")[0])
    check(bool(hc) and (not node or node not in hc), "host class %s names the host" % hc)
    check(host_class("My Box!") == "my-box", "named host class")
    # Adding points keeps every other row exactly as it was recorded.
    d = tempfile.mkdtemp()
    try:
        p = os.path.join(d, "baseline-x.csv")
        write_baseline(p, dict(host_class="x", rev="old", runs=3),
                       [{("a1p-u0008", "tick_p99_ms"): v, ("hotpath", "scene_frame_170_ns"): 10000 + v}
                        for v in (1.0, 1.2, 1.1)])
        before = open(p).read().splitlines()
        head, kept = read_rows(p)
        kept = {k: v for k, v in kept.items() if k[0] != "gui"}
        meta, _ = read_baseline(p)
        write_baseline(p, meta, [{("gui", "gui_x_ns"): v} for v in (5.0, 5.0, 5.0)], keep=kept,
                       notes=["gui -- now"])
        after = open(p).read().splitlines()
        check(all(line in after for line in before), "kept rows changed: %s" % after)
        check("# added: gui -- now" in after and any(line.startswith("gui,gui_x_ns,ns,5.0,") for line in after),
              "added rows %s" % after)
        meta2, rows2 = read_baseline(p)
        check(meta2.get("rev") == "old" and len(rows2) == 3, "after adding %s %s" % (meta2, rows2))
    finally:
        shutil.rmtree(d)
    # Every point has a known metric set.
    names = all_point_names()
    check(len(names) == len(set(names)) and all(re.match(r"^[a-z0-9-]+$", n) for n in names), names)
    check({"rig-mixed-u0032", "hotpath-rig", "gui", "web"} <= set(names), "the mixed rig and gui points")
    print("guard self-test:", "ok" if ok else "FAILED")
    return 0 if ok else 1

# ---------------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--record", action="store_true", help="record (or rebase) this host's baseline")
    ap.add_argument("--runs", type=int, default=3, help="runs to record (default 3; noise needs 3+)")
    ap.add_argument("--host-class", default="", help="this machine's baseline name (see above)")
    ap.add_argument("--baseline", default="", help="baseline file (default docs/perf-data/baseline-<class>.csv)")
    ap.add_argument("--build-dir", default=os.path.join(REPO, "build-perf"))
    ap.add_argument("--qt", default="", help="Qt prefix (default $QT6_PREFIX, brew's qt6, the system's)")
    ap.add_argument("--no-build", action="store_true", help="use build-dir as it is")
    ap.add_argument("--seconds", type=int, default=40, help="measured seconds per point")
    ap.add_argument("--warmup", type=int, default=10, help="warm-up seconds per point")
    ap.add_argument("--points", default="", help="check only these points, comma-separated "
                    "(a1p-u0128,hotpath,...): a quick before/after while working on a hot path; "
                    "with --record, add (or re-record) only these points in the baseline")
    ap.add_argument("--micro-reps", type=int, default=10, help="QLC_PERF_REPS for the hot-path test")
    ap.add_argument("--out", default="", help="results folder (default a temporary one)")
    ap.add_argument("--trend", default="", help="append the comparison to this trend CSV")
    ap.add_argument("--rev", default="", help="the revision measured (default: git HEAD)")
    ap.add_argument("--force", action="store_true", help="measure even on a busy machine")
    ap.add_argument("--busy-limit", type=float, default=35.0, help="%% of the CPU others may use (35)")
    ap.add_argument("--no-nice", action="store_true", help="full priority in working hours too")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()

    hour = datetime.datetime.now().hour
    if not a.no_nice and 8 <= hour < 22 and hasattr(os, "nice"):
        # Already niced (the daytime rule: `nice -n 20 make perf`): os.nice()
        # adds to the current value, and can fail or be refused, so only
        # raise it when it is below 19 and carry on if it can't be.
        try:
            cur = os.getpriority(os.PRIO_PROCESS, 0)
        except (AttributeError, OSError):
            cur = 0
        if cur < 19:
            try:
                os.nice(19 - cur)
            except OSError:
                pass
        try:
            log("working hours: nice %d" % os.getpriority(os.PRIO_PROCESS, 0))
        except (AttributeError, OSError):
            log("working hours: nice 19")

    hc = host_class(a.host_class)
    bpath = a.baseline or os.path.join(DATA, "baseline-%s.csv" % hc)
    record = a.record or not os.path.exists(bpath)
    if not a.record and record:
        log("no baseline for host class '%s' (%s): recording one (%d runs)" % (hc, os.path.relpath(bpath, REPO), a.runs))
    outdir = a.out or tempfile.mkdtemp(prefix="qlc-perf-")
    os.makedirs(outdir, exist_ok=True)
    qt = qt_prefix(a.qt)
    rev = a.rev or sh(["git", "-C", REPO, "rev-parse", "--short=12", "HEAD"]) or "unknown"
    if not a.rev and sh(["git", "-C", REPO, "status", "--porcelain", "--untracked-files=no"]):
        rev += "-dirty"

    # qlcstress and the test: offscreen, the engine library of this build
    os.environ.update(child_env(a.build_dir))
    t0 = time.time()
    try:
        qlcstress = os.path.join(a.build_dir, "qlcstress", "qlcstress") if a.no_build else build(a, qt, outdir)
    except SystemExit as e:
        print(e, file=sys.stderr)
        return 2
    built = time.time() - t0

    busy = busy_pct()
    log("host class %s, Qt %s, rev %s, others use %s%% of the CPU" % (hc, qt_version(qt), rev,
                                                                       "?" if busy is None else busy))
    if busy is not None and busy > a.busy_limit and not a.force:
        print("guard: the machine is busy (%.0f%% of the CPU in use, limit %.0f%%): the numbers would "
              "measure that, not this build. Try later, or --force." % (busy, a.busy_limit))
        return 3

    meta = dict(host_class=hc, recorded=datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%MZ"), rev=rev,
                qt=qt_version(qt), cpu=cpu_model(), os="%s %s" % (platform.system(), platform.release()),
                seconds="%d+%d" % (a.seconds, a.warmup), busy_pct=busy)
    try:
        only = set(a.points.split(",")) if a.points else None
        if only is not None:
            unknown = only - set(all_point_names())
            if unknown:
                print("guard: no such point: %s (points: %s)" % (", ".join(sorted(unknown)),
                      ", ".join(all_point_names())), file=sys.stderr)
                return 2
        if record and only is not None and not os.path.exists(bpath):
            print("guard: --record --points adds points to a baseline, and there is none yet: "
                  "record every point first (--record)", file=sys.stderr)
            return 2
        if record:
            runs = []
            for i in range(a.runs):
                log("record run %d of %d" % (i + 1, a.runs))
                runs.append(run_all(a, qlcstress, os.path.join(outdir, "run%d" % (i + 1)), only=only))
            os.makedirs(os.path.dirname(bpath), exist_ok=True)
            if only is not None:
                # Add (or re-record) these points; every other row, and the
                # header it was recorded with, stays as it was.
                head, kept = read_rows(bpath)
                kept = {k: v for k, v in kept.items() if k[0] not in only}
                bmeta, _ = read_baseline(bpath)
                notes = [m.group(1) for m in (re.match(r"#\s*added:\s*(.*)$", h.rstrip("\n")) for h in head) if m]
                notes.append("%s -- %s, rev %s, Qt %s, %d runs" % (",".join(sorted(only)), meta["recorded"],
                                                                     rev, meta["qt"], a.runs))
                write_baseline(bpath, bmeta, runs, keep=kept, notes=notes)
            else:
                meta["runs"] = a.runs
                write_baseline(bpath, meta, runs)
            _, rows = read_baseline(bpath)
            print("\nRecorded %s (%d runs, %.0f min). Spread and tolerance per metric:" % (
                os.path.relpath(bpath, REPO), a.runs, (time.time() - t0) / 60))
            for (p, m), r in sorted(rows.items()):
                print("  %-18s %-26s %10s %-2s  spread %5.1f%%  tolerance %5.1f%%" % (
                    p, m, fmt(r["value"]), r["unit"], r["spread_pct"], r["tolerance_pct"]))
            return 0

        bmeta, baseline = read_baseline(bpath)
        if bmeta.get("qt") and bmeta.get("qt") != meta["qt"]:
            log("NOTE: the baseline was recorded with Qt %s, this is Qt %s" % (bmeta.get("qt"), meta["qt"]))
        if only is not None:
            baseline = {k: v for k, v in baseline.items() if k[0] in only}
            if not baseline:
                print("guard: none of %s is in the baseline" % a.points, file=sys.stderr)
                return 2
        current = run_all(a, qlcstress, os.path.join(outdir, "run1"), only=only)
        results = compare(baseline, current)
        failing = sorted({p for p, m, b, v, verdict in results if verdict == "regressed"})
        if failing:
            # Noise passes; a regression stays. Rerun what failed, keep the better.
            log("rerunning %s to confirm" % ", ".join(failing))
            again = run_all(a, qlcstress, os.path.join(outdir, "rerun"),
                            only=set(failing) | ({"a1p-u%04d" % u for u in (8, 32, 128, 256)}
                                                  if "a1p" in failing else set()))
            for k, v in again.items():
                if k in current:
                    current[k] = min(current[k], v)
            results = compare(baseline, current)
        print()
        print("Performance guard: %s vs baseline %s (rev %s, %s)" % (rev, os.path.relpath(bpath, REPO),
              bmeta.get("rev", "?"), bmeta.get("recorded", "?")))
        print(table(results))
        unbased = sorted({p for (p, m) in current if (p, m) not in baseline and p != "a1p"})
        if unbased:
            print("\nNot in this baseline (measured, not judged): %s.\nAdd them on purpose: "
                  "make perf-record PERF_ARGS=--points=%s" % (", ".join(unbased), ",".join(unbased)))
        regressed = [r for r in results if r[4] in ("regressed", "missing")]
        improved = [r for r in results if r[4] == "improved"]
        mins = (time.time() - t0) / 60
        if a.trend:
            append_trend(a.trend, meta, results)
        # What a release checks for (docs/dev/RELEASE.md step 0, publish.sh)
        sha = sh(["git", "-C", REPO, "rev-parse", "HEAD"]) if not a.rev else a.rev
        with open(os.path.join(a.build_dir, "guard-result.txt"), "w") as f:
            f.write("perf=%d host=%s rev=%s sha=%s date=%s\n" % (1 if regressed else 0, hc, rev,
                    sha + ("-dirty" if rev.endswith("-dirty") else ""), meta["recorded"]))
        with open(os.path.join(outdir, "summary.json"), "w") as f:
            json.dump(dict(meta, regressed=len(regressed), improved=len(improved), minutes=round(mins, 1),
                           build_s=round(built)), f, indent=1)
        print()
        if regressed:
            print("perf=1 REGRESSED: %d metric(s) past tolerance on %s (%.0f min; results in %s)" % (
                len(regressed), hc, mins, outdir))
            print("A slowdown is a bug: find it, or if it is the price of a change worth having,")
            print("rebase on purpose (--record) with the reason in the commit.")
            return 1
        print("perf=0 within tolerance on %s: %d metrics%s (%.0f min)" % (
            hc, len(results), ", %d faster -- consider rebasing" % len(improved) if improved else "", mins))
        return 0
    except RuntimeError as e:
        print("guard:", e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
