#!/usr/bin/env python3
"""Charts for docs/performance.md, as static SVG, from docs/perf-data/*.csv.

  python3 -I tools/perf/plot.py [DATA_DIR] [OUT_DIR]
  python3 -I tools/perf/plot.py trends [DATA_DIR] [OUT_DIR]   # history.csv -> trend-*.svg
  python3 -I tools/perf/plot.py --self-test                   # the trends on a fixture (the gate)

DATA_DIR defaults to docs/perf-data (made by tools/perf/extract.py), OUT_DIR
to docs/images/perf (trends: docs/perf-data/images). No dependencies: a small SVG writer is below. The
output is deterministic (no dates, fixed number formatting), so rerunning
on the same data gives byte-identical files.

Chart rules: one y axis per chart; a legend for two or more series plus a
direct label at each line's end; hosts always in the order Pi, Linux, M4 with
the same colour and marker shape (the colours are the first three slots of a
colour-blind-safe categorical palette, validated all-pairs on the light
surface, plus slot 4 for the M4 Max of the guard's trend; the marker shape
is a second cue); 2 px lines, 8 px markers; a
light surface with a hairline border, so the charts read the same on a light
or a dark page; text in neutral ink; log scales are labelled as such.
"""
import csv
import math
import os
import sys

HOSTS = ["pi", "linux", "m4"]
# The trend chart's hosts: the guard's (tools/perf/guard.py), slot 4 after the three
TREND_HOSTS = ["pi", "linux", "m4", "m4max"]
COLOR = {"pi": "#2a78d6", "linux": "#eb6834", "m4": "#1baf7a", "m4max": "#eda100"}
SHAPE = {"pi": "circle", "linux": "square", "m4": "triangle", "m4max": "diamond"}

SURFACE = "#fcfcfb"
BORDER = "#d9d8d2"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#6f6d68"
GRID = "#e9e8e2"
AXIS = "#c3c2b7"
REF = "#52514e"
FONT = "system-ui, -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif"

W, H = 760, 450
L, R, T, B = 70, 150, 112, 62     # plot margins: right leaves room for direct labels


