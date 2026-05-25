#!/usr/bin/env python3
"""
QLC+ full-application black-box stress driver.

Launches the real qlcplus binary headless in operate mode with web access,
loads a (large) workspace, then hammers it over the WebSocket API while
monitoring the process for crashes, hangs, CPU saturation and RSS growth
(leaks). The goal is to find where the running application breaks.

Torture patterns (cycled):
  * start-all      : start every function at once
  * churn          : rapid random start/stop of functions
  * gm-sweep       : sweep the Grand Master 0..255..0 quickly
  * blackout-flap  : slam Grand Master between 0 and 255
  * reload         : (optional) re-load the project from disk

No third-party deps: stdlib WebSocket client (wsclient.py) + `ps` sampling.

Example:
  python3 stress_driver.py \
      --qlcplus ../../build-arm64/main/qlcplus \
      --workspace /tmp/stress80.qxw \
      --port 9999 --duration 120
"""

import argparse
import http.client
import os
import random
import re
import signal
import subprocess
import sys
import time

from wsclient import WSClient, WSError


def ps_sample(pid):
    """Return (pcpu, rss_mb) for pid, or (None, None) if gone."""
    try:
        out = subprocess.check_output(
            ["ps", "-o", "pcpu=,rss=", "-p", str(pid)],
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except subprocess.CalledProcessError:
        return None, None
    if not out:
        return None, None
    parts = out.split()
    return float(parts[0]), float(parts[1]) / 1024.0


class Monitor:
    def __init__(self, pid):
        self.pid = pid
        self.samples = []      # (t, pcpu, rss)
        self.rss0 = None
        self.rss_peak = 0.0

    def sample(self):
        cpu, rss = ps_sample(self.pid)
        if rss is None:
            return False
        if self.rss0 is None:
            self.rss0 = rss
        self.rss_peak = max(self.rss_peak, rss)
        self.samples.append((time.time(), cpu, rss))
        return True

    def report(self):
        if not self.samples:
            return "monitor: no samples"
        cpus = [c for _, c, _ in self.samples if c is not None]
        rss_end = self.samples[-1][2]
        return (
            f"monitor: samples={len(self.samples)} "
            f"cpu(avg/max)={sum(cpus)/len(cpus):.0f}/{max(cpus):.0f}%% "
            f"rss start/peak/end={self.rss0:.0f}/{self.rss_peak:.0f}/{rss_end:.0f} MB "
            f"growth={rss_end-self.rss0:+.0f} MB"
        )


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def launch_qlcplus(qlcplus, port, libdir, ticklog=None):
    """Launch the app BARE (web access only). We deliberately do NOT pass
    -o/-p: under the offscreen platform, app.loadXML() pops a modal dialog that
    blocks main() before the web server starts. Instead we load the workspace
    afterwards over the /loadProject HTTP API (no dialog)."""
    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"   # headless on macOS/Linux without a WM
    if libdir:
        env["DYLD_FALLBACK_LIBRARY_PATH"] = libdir
        env["LD_LIBRARY_PATH"] = libdir
    if ticklog:
        env["QLC_TICKLOG"] = str(ticklog)  # engine logs tick overruns to stderr
    args = [qlcplus, "-w", "-wp", str(port)]
    log(f"launching: {' '.join(args)}  (QT_QPA_PLATFORM=offscreen, QLC_TICKLOG={ticklog})")
    proc = subprocess.Popen(args, env=env,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.STDOUT)
    return proc


def http_get_ok(host, port, timeout=2.0):
    try:
        c = http.client.HTTPConnection(host, port, timeout=timeout)
        c.request("GET", "/")
        r = c.getresponse()
        r.read()
        c.close()
        return r.status == 200
    except OSError:
        return False


def wait_http(host, port, retries=60):
    for _ in range(retries):
        if http_get_ok(host, port):
            return True
        time.sleep(0.5)
    return False


def load_project(host, port, workspace_path):
    """POST the .qxw to /loadProject as multipart/form-data. The app's custom
    parser strips to the first '\\n\\r\\n' and truncates at the last, so we
    bracket the XML accordingly."""
    with open(workspace_path, "rb") as f:
        xml = f.read()
    boundary = "----qlcstressboundary"
    head = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="qlc"; filename="project.qxw"\r\n'
        "\r\n"
    ).encode()
    tail = (f"\n\r\n--{boundary}--\r\n").encode()
    body = head + xml + tail
    c = http.client.HTTPConnection(host, port, timeout=30)
    c.request("POST", "/loadProject", body=body,
              headers={"Content-Type": f"multipart/form-data; boundary={boundary}",
                       "Content-Length": str(len(body))})
    r = c.getresponse()
    r.read()
    c.close()
    return r.status in (200, 302)


def connect_ws(host, port, retries=40):
    for i in range(retries):
        try:
            ws = WSClient(host, port)
            ws.connect()
            return ws
        except (OSError, WSError):
            time.sleep(0.5)
    return None


def get_function_ids(ws):
    msg = ws.request("QLC+API|getFunctionsList",
                     expect_prefix="QLC+API|getFunctionsList", wait=5.0)
    if not msg:
        return []
    fields = msg.split("|")[2:]   # drop "QLC+API","getFunctionsList"
    ids = []
    # fields alternate id,name,id,name,...
    for i in range(0, len(fields) - 1, 2):
        if re.fullmatch(r"\d+", fields[i]):
            ids.append(int(fields[i]))
    return ids


