#!/usr/bin/env python3
"""The performance history: docs/perf-data/history.csv, one row per metric per run.

  python3 tools/perf/history.py commit [--from FILE]   # commit history.csv (never pushes)
  python3 tools/perf/history.py merge FILE             # add FILE's rows not already in it
  python3 tools/perf/history.py --self-test            # the gate runs this (and plot.py trends)

tools/perf/guard.py appends here after every `make perf` and `make
perf-record` (the nightly run appends to its own file, see nightly.sh); the
trend charts (plot.py trends) and docs/performance.md, "Performance over
time", read it. Long format, so a new metric or scenario needs no new column:

  date,sha,host,machine,qt,kind,scenario,metric,value,baseline,verdict

  date      UTC, 2026-11-01T04:34Z
  sha       the revision measured (12 hex digits; "-dirty" when the tree was)
  host      the host class, as the baselines name it (macos-apple-m4-max-10p4e)
  machine   the machine's name (ender)
  qt        the Qt version measured with
  kind      perf | record | nightly (the guard); ab | bisect (short hand runs)
  scenario  a guard point (a1p-u0128, rig-mixed-u0032, hotpath, ...), or
            "all" for the run's own verdict (metric "perf": value 0 or 1)
  metric    tick_p50_ms, tick_p99_ms, tick_cpu_ms, cpu_pct, rss_mb,
            rss_mb_per_universe, *_ns
  value     what was measured
  baseline  the baseline's value for it (a record: the value itself)
  verdict   ok | faster | regressed | missing | record; "" for an unjudged run

Appending never fails a run: a bad write prints a warning and the run goes on.
"""
import csv
import datetime
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
HISTORY = os.path.join(REPO, "docs", "perf-data", "history.csv")
FIELDS = ["date", "sha", "host", "machine", "qt", "kind", "scenario", "metric", "value", "baseline", "verdict"]
KEY = ["date", "sha", "host", "kind", "scenario", "metric"]

# The metrics worth a history. The rest of what the guard measures
# (frame_p99_ms) stays in the run's own results.
KEY_METRICS = {"tick_p50_ms", "tick_p99_ms", "tick_cpu_ms", "cpu_pct", "rss_mb", "rss_mb_per_universe"}


def is_key(metric):
    return metric in KEY_METRICS or metric.endswith("_ns")


def fnum(v):
    if v is None or v == "":
        return ""
    s = "%.4f" % float(v)
    return s.rstrip("0").rstrip(".") if "." in s else s


def rows_for_run(meta, kind, results, perf_rc, machine=""):
    """results: [(point, metric, baseline row or None, value, verdict)] as the guard's
    compare() returns them; perf_rc: 0 within tolerance, 1 regressed."""
    base = dict(date=meta["recorded"], sha=meta["rev"], host=meta["host_class"], machine=machine,
                qt=meta.get("qt", ""), kind=kind)
    out = []
    for point, metric, b, value, verdict in results:
        if not is_key(metric) or value is None:
            continue
        out.append(dict(base, scenario=point, metric=metric, value=fnum(value),
                        baseline=fnum(b["value"]) if b else "", verdict=verdict))
    out.append(dict(base, scenario="all", metric="perf", value=str(perf_rc),
                    baseline="", verdict="regressed" if perf_rc else ("record" if kind == "record" else "ok")))
    return out


def rows_for_record(meta, rows, machine=""):
    """The baseline just recorded: {(point, metric): {"value": ...}}."""
    results = [(p, m, r, r["value"], "record") for (p, m), r in sorted(rows.items())]
    return rows_for_run(meta, "record", results, 0, machine)