def num(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def f(v):
    """Coordinates: fixed 1-decimal formatting keeps the files stable."""
    return ("%.1f" % v).rstrip("0").rstrip(".")


def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def label_num(v):
    if v >= 1000 and v == int(v):
        return "{:,}".format(int(v))
    if v == int(v):
        return str(int(v))
    return ("%.1f" % v).rstrip("0").rstrip(".")


class Chart:
    def __init__(self, title, subtitle, xlabel, ylabel, x, y, hosts):
        self.title, self.subtitle, self.xlabel, self.ylabel = title, subtitle, xlabel, ylabel
        self.x, self.y = x, y            # Axis objects
        self.hosts = hosts
        self.body = []
        self.ends = []                   # (host, y px, text) for direct labels
        self.notes = []
        self.top = T

    # plot area
    @property
    def px0(self):
        return L

    @property
    def px1(self):
        return W - R

    @property
    def py0(self):
        return H - B

    @property
    def py1(self):
        return self.top

    def X(self, v):
        return self.px0 + self.x.frac(v) * (self.px1 - self.px0)

    def Y(self, v):
        return self.py0 - self.y.frac(v) * (self.py0 - self.py1)

    def marker(self, host, cx, cy, hollow=False, tip=None, size=8, color=None):
        c = color or COLOR[host]
        fill = SURFACE if hollow else c
        r = size / 2
        t = "<title>%s</title>" % esc(tip) if tip else ""
        common = 'fill="%s" stroke="%s" stroke-width="%s"' % (fill, c if hollow else SURFACE, "2" if hollow else "1.5")
        if SHAPE[host] == "circle":
            return '<circle cx="%s" cy="%s" r="%s" %s>%s</circle>' % (f(cx), f(cy), f(r + 0.5), common, t)
        if SHAPE[host] == "square":
            return '<rect x="%s" y="%s" width="%s" height="%s" %s>%s</rect>' % (
                f(cx - r), f(cy - r), f(size), f(size), common, t)
        if SHAPE[host] == "diamond":
            d = r + 1.2
            pts = "%s,%s %s,%s %s,%s %s,%s" % (f(cx), f(cy - d), f(cx + d), f(cy), f(cx), f(cy + d), f(cx - d), f(cy))
            return '<polygon points="%s" %s>%s</polygon>' % (pts, common, t)
        h = size * 1.15
        pts = "%s,%s %s,%s %s,%s" % (f(cx), f(cy - h * 0.62), f(cx + r + 0.6), f(cy + h * 0.45),
                                     f(cx - r - 0.6), f(cy + h * 0.45))
        return '<polygon points="%s" %s>%s</polygon>' % (pts, common, t)

    def series(self, host, pts, end_text, tipfmt, hollow=lambda p: False, dash=None, mark=lambda p: True):
        """pts: [(x, y, row)] sorted by x; mark(p) picks the points that get a marker."""
        if not pts:
            return
        d = " ".join(("M" if i == 0 else "L") + "%s %s" % (f(self.X(x)), f(self.Y(y)))
                     for i, (x, y, _) in enumerate(pts))
        self.body.append('<path d="%s" fill="none" stroke="%s" stroke-width="2" stroke-linejoin="round"%s/>'
                         % (d, COLOR[host], ' stroke-dasharray="%s"' % dash if dash else ""))
        for x, y, row in pts:
            if not mark((x, y, row)) and (x, y, row) is not pts[-1]:
                continue
            self.body.append(self.marker(host, self.X(x), self.Y(y), hollow((x, y, row)), tipfmt(x, y, row)))
        lx, ly, _ = pts[-1]
        self.ends.append((host, self.X(lx), self.Y(ly), end_text))

    def hline(self, v, text, right=False):
        y = self.Y(v)
        self.body.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1.25" '
                         'stroke-dasharray="6 4"/>' % (f(self.px0), f(y), f(self.px1), f(y), REF))
        self.body.append('<text x="%s" y="%s" font-size="12" fill="%s" text-anchor="%s">%s</text>'
                         % (f(self.px1 - 6 if right else self.px0 + 6), f(y - 6), INK2,
                            "end" if right else "start", esc(text)))

    def render(self, path):
        o = []
        o.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                 'font-family="%s" role="img" aria-labelledby="t d">' % (W, H, W, H, FONT))
        o.append('<title id="t">%s</title>' % esc(self.title))
        o.append('<desc id="d">%s</desc>' % esc(self.subtitle))
        o.append('<rect x="0.5" y="0.5" width="%d" height="%d" rx="8" fill="%s" stroke="%s"/>'
                 % (W - 1, H - 1, SURFACE, BORDER))
        o.append('<text x="20" y="30" font-size="16" font-weight="600" fill="%s">%s</text>' % (INK, esc(self.title)))
        o.append('<text x="20" y="50" font-size="12.5" fill="%s">%s</text>' % (INK2, esc(self.subtitle)))
        # legend
        lx = 20
        for h in self.hosts:
            o.append('<line x1="%d" y1="70" x2="%d" y2="70" stroke="%s" stroke-width="2"/>' % (lx, lx + 22, COLOR[h]))
            o.append(self.marker(h, lx + 11, 70))
            o.append('<text x="%d" y="74.5" font-size="12.5" fill="%s">%s</text>' % (lx + 30, INK, esc(HOST_LABEL[h])))
            lx += 30 + 7.2 * len(HOST_LABEL[h]) + 26
        for i, n in enumerate(self.notes):
            o.append('<text x="20" y="%s" font-size="11.5" fill="%s">%s</text>' % (f(93 + 15 * i), MUTED, esc(n)))
        # grid + y ticks
        for v in self.y.ticks():
            y = self.Y(v)
            o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1"/>'
                     % (f(self.px0), f(y), f(self.px1), f(y), GRID))
            o.append('<text x="%s" y="%s" font-size="12" fill="%s" text-anchor="end">%s</text>'
                     % (f(self.px0 - 8), f(y + 4), INK2, esc(self.y.fmt(v))))
        for v in self.x.ticks():
            x = self.X(v)
            o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1"/>'
                     % (f(x), f(self.py0), f(x), f(self.py0 + 5), AXIS))
            o.append('<text x="%s" y="%s" font-size="12" fill="%s" text-anchor="middle">%s</text>'
                     % (f(x), f(self.py0 + 20), INK2, esc(self.x.fmt(v))))
        o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1"/>'
                 % (f(self.px0), f(self.py0), f(self.px1), f(self.py0), AXIS))
        o.append('<text x="%s" y="%s" font-size="12.5" fill="%s" text-anchor="middle">%s</text>'
                 % (f((self.px0 + self.px1) / 2), f(H - 18), INK2, esc(self.xlabel)))
        o.append('<text transform="translate(20 %s) rotate(-90)" font-size="12.5" fill="%s" text-anchor="middle">%s</text>'
                 % (f((self.py0 + self.py1) / 2), INK2, esc(self.ylabel)))
        o.extend(self.body)
        # direct labels at line ends, pushed apart vertically so they never overlap
        ends = sorted(self.ends, key=lambda e: e[2])
        ys = [e[2] for e in ends]
        for i in range(1, len(ys)):
            ys[i] = max(ys[i], ys[i - 1] + 16)
        over = ys[-1] - self.py0 if ys else 0
        if over > 0:
            ys = [y - over for y in ys]
            for i in range(len(ys) - 2, -1, -1):
                ys[i] = min(ys[i], ys[i + 1] - 16)
        for (h, x, y, text), ly in zip(ends, ys):
            tx = self.px1 + 14
            if abs(ly - y) > 1 or x < self.px1 - 1:
                o.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1"/>'
                         % (f(x + 7), f(y), f(tx - 3), f(ly), AXIS))
            o.append(self.marker(h, tx + 4, ly, size=7))
            o.append('<text x="%s" y="%s" font-size="12" fill="%s">%s</text>' % (f(tx + 13), f(ly + 4), INK, esc(text)))
        o.append("</svg>")
        with open(path, "w") as fh:
            fh.write("\n".join(o) + "\n")
        print("wrote", path)


