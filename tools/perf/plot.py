#!/usr/bin/env python3
"""Charts for docs/performance.md, as static SVG, from docs/perf-data/*.csv.

  python3 -I tools/perf/plot.py [DATA_DIR] [OUT_DIR]

DATA_DIR defaults to docs/perf-data (made by tools/perf/extract.py), OUT_DIR
to docs/images/perf. No dependencies: a small SVG writer is below. The
output is deterministic (no dates, fixed number formatting), so rerunning
on the same data gives byte-identical files.

Chart rules: one y axis per chart; a legend for two or more series plus a
direct label at each line's end; hosts always in the order Pi, Linux, M4 with
the same colour and marker shape (the colours are the first three slots of a
colour-blind-safe categorical palette, validated all-pairs on the light
surface; the marker shape is a second cue); 2 px lines, 8 px markers; a
light surface with a hairline border, so the charts read the same on a light
or a dark page; text in neutral ink; log scales are labelled as such.
"""
import csv
import math
import os
import sys

HOSTS = ["pi", "linux", "m4"]
COLOR = {"pi": "#2a78d6", "linux": "#eb6834", "m4": "#1baf7a"}
SHAPE = {"pi": "circle", "linux": "square", "m4": "triangle"}

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
        return T

    def X(self, v):
        return self.px0 + self.x.frac(v) * (self.px1 - self.px0)

    def Y(self, v):
        return self.py0 - self.y.frac(v) * (self.py0 - self.py1)

    def marker(self, host, cx, cy, hollow=False, tip=None, size=8):
        c = COLOR[host]
        fill = SURFACE if hollow else c
        r = size / 2
        t = "<title>%s</title>" % esc(tip) if tip else ""
        common = 'fill="%s" stroke="%s" stroke-width="%s"' % (fill, c if hollow else SURFACE, "2" if hollow else "1.5")
        if SHAPE[host] == "circle":
            return '<circle cx="%s" cy="%s" r="%s" %s>%s</circle>' % (f(cx), f(cy), f(r + 0.5), common, t)
        if SHAPE[host] == "square":
            return '<rect x="%s" y="%s" width="%s" height="%s" %s>%s</rect>' % (
                f(cx - r), f(cy - r), f(size), f(size), common, t)
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
SHORT = {"pi": "Pi 5", "linux": "i7", "m4": "M4"}


def by_host(rows):
    out = {h: [] for h in HOSTS}
    for r in rows:
        if r["host"] in out:
            out[r["host"]].append(r)
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    data = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "docs", "perf-data")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "..", "docs", "images", "perf")
    os.makedirs(out, exist_ok=True)
    for r in read(os.path.join(data, "hosts.csv")):
        HOST_LABEL[r["host"]] = r["label"]

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


if __name__ == "__main__":
    main()
