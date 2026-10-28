#!/usr/bin/env python3
"""
Art-Net output on the wire vs universes: qlcstress --mode realtime with the
'mixed' load, every universe patched to Art-Net on the 127.0.0.1 line
(--artnet 127.0.0.1), the loopback captured, each universe's frame rate,
gaps and sequence continuity measured. LOOPBACK ONLY (on Linux run inside
`unshare -n`).

  wire_sweep.py --qlcstress Q --resources R --plugins DIR --out DIR --capture mac|linux
"""
import argparse
import json
import math
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from io_load import start_capture, stop_capture  # noqa: E402
import wire_stats  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qlcstress", required=True)
    ap.add_argument("--resources", required=True)
    ap.add_argument("--plugins", required=True, help="dir with only the Art-Net plugin")
    ap.add_argument("--out", required=True)
    ap.add_argument("--capture", choices=("mac", "linux"), required=True)
    ap.add_argument("--universes", default="8,32,64,128,256")
    ap.add_argument("--seconds", type=int, default=60)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rows = []
    for u in [int(x) for x in a.universes.split(",")]:
        name = "u%03d" % u
        pcap = os.path.join(a.out, name + ".pcap")
        cap = start_capture(a.capture, pcap)
        q = max(1, math.ceil(u / 4))
        t0 = time.time()
        cmd = [a.qlcstress, "engine", "--mode", "realtime", "--seconds", str(a.seconds),
               "--warmup-seconds", "10", "--universes", str(u), "--fixtures-per-uni", "170",
               "--scenes", str(u), "--chasers", str(q), "--matrices", str(q), "--efx", str(q),
               "--collections", "0", "--cuelists", str(max(1, math.ceil(u / 8))),
               "--scene-fixtures", "170", "--efx-per-universe",
               "--artnet", "127.0.0.1", "--plugin-dir", a.plugins,
               "--fixtures-dir", os.path.join(a.resources, "fixtures"),
               "--scripts-dir", os.path.join(a.resources, "rgbscripts"),
               "--out", os.path.join(a.out, name), "--scenario", "wire/" + name]
        with open(os.path.join(a.out, name + ".log"), "w") as lf:
            rc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT).returncode
        time.sleep(1)
        stop_capture(cap)
        res = json.load(open(os.path.join(a.out, name + ".json")))
        # the measured window: after warm-up, to the end of the run
        t_end = os.path.getmtime(os.path.join(a.out, name + ".json")) - 1.0
        win_t0 = t_end - (a.seconds - 10)
        pk = wire_stats.read_artdmx(pcap)
        per = wire_stats.analyse(pk, win_t0, t_end - 0.5, set(range(u)), 20.0)
        s = wire_stats.summary(per)
        row = {"universes": u, "rc": rc, "compute_p99": res["compute"]["p99"],
               "gap_p99": res["gap"]["p99"], "cpuPct": round(res["cpuPct"], 1), "wire": s}
        json.dump({"row": row, "per_universe": per}, open(os.path.join(a.out, name + ".wire.json"), "w"), indent=1)
        rows.append(row)
        print(json.dumps(row), flush=True)
        if u > 8:
            os.remove(pcap)          # big; the per-universe numbers are kept
    json.dump(rows, open(os.path.join(a.out, "wire_sweep.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