class Lin:
    def __init__(self, lo, hi, step, fmt=label_num):
        self.lo, self.hi, self.step, self._fmt = lo, hi, step, fmt

    def frac(self, v):
        return (v - self.lo) / (self.hi - self.lo)

    def ticks(self):
        n = int(round((self.hi - self.lo) / self.step))
        return [self.lo + i * self.step for i in range(n + 1)]

    def fmt(self, v):
        return self._fmt(v)


class Days:
    """Dates (days since 2000-01-01) labelled as 'Oct 29'; about 6 ticks."""
    MONTHS = "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec".split()

    def __init__(self, lo, hi):
        self.lo, self.hi = lo, hi

    def frac(self, v):
        return (v - self.lo) / (self.hi - self.lo)

    def ticks(self):
        step = max(1, int(math.ceil((self.hi - self.lo) / 6.0)))
        return list(range(int(self.lo), int(self.hi) + 1, step))

    def fmt(self, v):
        import datetime
        d = datetime.date(2000, 1, 1) + datetime.timedelta(days=int(v))
        return "%s %d" % (self.MONTHS[d.month - 1], d.day)


class Log2:
    def __init__(self, lo, hi):
        self.lo, self.hi = lo, hi

    def frac(self, v):
        return (math.log2(v) - math.log2(self.lo)) / (math.log2(self.hi) - math.log2(self.lo))

    def ticks(self):
        out, v = [], self.lo
        while v <= self.hi:
            out.append(v)
            v *= 2
        return out

    def fmt(self, v):
        return label_num(v)


class Log10Zero:
    """Log10 axis with a separate '0' row below the lowest decade (zeros can't sit on a log scale)."""

    def __init__(self, lo, hi):
        self.lo, self.hi = lo, hi
        self.zero_gap = 0.09        # fraction of the axis kept for the zero row

    def frac(self, v):
        if v <= 0:
            return 0.0
        g = self.zero_gap
        return g + (1 - g) * (math.log10(v) - math.log10(self.lo)) / (math.log10(self.hi) - math.log10(self.lo))

    def ticks(self):
        out, v = [0], self.lo
        while v <= self.hi * 1.0001:
            out.append(v)
            v *= 10
        return out

    def fmt(self, v):
        return label_num(v)


def read(path):
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


HOST_LABEL = {}
SHORT = {"pi": "Pi 5", "linux": "i7", "m4": "M4", "m4max": "M4 Max"}


def by_host(rows):
    out = {h: [] for h in HOSTS}
    for r in rows:
        if r["host"] in out:
            out[r["host"]].append(r)
    return out