def read(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def key_of(r):
    return tuple(r.get(k, "") for k in KEY)


def append(path, rows):
    """Add rows to the history. True if written; a failure only warns."""
    try:
        if not rows:
            return True
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        new = not os.path.exists(path) or os.path.getsize(path) == 0
        with open(path, "a", newline="") as f:
            w = csv.DictWriter(f, FIELDS, lineterminator="\n", extrasaction="ignore")
            if new:
                w.writeheader()
            w.writerows(rows)
        return True
    except Exception as e:     # never fail a run over its history
        print("history: could not append to %s: %s (the run is not affected)" % (path, e), file=sys.stderr)
        return False


def merge(dst, src):
    """Add the rows of src that dst lacks (same date/sha/host/kind/scenario/metric),
    keeping dst in date order. Returns how many were added."""
    have = read(dst) if os.path.exists(dst) else []
    seen = {key_of(r) for r in have}
    add = []
    for r in read(src):
        if key_of(r) not in seen:
            seen.add(key_of(r))
            add.append(r)
    if add:
        allrows = sorted(have + add, key=lambda r: r["date"])     # stable: file order inside a date
        with open(dst, "w", newline="") as f:
            w = csv.DictWriter(f, FIELDS, lineterminator="\n", extrasaction="ignore")
            w.writeheader()
            w.writerows(allrows)
    return len(add)


def git(*args):
    return subprocess.run(["git", "-C", REPO] + list(args), capture_output=True, text=True)


def commit(src=""):
    """make perf-history-commit: merge the nightly's file in (if any), then commit
    docs/perf-data/history.csv and nothing else. Never pushes: the train or the
    lead does."""
    src = src or os.path.join(os.environ.get("PERF_NIGHTLY", os.path.expanduser("~/git/perf-nightly")), "history.csv")
    if os.path.exists(src) and os.path.abspath(src) != os.path.abspath(HISTORY):
        n = merge(HISTORY, src)
        print("history: %d new row(s) from %s" % (n, src))
    rel = os.path.relpath(HISTORY, REPO)
    if not os.path.exists(HISTORY):
        print("history: no %s yet" % rel)
        return 0
    if not git("status", "--porcelain", "--", rel).stdout.strip():
        print("history: %s has nothing new to commit" % rel)
        return 0
    n = len(read(HISTORY))
    msg = "perf: history (%d rows)\n\nCo-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>\n" % n
    # A pathspec commit takes only that file, whatever else is staged.
    r = git("commit", "-q", "-m", msg, "--", rel)
    if r.returncode:
        print("history: git commit failed: %s" % (r.stderr or r.stdout).strip())
        return 1
    print("history: committed %s (%d rows); not pushed" % (rel, n))
    return 0


def self_test():
    ok = True

    def check(c, what):
        nonlocal ok
        if not c:
            print("FAIL history:", what)
            ok = False

    meta = dict(recorded="2026-11-01T04:34Z", rev="66da743e5e44", host_class="macos-x", qt="6.11.1")
    res = [("a1p", "rss_mb_per_universe", {"value": 1.4158}, 1.6334, "regressed"),
           ("a1p-u0008", "frame_p99_ms", {"value": 21.0}, 21.06, "ok"),          # not a key metric
           ("a1p-u0008", "tick_p50_ms", {"value": 0.05}, 0.05, "ok"),
           ("hotpath", "scene_frame_170_ns", {"value": 11151}, 10825.0, "ok"),
           ("web", "x_ns", {"value": 1}, None, "missing")]                       # no value: skipped
    rows = rows_for_run(meta, "perf", res, 1, "box")
    check(len(rows) == 4, "key metrics and the run verdict only: %d" % len(rows))
    check(rows[-1]["scenario"] == "all" and rows[-1]["verdict"] == "regressed", "overall row")
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "sub", "history.csv")
        check(append(p, rows), "append")
        check(append(p, rows_for_run(dict(meta, recorded="2026-11-02T04:34Z"), "nightly", res, 0, "box")), "append 2")
        back = read(p)
        check(len(back) == 8 and list(back[0]) == FIELDS, "header written once, long format")
        check(back[0]["value"] == "1.6334" and back[2]["value"] == "10825", "values round-trip: %s" % back[2]["value"])
        check({r["kind"] for r in back} == {"perf", "nightly"}, "kinds")
        # merge: only new rows, date order
        q = os.path.join(d, "other.csv")
        append(q, rows_for_run(dict(meta, recorded="2026-11-01T01:00Z"), "nightly", res, 0, "box"))
        append(q, rows[:2])
        check(merge(p, q) == 4, "merge adds the 4 new rows, not the 2 it has")
        check(merge(p, q) == 0, "merge is idempotent")
        check([r["date"] for r in read(p)] == sorted(r["date"] for r in read(p)), "merge keeps date order")
        # a bad write only warns
        import contextlib
        import io
        with contextlib.redirect_stderr(io.StringIO()) as err:
            check(append(os.path.join(p, "x", "history.csv"), rows) is False, "an unwritable path returns False, no raise")
        check("could not append" in err.getvalue(), "and warns")
        check(append(p, [{"date": object()}]) in (True, False), "odd rows do not raise")
    return 0 if ok else 1


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["--self-test"]:
        sys.exit(self_test())
    if a[:1] == ["merge"] and len(a) == 2:
        print("%d row(s) added" % merge(HISTORY, a[1]))
        sys.exit(0)
    if a[:1] == ["commit"]:
        sys.exit(commit(a[2] if a[1:2] == ["--from"] and len(a) > 2 else ""))
    print(__doc__)
    sys.exit(2)
