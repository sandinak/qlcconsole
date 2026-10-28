#!/usr/bin/env python3
"""ArtDMX on the wire, from a pcap (Linux lo: Ethernet; macOS lo0: NULL).

Per Art-Net universe, over an optional time window: frames, frames/s,
inter-frame gap p50/p95/p99/max (ms), gaps > 1.5x the expected period,
sequence-number skips (ArtDMX Sequence, 1..255, 0 = off) and repeats.

  wire_stats.py CAP.pcap [--from T0 --to T1] [--universes 0-31] [--json OUT]
                         [--period-ms 20] [--windows phases.json]

--windows: a JSON list of {"name":..,"t0":..,"t1":..} (epoch seconds); a
summary per window is printed and written.
"""
import argparse
import json
import struct
import sys


def read_pcap(path):
    with open(path, "rb") as f:
        hdr = f.read(24)
        if len(hdr) < 24:
            return
        magic = struct.unpack("<I", hdr[:4])[0]
        if magic == 0xa1b2c3d4:
            e, nano = "<", False
        elif magic == 0xd4c3b2a1:
            e, nano = ">", False
        elif magic == 0xa1b23c4d:
            e, nano = "<", True
        else:
            raise SystemExit("not a pcap: %x" % magic)
        link = struct.unpack(e + "I", hdr[20:24])[0]
        while True:
            ph = f.read(16)
            if len(ph) < 16:
                break
            sec, frac, incl, orig = struct.unpack(e + "IIII", ph)
            data = f.read(incl)
            if len(data) < incl:
                break
            t = sec + frac / (1e9 if nano else 1e6)
            if link == 1:             # Ethernet
                if data[12:14] != b"\x08\x00":
                    continue
                ip = data[14:]
            elif link == 0:           # BSD loopback (NULL): 4-byte family
                ip = data[4:]
            elif link == 12 or link == 101:   # raw IP
                ip = data
            elif link == 113:         # Linux cooked
                ip = data[16:]
            else:
                continue
            if len(ip) < 28 or (ip[0] >> 4) != 4 or ip[9] != 17:
                continue
            ihl = (ip[0] & 0x0F) * 4
            sport, dport = struct.unpack("!HH", ip[ihl:ihl + 4])
            yield t, sport, dport, ip[ihl + 8:]


def pct(v, p):
    if not v:
        return 0.0
    v = sorted(v)
    i = max(0, min(len(v) - 1, int(-(-p / 100.0 * len(v) // 1)) - 1))
    return v[i]


def read_artdmx(path):
    """(t, universe, sequence, length) of every ArtDMX to port 6454: small
    tuples, so a capture of a million frames fits in memory."""
    out = []
    for t, sport, dport, pl in read_pcap(path):
        if dport != 6454 or len(pl) < 18 or pl[:8] != b"Art-Net\x00":
            continue
        if pl[8] != 0x00 or pl[9] != 0x50:
            continue
        out.append((t, pl[14] | (pl[15] << 8), pl[12], (pl[16] << 8) | pl[17]))
    return out


def analyse(pkts, t0, t1, unis, period):
    per = {}
    for t, uni, seq, length in pkts:
        if t < t0 or t > t1:
            continue
        if unis is not None and uni not in unis:
            continue
        d = per.setdefault(uni, {"t": [], "seq": [], "len": length})
        d["t"].append(t)
        d["seq"].append(seq)
    out = {}
    for uni, d in sorted(per.items()):
        ts = d["t"]
        gaps = [(b - a) * 1000.0 for a, b in zip(ts, ts[1:])]
        skips = repeats = 0
        for a, b in zip(d["seq"], d["seq"][1:]):
            if a == 0 or b == 0:
                continue
            exp = 1 if a == 255 else a + 1
            if b == a:
                repeats += 1
            elif b != exp:
                skips += 1
        span = (ts[-1] - ts[0]) if len(ts) > 1 else 0.0
        out[uni] = {"frames": len(ts), "fps": round((len(ts) - 1) / span, 2) if span > 0 else 0.0,
                    "gap_p50": round(pct(gaps, 50), 3), "gap_p95": round(pct(gaps, 95), 3),
                    "gap_p99": round(pct(gaps, 99), 3), "gap_max": round(max(gaps), 3) if gaps else 0.0,
                    "late": sum(1 for g in gaps if g > 1.5 * period),
                    "seq_skips": skips, "seq_repeats": repeats, "dmx_len": d["len"]}
    return out


def summary(per):
    if not per:
        return {"universes": 0}
    v = list(per.values())
    return {"universes": len(v), "frames": sum(x["frames"] for x in v),
            "fps_min": min(x["fps"] for x in v), "fps_mean": round(sum(x["fps"] for x in v) / len(v), 2),
            "gap_p99_max": max(x["gap_p99"] for x in v), "gap_max": max(x["gap_max"] for x in v),
            "late": sum(x["late"] for x in v), "seq_skips": sum(x["seq_skips"] for x in v),
            "seq_repeats": sum(x["seq_repeats"] for x in v)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pcap")
    ap.add_argument("--from", dest="t0", type=float, default=0)
    ap.add_argument("--to", dest="t1", type=float, default=1e12)
    ap.add_argument("--universes", default="")
    ap.add_argument("--period-ms", type=float, default=20.0)
    ap.add_argument("--windows", default="")
    ap.add_argument("--json", default="")
    a = ap.parse_args()
    unis = None
    if a.universes:
        unis = set()
        for part in a.universes.split(","):
            if "-" in part:
                lo, hi = part.split("-")
                unis.update(range(int(lo), int(hi) + 1))
            else:
                unis.add(int(part))
    pkts = read_artdmx(a.pcap)
    res = {}
    windows = json.load(open(a.windows)) if a.windows else [{"name": "all", "t0": a.t0, "t1": a.t1}]
    for w in windows:
        per = analyse(pkts, w["t0"], w["t1"], unis, a.period_ms)
        s = summary(per)
        res[w["name"]] = {"summary": s, "per_universe": per}
        print("%-14s %s" % (w["name"], json.dumps(s)))
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