def main():
    if sys.argv[1:2] == ["--self-test"]:
        sys.exit(self_test())
    if sys.argv[1:2] == ["trends"]:
        here = os.path.dirname(os.path.abspath(__file__))
        data = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "..", "docs", "perf-data")
        out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(data, "images")
        classes = {}
        for r in read(os.path.join(data, "hosts.csv")):
            HOST_LABEL[r["host"]] = r["label"]
            if r.get("host_class"):
                classes[r["host_class"]] = r["host"]
        trends(data, out, classes)
        return
    here = os.path.dirname(os.path.abspath(__file__))
    data = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "docs", "perf-data")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "..", "docs", "images", "perf")
    os.makedirs(out, exist_ok=True)
    classes = {}
    for r in read(os.path.join(data, "hosts.csv")):
        HOST_LABEL[r["host"]] = r["label"]
        if r.get("host_class"):
            classes[r["host_class"]] = r["host"]

    scaling = by_host(read(os.path.join(data, "scaling.csv")))
    broken = lambda p: bool(p[2]["broken"])
    note_hollow = "Hollow marker: past the safe envelope"

    # a. tick p99 vs universes
    c = Chart("Timer tick p99 vs universes",
              "Realistic mixed load, all running at once. Each 50 Hz tick must finish inside 20 ms.",
              "Universes (log scale)", "Tick time, 99th percentile (ms)",
              Log2(1, 1024), Lin(0, 40, 5), HOSTS)
    c.notes = [note_hollow]
    c.hline(20, "20 ms tick budget")
    for h in HOSTS:
        pts = [(int(r["universes"]), num(r["tick_p99_ms"]), r) for r in scaling[h]]
        pts = [p for p in pts if p[1] is not None]
        ok = [p for p in pts if not p[2]["broken"]]
        last = ok[-1][0] if ok else None
        c.series(h, pts, "%s: %s ok" % (SHORT[h], label_num(last)) if last else SHORT[h],
                 lambda x, y, r, h=h: "%s, %d universes: tick p99 %s ms, frame interval p99 %s ms%s" % (
                     HOST_LABEL[h], x, r["tick_p99_ms"], r["frame_interval_p99_ms"],
                     " (past the envelope: %s)" % r["broken"] if r["broken"] else ""),
                 hollow=broken)
    c.render(os.path.join(out, "tick-p99.svg"))

    # b. CPU vs universes
    c = Chart("CPU use vs universes",
              "Same runs. Whole process, mean after warm-up; 1 = one core fully busy.",
              "Universes (log scale)", "CPU (cores busy)",
              Log2(1, 1024), Lin(0, 8, 1), HOSTS)
    c.notes = [note_hollow]
    for h in HOSTS:
        pts = [(int(r["universes"]), num(r["cpu_pct"]) / 100.0, r) for r in scaling[h]]
        cores = {"pi": 4, "linux": 8, "m4": 10}[h]
        c.series(h, pts, "%s (%d cores)" % (SHORT[h], cores),
                 lambda x, y, r, h=h: "%s, %d universes: %s cores busy, RSS peak %s MB" % (
                     HOST_LABEL[h], x, label_num(round(y, 2)), r["rss_peak_mb"]),
                 hollow=broken)
    c.render(os.path.join(out, "cpu.svg"))

    # c. Art-Net output fps vs universes
    art = by_host(read(os.path.join(data, "artnet.csv")))
    c = Chart("Art-Net output rate vs universes",
              "Every channel of every universe changing every frame, sent to loopback and captured.",
              "Universes sent (log scale)", "Frames per second per universe (mean)",
              Log2(8, 512), Lin(0, 60, 10), HOSTS)
    c.hline(50, "50 fps: full rate", right=True)
    for h in HOSTS:
        pts = [(int(r["universes"]), num(r["fps_mean"]), r) for r in art[h]]
        full = [p[0] for p in pts if num(p[2]["fps_min"]) >= 49]
        c.series(h, pts, "%s: %s at 50 fps" % (SHORT[h], max(full)) if full else SHORT[h],
                 lambda x, y, r, h=h: "%s, %d universes: %s fps mean, %s fps min, %s late of %s frames" % (
                     HOST_LABEL[h], x, r["fps_mean"], r["fps_min"], r["late_frames"], r["frames"]))
    c.render(os.path.join(out, "artnet-fps.svg"))

    # d. memory over the long run (the real show)
    mem = by_host([r for r in read(os.path.join(data, "soak_memory.csv")) if r["phase"] == "real"])
    hours = max(num(r["minutes"]) for h in HOSTS for r in mem[h]) / 60.0
    c = Chart("Memory over a long run",
              "The real show, looping its cues with nothing else running. Private memory (macOS: footprint), 10-minute means.",
              "Hours since start", "Private memory (MB)",
              Lin(0, math.ceil(hours), 1 if hours <= 12 else 2), Lin(0, 250, 50), HOSTS)
    for h in HOSTS:
        pts = [(num(r["minutes"]) / 60.0, num(r["mem_mb"]), r) for r in mem[h]]
        if not pts:
            continue
        c.series(h, pts, "%s: %s MB" % (SHORT[h], label_num(round(pts[-1][1]))),
                 lambda x, y, r, h=h: "%s, %s h: %s MB private, %s MB RSS, CPU %s%%" % (
                     HOST_LABEL[h], label_num(round(x, 2)), label_num(round(y, 1)),
                     label_num(round(num(r["rss_mb"]), 1)), label_num(round(num(r["cpu_pct"]), 1))),
                 mark=lambda p: int(num(p[2]["minutes"])) % 60 == 55)   # 10-minute bins are dense: one marker an hour
    c.render(os.path.join(out, "soak-memory.svg"))

    # e. ticks over budget per hour of the long run (the real show)
    hourly = by_host([r for r in read(os.path.join(data, "soak_hourly.csv")) if r["phase"] == "real"])
    maxh = max(int(r["hour"]) for h in HOSTS for r in hourly[h]) + 1
    c = Chart("Ticks over budget, per hour of the long run",
              "The real show. 180,000 ticks an hour at 50 Hz; a tick over 20 ms is late.",
              "Hour of the run", "Ticks over 20 ms in that hour (log scale)",
              Lin(0, maxh, 1 if maxh <= 12 else 2), Log10Zero(1, 10000), HOSTS)
    for h in HOSTS:
        pts = [(int(r["hour"]) + 0.5, num(r["ticks_over_per_h"]), r) for r in hourly[h]]
        tot = sum(int(r["ticks_over"]) for r in hourly[h])
        c.series(h, pts, "%s: %s in %d h" % (SHORT[h], label_num(tot), len(pts)),
                 lambda x, y, r, h=h: "%s, hour %s: %s ticks over (%s per hour), worst %s ms" % (
                     HOST_LABEL[h], r["hour"], r["ticks_over"], r["ticks_over_per_h"], r["worst_ms"] or "-"))
    c.render(os.path.join(out, "soak-ticks-over.svg"))

    # f. the guard's trend: every run against its host's baseline
    trend_path = os.path.join(data, "trend.csv")
    if os.path.exists(trend_path):
        trend_chart(read(trend_path), classes, os.path.join(out, "trend.svg"))


