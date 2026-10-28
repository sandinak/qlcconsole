#!/usr/bin/env python3
"""Summarise LeakSanitizer reports from m2leak runs of different lengths.

  lsan_parse.py OUT.json RUN_DIR:CYCLES [RUN_DIR:CYCLES ...]

Each RUN_DIR holds asan.* / ubsan.* log files (ASAN/UBSAN log_path) and
session.log (m2leak's stderr). For every run: total leaked bytes and
allocations (direct + indirect), a breakdown by known leak (L1-L3 of
docs/TEST_MATRIX.md, matched on stack frames) and by top in-project frame
for the rest; m2leak's per-cycle RSS and UI counters (menuChildActions: L4;
iolist: M1). Bytes per cycle = the slope between runs of different lengths.
"""
import glob
import json
import os
import re
import sys

KNOWN = [
    ("L1 Fixture::genericDimmerDef/Mode (QLCFixtureDef per generic dimmer)",
     re.compile(r"Fixture::genericDimmer(Def|Mode)")),
    ("L2 RGBAudio dtor creates the Doc's audio capture",
     re.compile(r"RGBAudio::~RGBAudio|Doc::audioInputCapture|AudioCapture")),
    ("L3 QLCFixtureDef::loadXML(path) ReadError path leaks the reader",
     re.compile(r"QLCFixtureDef::loadXML\(QString const&\)|QLCFile::getXMLReader")),
]
HEAD = re.compile(r"^(Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\) allocated from:")
FRAME = re.compile(r"^\s+#\d+ 0x[0-9a-f]+ in (.+?) (?:\S+:\d+(?::\d+)?|\(.+\))\s*$")
SUMMARY = re.compile(r"SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)")
CYCLE = re.compile(r"\[m2leak\] cycle (\d+) (\S+) load=(\d+) functions=(\d+) edits=(\d+) started=(\d+) modals=(\d+) rss ([\d.]+) MB")
UI = re.compile(r"\[m2leak\] ui after load\+close: (.*)")


def parse_run(d):
    leaks = []
    total_b = total_n = 0
    summaries = []
    for p in sorted(glob.glob(os.path.join(d, "asan*")) + glob.glob(os.path.join(d, "ubsan*"))
                    + [os.path.join(d, "session.log")]):
        if not os.path.isfile(p):
            continue
        cur = None
        for ln in open(p, errors="replace"):
            m = HEAD.match(ln)
            if m:
                cur = {"kind": m.group(1), "bytes": int(m.group(2)), "objs": int(m.group(3)), "frames": []}
                leaks.append(cur)
                continue
            if cur is not None:
                f = FRAME.match(ln)
                if f:
                    cur["frames"].append(f.group(1))
                    continue
                if ln.strip() == "":
                    cur = None
            s = SUMMARY.search(ln)
            if s:
                summaries.append((int(s.group(1)), int(s.group(2))))
    total_b = sum(x["bytes"] for x in leaks)
    total_n = sum(x["objs"] for x in leaks)
    by = {}
    for lk in leaks:
        stack = " | ".join(lk["frames"])
        tag = None
        for name, rx in KNOWN:
            if rx.search(stack):
                tag = name
                break
        if tag is None:
            proj = [f for f in lk["frames"] if not re.match(r"(operator new|malloc|calloc|realloc|Q[A-Z]\w*(<.*?>)?::|QTypedArrayData|QMap|QHash|QList|q\w+\(|__|std::|g_|_dl_|dlopen|QtPrivate|QtSharedPointer|connect<|_start|main$)", f)]
            tag = "other: " + (proj[0] if proj else (lk["frames"][1] if len(lk["frames"]) > 1 else "?"))[:140]
        b = by.setdefault(tag, {"bytes": 0, "objs": 0, "direct": 0, "indirect": 0})
        b["bytes"] += lk["bytes"]
        b["objs"] += lk["objs"]
        b["direct" if lk["kind"] == "Direct" else "indirect"] += lk["bytes"]
    cycles, ui = [], []
    sp = os.path.join(d, "session.log")
    if os.path.exists(sp):
        for ln in open(sp, errors="replace"):
            m = CYCLE.search(ln)
            if m:
                cycles.append({"cycle": int(m.group(1)), "show": m.group(2), "functions": int(m.group(4)),
                               "edits": int(m.group(5)), "started": int(m.group(6)), "rss_mb": float(m.group(8))})
            m = UI.search(ln)
            if m:
                ui.append(dict(kv.split("=") for kv in m.group(1).split()))
    return {"leaked_bytes": total_b, "leaked_allocs": total_n, "summaries": summaries,
            "by_source": dict(sorted(by.items(), key=lambda kv: -kv[1]["bytes"])),
            "cycles": cycles, "ui": ui}


def main():
    out = sys.argv[1]
    runs = {}
    for arg in sys.argv[2:]:
        d, n = arg.rsplit(":", 1)
        runs[int(n)] = parse_run(d)
        r = runs[int(n)]
        print("%3d cycles: %d bytes in %d allocations" % (int(n), r["leaked_bytes"], r["leaked_allocs"]))
    ns = sorted(runs)
    res = {"runs": runs}
    if len(ns) >= 2:
        xs = ns
        ys = [runs[n]["leaked_bytes"] for n in ns]
        mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
        d = sum((x - mx) ** 2 for x in xs)
        res["bytes_per_cycle_fit"] = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / d if d else None
        res["bytes_per_cycle_pairs"] = {"%d->%d" % (a, b): (runs[b]["leaked_bytes"] - runs[a]["leaked_bytes"]) / (b - a)
                                        for a, b in zip(ns, ns[1:])}
        per_src = {}
        for n in ns:
            for k, v in runs[n]["by_source"].items():
                per_src.setdefault(k, {})[n] = v["bytes"]
        res["by_source_bytes_per_cycle"] = {
            k: ((v.get(ns[-1], 0) - v.get(ns[0], 0)) / (ns[-1] - ns[0])) for k, v in per_src.items()}
        res["by_source_by_run"] = per_src
        print("bytes/cycle (fit): %.0f" % res["bytes_per_cycle_fit"])
        for k, v in sorted(res["by_source_bytes_per_cycle"].items(), key=lambda kv: -kv[1])[:15]:
            print("  %9.0f B/cycle  %s" % (v, k))
    json.dump(res, open(out, "w"), indent=1)


if __name__ == "__main__":
    main()