# ---- torture patterns -------------------------------------------------------
def pat_start_all(ws, ids, rng):
    for fid in ids:
        ws.send(f"QLC+API|setFunctionStatus|{fid}|1")
    return f"start-all ({len(ids)} funcs)"


def pat_churn(ws, ids, rng):
    n = min(len(ids), 200)
    for _ in range(n):
        fid = rng.choice(ids)
        ws.send(f"QLC+API|setFunctionStatus|{fid}|{rng.randint(0,1)}")
    return f"churn ({n} toggles)"


def pat_gm_sweep(ws, ids, rng):
    for v in list(range(0, 256, 8)) + list(range(255, -1, -8)):
        ws.send(f"GM_VALUE|{v}")
    return "gm-sweep 0..255..0"


def pat_blackout_flap(ws, ids, rng):
    for _ in range(40):
        ws.send(f"GM_VALUE|{rng.choice([0,255])}")
    return "blackout-flap x40"


def pat_channel_spray(ws, ids, rng):
    for _ in range(300):
        addr = rng.randint(1, 512 * 4)
        ws.send(f"CH|{addr}|{rng.randint(0,255)}")
    return "channel-spray x300"


PATTERNS = [pat_start_all, pat_churn, pat_gm_sweep, pat_blackout_flap, pat_channel_spray]


def main():
    ap = argparse.ArgumentParser(description="QLC+ full-app black-box stress driver")
    ap.add_argument("--qlcplus", required=True, help="path to qlcplus binary")
    ap.add_argument("--workspace", required=True, help="path to .qxw workspace")
    ap.add_argument("--libdir", default=None, help="dir with libqlcplus*.dylib (DYLD path)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9999)
    ap.add_argument("--duration", type=int, default=120, help="seconds to torture")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--leak-threshold", type=float, default=200.0,
                    help="flag if RSS grows more than this many MB")
    ap.add_argument("--ticklog", type=int, default=100,
                    help="QLC_TICKLOG interval for engine tick-overrun logging (0=off)")
    args = ap.parse_args()

    rng = random.Random(args.seed)

    if not os.path.exists(args.qlcplus):
        log(f"ERROR: qlcplus not found: {args.qlcplus}"); return 2
    if not os.path.exists(args.workspace):
        log(f"ERROR: workspace not found: {args.workspace}"); return 2

    proc = launch_qlcplus(args.qlcplus, args.port, args.libdir, ticklog=args.ticklog)
    mon = Monitor(proc.pid)
    verdict = 0

    try:
        log("waiting for web server ...")
        if not wait_http(args.host, args.port):
            log("ERROR: web server never came up (app may have crashed)")
            return 1
        log(f"web up. loading workspace {args.workspace} via /loadProject ...")
        if not load_project(args.host, args.port, args.workspace):
            log("ERROR: /loadProject failed")
            return 1
        time.sleep(2.0)  # let the load settle

        ws = connect_ws(args.host, args.port)
        if ws is None:
            log("ERROR: could not open WebSocket")
            return 1
        log("entering operate mode (QLC+CMD|opMode) ...")
        ws.send("QLC+CMD|opMode")
        time.sleep(1.0)
        log("enumerating functions ...")
        ids = get_function_ids(ws)
        log(f"workspace exposes {len(ids)} functions")
        if not ids:
            log("WARNING: no functions found; continuing with GM/channel patterns only")
            ids = [0]

        start = time.time()
        last_resp_ok = start
        round_no = 0
        while time.time() - start < args.duration:
            round_no += 1
            pattern = PATTERNS[round_no % len(PATTERNS)]
            try:
                what = pattern(ws, ids, rng)
            except WSError as e:
                log(f"WS error during pattern: {e} -> checking process")
                break

            # liveness probe: app must still answer the API
            alive_msg = ws.request("QLC+API|getFunctionsNumber",
                                   expect_prefix="QLC+API|getFunctionsNumber", wait=3.0)
            responsive = alive_msg is not None
            if responsive:
                last_resp_ok = time.time()

            ok = mon.sample()
            if not ok or proc.poll() is not None:
                log(f"*** CRASH: process exited (rc={proc.returncode}) during '{what}'")
                verdict = 1
                break

            stalled = time.time() - last_resp_ok
            cpu, rss = (mon.samples[-1][1], mon.samples[-1][2]) if mon.samples else (0, 0)
            flag = ""
            if not responsive:
                flag = f"  !! UNRESPONSIVE for {stalled:.1f}s"
            log(f"round {round_no:3d}: {what:<22} resp={responsive} cpu={cpu:.0f}% rss={rss:.0f}MB{flag}")

            if stalled > 15:
                log(f"*** HANG: no API response for {stalled:.1f}s")
                verdict = 1
                break
            time.sleep(0.4)

        # final assessment
        log(mon.report())
        if mon.rss0 is not None and (mon.rss_peak - mon.rss0) > args.leak_threshold:
            log(f"*** POSSIBLE LEAK: RSS grew {mon.rss_peak - mon.rss0:.0f} MB "
                f"(> {args.leak_threshold:.0f} MB threshold)")
            verdict = max(verdict, 1)
        if verdict == 0:
            log("RESULT: survived torture run within thresholds")
        else:
            log("RESULT: breaking point detected (see above)")

    finally:
        try:
            if proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
                time.sleep(2)
                if proc.poll() is None:
                    proc.kill()
        except Exception:
            pass

    return verdict


if __name__ == "__main__":
    sys.exit(main())