def trend_chart(rows, classes, path):
    """One point per guard run: the median, over every metric the guard
    keeps, of now / baseline (100% = the baseline; under 100 is faster).
    The median shrugs off one noisy metric; a real slowdown moves most of
    them. Hollow: that run had a metric past its tolerance."""
    import datetime
    runs = {}
    for r in rows:
        host = classes.get(r["host_class"])
        v, b = num(r["value"]), num(r["baseline"])
        if host is None or v is None or not b:
            continue
        key = (host, r["date"], r["rev"])
        runs.setdefault(key, []).append((v / b * 100.0, r["verdict"]))
    if not runs:
        return
    per_host = {h: [] for h in TREND_HOSTS}
    for (host, date, rev), vals in sorted(runs.items(), key=lambda kv: kv[0][1]):
        d = datetime.datetime.strptime(date[:16], "%Y-%m-%dT%H:%M")
        x = (d - datetime.datetime(2000, 1, 1)).total_seconds() / 86400.0
        ratios = sorted(v for v, _ in vals)
        med = ratios[len(ratios) // 2] if len(ratios) % 2 else (ratios[len(ratios) // 2 - 1] + ratios[len(ratios) // 2]) / 2
        bad = sum(1 for _, verdict in vals if verdict == "regressed")
        per_host[host].append((x, med, dict(date=date, rev=rev, n=len(vals), bad=bad)))
    hosts = [h for h in TREND_HOSTS if per_host[h]]
    xs = [p[0] for h in hosts for p in per_host[h]]
    lo, hi = math.floor(min(xs)), math.ceil(max(xs))
    if hi - lo < 2:
        lo, hi = lo - 1, lo + 1 if hi - lo < 1 else hi
    ys = [p[1] for h in hosts for p in per_host[h]]
    ylo = min(80, 10 * math.floor(min(ys) / 10.0))
    yhi = max(120, 10 * math.ceil(max(ys) / 10.0))
    c = Chart("Engine cost over time (make perf)",
              "Each point is one guard run: the median of its metrics as a % of the host's own baseline.",
              "Date of the run", "Median metric, % of baseline",
              Days(lo, hi), Lin(ylo, yhi, 10), hosts)
    c.notes = ["Under 100% is faster. Hollow marker: a metric past its tolerance in that run"]
    c.hline(100, "baseline")
    for h in hosts:
        pts = per_host[h]
        c.series(h, pts, "%s: %s%%" % (SHORT[h], label_num(round(pts[-1][1]))),
                 lambda x, y, r, h=h: "%s, %s (rev %s): %s%% of baseline over %d metrics, %d past tolerance" % (
                     HOST_LABEL[h], r["date"], r["rev"], label_num(round(y, 1)), r["n"], r["bad"]),
                 hollow=lambda p: p[2]["bad"] > 0)
    c.render(path)


# ---------------------------------------------------------------------------
# Performance over time: docs/perf-data/history.csv (tools/perf/history.py)
# ---------------------------------------------------------------------------

BAD = "#d03b30"          # a metric past its tolerance (a second cue besides colour: the tooltip says so)
# (file, title, scenario, metric, y label, how to read it)
TRENDS = [
    ("trend-rig-tick-p50", "Mixed rig: timer tick, median", "rig-mixed-u0032", "tick_p50_ms",
     "Tick time, median (ms)", "32 universes of moving heads, pixel bars and dimmers."),
    ("trend-rig-cpu", "Mixed rig: CPU", "rig-mixed-u0032", "cpu_pct",
     "CPU (% of one core)", "The same run: whole process, mean after warm-up."),
    ("trend-c-matrix-tick", "64 RGB matrices: timer tick, median", "c-matrix-n0064", "tick_p50_ms",
     "Tick time, median (ms)", "Scripted matrices, 32 universes."),
    ("trend-cp-chasers-tick", "64 chasers: timer tick, median", "cp-chasers-n0064", "tick_p50_ms",
     "Tick time, median (ms)", "Per-step chasers, 32 universes."),
    ("trend-a1p-rss-per-universe", "Memory per universe", "a1p", "rss_mb_per_universe",
     "Peak RSS added per universe (MB)", "Realistic mixed load: slope of RSS from 8 to 256 universes."),
    ("trend-hotpath-scene-frame", "Hot path: a scene frame", "hotpath", "scene_frame_170_ns",
     "CPU per scene frame, 170 fixtures (ns)", "One thread, no I/O."),
    ("trend-hotpath-fader-write", "Hot path: a fader write", "hotpath", "genericfader_write_510_ns",
     "CPU per fader write, 510 channels (ns)", "One thread, no I/O."),
    ("trend-hotpath-universe-frame", "Hot path: a universe frame", "hotpath", "universe_frame_510_ns",
     "CPU per universe frame (ns)", "One thread, no I/O."),
]


class Runs:
    """The x axis: the runs in date order, evenly spaced (runs cluster in time),
    labelled with their dates."""
    MONTHS = Days.MONTHS

    def __init__(self, dates):
        self.dates = dates            # one 'YYYY-MM-DD...' per run, in order
        self.lo, self.hi = -0.5, len(dates) - 0.5

    def frac(self, v):
        return (v - self.lo) / (self.hi - self.lo)

    def ticks(self):
        firsts = [i for i, d in enumerate(self.dates) if i == 0 or d[:10] != self.dates[i - 1][:10]]
        gap = len(self.dates) / 8.0        # labels about 70 px apart at most 8 across
        out = []
        for i in firsts:
            if not out or i - out[-1] >= gap:
                out.append(i)
        return out

    def fmt(self, v):
        d = self.dates[int(v)]
        return "%s %d" % (self.MONTHS[int(d[5:7]) - 1], int(d[8:10]))


def sig3(v):
    """3 significant digits, no trailing zeros (0.15, 1.63, 11000)."""
    if v == 0:
        return "0"
    d = max(0, 2 - int(math.floor(math.log10(abs(v)))))
    return label_num(round(v, d))


def nice_axis(hi):
    """A linear axis from 0 over a 'nice' step: about 5 ticks, hi rounded up."""
    hi = max(hi, 1e-9) * 1.25      # headroom for the notes
    mag = 10 ** math.floor(math.log10(hi / 5.0))
    for k in (1, 2, 2.5, 5, 10):
        if hi / (k * mag) <= 5.0 + 1e-9:
            step = k * mag
            break
    top = math.ceil(hi / step) * step
    return Lin(0, top, step, lambda v, step=step: ("%g" % round(v, 6)) if step < 1 else label_num(v))


def read_annotations(data):
    p = os.path.join(data, "annotations.csv")
    return read(p) if os.path.exists(p) else []


def trends(data, out, classes, hist=None, annotations=None):
    """One SVG per key metric: its value over the runs in history.csv, per host class."""
    path = os.path.join(data, "history.csv")
    if hist is None:
        if not os.path.exists(path):
            print("no", path)
            return []
        hist = read(path)
    if annotations is None:
        annotations = read_annotations(data)
    os.makedirs(out, exist_ok=True)
    verdict_of = {(r["date"], r["sha"], r["host"], r["kind"]): r["verdict"]
                  for r in hist if r["scenario"] == "all" and r["metric"] == "perf"}
    written = []
    for fname, title, scenario, metric, ylabel, what in TRENDS:
        rows = [r for r in hist if r["scenario"] == scenario and r["metric"] == metric
                and num(r["value"]) is not None and classes.get(r["host"])]
        if not rows:
            continue
        # the runs of this chart, in date order (history.csv is in run order within a date)
        order = []
        for i, r in sorted(enumerate(rows), key=lambda t: (t[1]["date"], t[0])):
            k = (r["date"], r["sha"], r["kind"])
            if k not in order:
                order.append(k)
        pos = {k: i for i, k in enumerate(order)}
        dates = [k[0] for k in order]

        def xat(date, sha=""):
            for k in order:
                if k[0] == date and (not sha or k[1] == sha):
                    return pos[k]
            return sum(1 for d in dates if d < date) - 0.5

        hosts = [h for h in TREND_HOSTS if any(classes[r["host"]] == h for r in rows)]
        top = max(num(r["value"]) for r in rows)
        c = Chart(title, "%s Lower is better. One point per run, in date order." % what,
                  "Date of the run (runs evenly spaced)", ylabel, Runs(dates), nice_axis(top), hosts)
        c.notes = ["Red: this metric past its tolerance. Hollow: a short hand run (A/B or bisect), not judged.",
                   "Dashed line: that host's baseline re-recorded. Dotted: a change worth knowing."]
        c.top = 132
        # re-records of the baselines (the record rows of these hosts)
        shown = set()
        for r in rows:
            if r["kind"] != "record":
                continue
            h = classes[r["host"]]
            x = pos[(r["date"], r["sha"], r["kind"])]
            c.body.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1.25" '
                          'stroke-dasharray="6 4"><title>%s</title></line>' % (
                              f(c.X(x)), f(c.py0), f(c.X(x)), f(c.py1), COLOR[h],
                              esc("%s: baseline re-recorded %s at %s" % (HOST_LABEL.get(h, h), r["date"], r["sha"]))))
            shown.add(h)
        for hname in hosts:
            cls = [k for k, v in classes.items() if v == hname]
            pts, shorts = [], []
            for i, r in sorted(enumerate(rows), key=lambda t: (t[1]["date"], t[0])):
                if r["host"] not in cls:
                    continue
                x, y = pos[(r["date"], r["sha"], r["kind"])], num(r["value"])
                run = verdict_of.get((r["date"], r["sha"], r["host"], r["kind"]), "")
                tip = "%s, %s, %s (%s%s): %s%s" % (
                    HOST_LABEL.get(hname, hname), r["date"], r["sha"], r["kind"],
                    ", " + r["machine"] if r["machine"] else "", sig3(y),
                    "; baseline %s" % r["baseline"] if r["baseline"] else "")
                if run or r["verdict"]:
                    tip += "; this metric: %s; run: %s" % (r["verdict"] or "-", run or "-")
                (shorts if r["kind"] in ("ab", "bisect") else pts).append((x, y, r, tip))
            if pts:
                d = " ".join(("M" if i == 0 else "L") + "%s %s" % (f(c.X(x)), f(c.Y(y)))
                             for i, (x, y, _, _) in enumerate(pts))
                c.body.append('<path d="%s" fill="none" stroke="%s" stroke-width="2" stroke-linejoin="round"/>'
                              % (d, COLOR[hname]))
            for x, y, r, tip in shorts:
                c.body.append(c.marker(hname, c.X(x), c.Y(y), hollow=True, tip=tip, size=6))
            for x, y, r, tip in pts:
                bad = r["verdict"] == "regressed"
                c.body.append(c.marker(hname, c.X(x), c.Y(y), tip=tip, color=BAD if bad else None))
            if pts:
                x, y, r, _ = pts[-1]
                c.ends.append((hname, c.X(x), c.Y(y), "%s: %s" % (SHORT[hname], sig3(y))))
        # annotations: a line where it happened, the words above the plot
        slot = 0
        for a in annotations:
            if a.get("charts") not in ("*", None, "") and fname.replace("trend-", "") not in a["charts"].split(","):
                continue
            x = xat(a["date"], a.get("sha", "") if a.get("sha") and any(k[1] == a["sha"] for k in order) else "")
            px = c.X(x)
            c.body.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="1.5" '
                          'stroke-dasharray="2 4"><title>%s</title></line>' % (
                              f(px), f(c.py0), f(px), f(c.py1 + 4), REF, esc("%s %s: %s" % (a["date"], a.get("sha", ""), a["text"]))))
            anchor = "end" if px > (c.px0 + c.px1) / 2 else "start"
            c.body.append('<text x="%s" y="%s" font-size="11.5" fill="%s" text-anchor="%s">%s</text>' % (
                f(px + (-5 if anchor == "end" else 5)), f(c.py1 + 14 + 14 * slot), INK, anchor, esc(a["text"])))
            slot += 1
        c.render(os.path.join(out, fname + ".svg"))
        written.append(os.path.join(out, fname + ".svg"))
    return written


def self_test():
    import tempfile
    ok = True

    def check(cond, what):
        nonlocal ok
        if not cond:
            ok = False
            print("FAIL plot:", what)

    def row(date, sha, host, kind, scen, metric, value, base="", verdict=""):
        return dict(date=date, sha=sha, host=host, machine="box", qt="6", kind=kind, scenario=scen,
                    metric=metric, value=value, baseline=base, verdict=verdict)
    h1, h2 = "macos-apple-m4-max-10p4e", "linux-intel-core-i7-9750h-8t"
    hist = []
    for d, sha, kind, v, vd, run in (("2026-10-29T06:22Z", "aaa", "record", "1.4", "record", "record"),
                                     ("2026-10-30T03:00Z", "bbb", "nightly", "1.45", "ok", "ok"),
                                     ("2026-11-01T04:34Z", "ccc", "perf", "1.63", "regressed", "regressed"),
                                     ("2026-11-01T05:10Z", "ddd", "bisect", "1.5", "", "")):
        hist.append(row(d, sha, h1, kind, "a1p", "rss_mb_per_universe", v, "1.4", vd))
        hist.append(row(d, sha, h1, kind, "all", "perf", "1" if run == "regressed" else "0", "", run) if run else {})
    hist = [r for r in hist if r]
    hist.append(row("2026-10-29T06:25Z", "eee", h2, "record", "a1p", "rss_mb_per_universe", "1.1", "1.1", "record"))
    hist.append(row("2026-11-01T04:34Z", "ccc", h1, "perf", "hotpath", "scene_frame_170_ns", "11000", "11151", "ok"))
    ann = [dict(date="2026-10-31T18:54Z", sha="zzz", text="FadeChannel grows", charts="a1p-rss-per-universe")]
    classes = {h1: "m4max", h2: "linux"}
    HOST_LABEL.update({"m4max": "Apple M4 Max Mac", "linux": "Intel i7 (Linux)"})
    with tempfile.TemporaryDirectory() as d:
        w = trends(d, d, classes, hist=hist, annotations=ann)
        names = sorted(os.path.basename(p) for p in w)
        check(names == ["trend-a1p-rss-per-universe.svg", "trend-hotpath-scene-frame.svg"], "only charts with data: %s" % names)
        svg = open(os.path.join(d, "trend-a1p-rss-per-universe.svg")).read()
        check(svg.startswith("<svg") and svg.rstrip().endswith("</svg>"), "an svg")
        check(BAD in svg, "a failed metric is red")
        check("baseline re-recorded" in svg and "FadeChannel grows" in svg, "record line and annotation")
        check("66da" not in svg and "ccc" in svg, "commits in the hover text")
        check("Apple M4 Max Mac" in svg and "Intel i7 (Linux)" in svg, "both hosts")
        check(svg.count("<polygon") + svg.count("<circle") + svg.count("<rect") >= 8, "markers drawn")
        w2 = trends(d, os.path.join(d, "again"), classes, hist=hist, annotations=ann)
        check(open(w2[0]).read() == svg, "deterministic")
        check(trends(d, os.path.join(d, "none"), classes, hist=[], annotations=[]) == [], "empty history draws nothing")
    check(Runs(["2026-10-29T0", "2026-10-29T1", "2026-11-01T0"]).ticks() == [0, 2], "date ticks at the first run of a day")
    print("plot self-test:", "ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    main()
