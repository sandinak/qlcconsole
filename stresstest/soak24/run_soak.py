#!/usr/bin/env python3
"""
24-hour soak of the real app, on screen, on the machine that runs the shows.

Three phases (default 8 h each), each a fresh start of the app:

  real     the show as it is, cycling through its looks
  scaled   the show with a synthetic block on top (merge_block.py): every
           block matrix and chaser running all the while, looks cycling
  rough    the show, handled roughly: churn (fast start/stop), grand master
           sweeps, start-all/stop-all, a reload of the project every hour

The app is started through `open` (LaunchServices), so it is qlcconsole.app
that talks to the network, with the permission the user gave it; with web
access on, and the show posted to /loadProject (no dialog). Every 30 s:
CPU, memory and threads of the app, whether it still answers, and the
engine's tick log (QLC_TICKLOG) since the last sample. A crash or a hang is
recorded and the app started again, and the phase carries on.

  python3 run_soak.py --show real.qlcc --scaled-show stress.qlcc --out DIR

Stdlib only (wsclient.py from ../driver). Stop it with Ctrl-C or a TERM: it
stops the app and writes the summary.
"""
import argparse
import csv
import glob
import gzip
import http.client
import json
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "driver"))
from wsclient import WSClient, WSError  # noqa: E402

APP = "/Applications/qlcconsole.app"
EXE = APP + "/Contents/MacOS/qlcconsole"
# A show posted to /loadProject has no file name: it autosaves as "untitled",
# and a start after the run stopped the app would offer it back.
UNTITLED_AUTOSAVE = os.path.expanduser(
    "~/Library/Application Support/qlcconsole/untitled.qxw.autosave")
STOP = False


def now():
    return time.strftime("%Y-%m-%d %H:%M:%S")


class Run:
    def __init__(self, out):
        self.out = out
        os.makedirs(out, exist_ok=True)
        self.events = open(os.path.join(out, "events.log"), "a", buffering=1)
        new = not os.path.exists(os.path.join(out, "samples.csv"))
        self.csvf = open(os.path.join(out, "samples.csv"), "a", newline="", buffering=1)
        self.csv = csv.writer(self.csvf)
        if new:
            self.csv.writerow(["time", "phase", "elapsed_s", "pid", "cpu_pct", "rss_mb",
                               "threads", "answers", "ticks_over", "tick_mean_ms",
                               "tick_max_ms", "overrun_pct", "running_note"])

    def event(self, phase, msg):
        line = "%s [%s] %s" % (now(), phase, msg)
        print(line, flush=True)
        self.events.write(line + "\n")


# ---- the app ----------------------------------------------------------------

def app_pids():
    out = subprocess.run(["pgrep", "-f", "^" + re.escape(EXE) + "( |$)"],
                         capture_output=True, text=True).stdout.split()
    return [int(p) for p in out]


def start_app(port, log_path, ticklog):
    # -d 1: warnings and up reach the log (the tick log is warnings; the app
    # shows only criticals otherwise). No recovery prompt on screen between
    # launches: the run's own autosave is not the user's work.
    subprocess.run(["open", "-n", "-a", APP,
                    "--stdout", log_path, "--stderr", log_path,
                    "--env", "QLC_TICKLOG=%d" % ticklog,
                    "--env", "QLC_NO_RECOVERY_PROMPT=1",
                    "--args", "-d", "1", "-w", "-wp", str(port)], check=True)
    for _ in range(120):
        pids = app_pids()
        if pids:
            return pids[0]
        time.sleep(0.5)
    return None


def stop_app(pid):
    if pid is None:
        return
    try:
        os.kill(pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    for _ in range(40):
        time.sleep(0.25)
        if pid not in app_pids():
            return
    try:
        os.kill(pid, signal.SIGKILL)       # leaves no crash report
    except ProcessLookupError:
        pass


def wait_http(port, tries=240):
    for _ in range(tries):
        try:
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            c.request("GET", "/")
            if c.getresponse().status < 500:
                return True
        except OSError:
            pass
        time.sleep(0.5)
    return False


def load_project(port, path):
    """POST the show to /loadProject as the app's own parser wants it: it
    keeps what lies between the first and the last "\\n\\r\\n" (framing as
    ../driver/stress_driver.py, which is known to work)."""
    xml = open(path, "rb").read()
    boundary = "----soak24boundary"
    head = ("--%s\r\nContent-Disposition: form-data; name=\"qlc\"; "
            "filename=\"project.qxw\"\r\n\r\n" % boundary).encode()
    body = head + xml + ("\n\r\n--%s--\r\n" % boundary).encode()
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=120)
    c.request("POST", "/loadProject", body=body,
              headers={"Content-Type": "multipart/form-data; boundary=" + boundary,
                       "Content-Length": str(len(body))})
    r = c.getresponse()
    r.read()
    c.close()
    return r.status in (200, 302)


def connect_ws(port, tries=60):
    for _ in range(tries):
        try:
            ws = WSClient("127.0.0.1", port)
            ws.connect()
            return ws
        except (OSError, WSError):
            time.sleep(0.5)
    return None


def functions(ws):
    msg = ws.request("QLC+API|getFunctionsList", expect_prefix="QLC+API|getFunctionsList", wait=10.0)
    if not msg:
        return []
    f = msg.split("|")[2:]
    res = []
    for i in range(0, len(f) - 1, 2):
        if re.fullmatch(r"\d+", f[i]):
            res.append((int(f[i]), f[i + 1]))
    typed = []
    for fid, name in res:
        t = ws.request("QLC+API|getFunctionType|%d" % fid,
                       expect_prefix="QLC+API|getFunctionType", wait=3.0) or ""
        typed.append((fid, name, t.split("|")[-1] if t else "?"))
    return typed


def ps(pid):
    try:
        out = subprocess.check_output(["ps", "-o", "pcpu=,rss=", "-p", str(pid)],
                                      stderr=subprocess.DEVNULL, text=True).split()
        th = subprocess.check_output(["ps", "-M", "-p", str(pid)],
                                     stderr=subprocess.DEVNULL, text=True).count("\n") - 1
        return float(out[0]), float(out[1]) / 1024.0, th
    except (subprocess.CalledProcessError, IndexError, ValueError):
        return None, None, None


class TickLog:
    """What the engine's tick log said since last asked."""
    OVER = re.compile(r"\[TICKLOG\] tick compute ([\d.]+) ms > budget")
    SUMMARY = re.compile(r"\[TICKLOG\] (\d+) ticks: mean ([\d.]+) ms, max ([\d.]+) ms.*overruns (\d+) \(([\d.]+)%\)")

    def __init__(self, path):
        self.path = path
        self.pos = 0

    def read(self):
        over, worst, last = 0, 0.0, None
        try:
            with open(self.path, "r", errors="replace") as f:
                f.seek(self.pos)
                for ln in f:
                    m = self.OVER.search(ln)
                    if m:
                        over += 1
                        worst = max(worst, float(m.group(1)))
                        continue
                    m = self.SUMMARY.search(ln)
                    if m:
                        last = (float(m.group(2)), float(m.group(3)), float(m.group(5)))
                self.pos = f.tell()
        except FileNotFoundError:
            pass
        return over, worst, last


def newest_crash(since):
    pats = glob.glob(os.path.expanduser("~/Library/Logs/DiagnosticReports/qlcconsole*.ips"))
    return [p for p in pats if os.path.getmtime(p) >= since]


# ---- what the show is put through -------------------------------------------

class Driver:
    def __init__(self, ws, funcs, rng, phase):
        self.ws, self.rng, self.phase = ws, rng, phase
        real = [f for f in funcs if not f[1].startswith("STRESS")]
        self.block = [f[0] for f in funcs if f[1].startswith("STRESS") and f[2] in ("RGBMatrix", "Chaser")]
        self.looks = [f[0] for f in real if f[2] in ("Collection", "Scene")] or [f[0] for f in real]
        self.chasers = [f[0] for f in real if f[2] in ("Chaser", "RGBMatrix")]
        self.shows = [f[0] for f in real if f[2] == "Show"]
        self.all = [f[0] for f in funcs]
        self.on = set()
        self.next_look = 0
        self.next_rough = 0
        self.next_show = time.time() + 1800

    def set(self, fid, on):
        self.ws.send("QLC+API|setFunctionStatus|%d|%d" % (fid, 1 if on else 0))
        (self.on.add if on else self.on.discard)(fid)

    def start_block(self):
        for fid in self.block:
            self.set(fid, True)
        return "block running: %d matrices/chasers" % len(self.block)

    def tick(self):
        """Called every few seconds: what to do next. Returns a note or None."""
        t = time.time()
        note = None
        if t >= self.next_look:
            for fid in list(self.on):
                if fid not in self.block:
                    self.set(fid, False)
            picks = self.rng.sample(self.looks, min(len(self.looks), self.rng.randint(1, 3)))
            if self.chasers and self.rng.random() < 0.6:
                picks.append(self.rng.choice(self.chasers))
            for fid in picks:
                self.set(fid, True)
            self.next_look = t + self.rng.randint(60, 120)
            note = "looks %s" % picks
        if self.shows and t >= self.next_show:
            self.set(self.rng.choice(self.shows), True)
            self.next_show = t + 1800
            note = "show started"
        if self.phase == "rough" and t >= self.next_rough:
            kind = self.rng.choice(["churn", "gm", "allonoff"])
            if kind == "churn":
                for _ in range(60):
                    self.set(self.rng.choice(self.all), self.rng.random() < 0.5)
                    time.sleep(0.05)
            elif kind == "gm":
                for v in list(range(255, -1, -5)) + list(range(0, 256, 5)):
                    self.ws.send("GM_VALUE|%d" % v)
                    time.sleep(0.2)            # a 20 s sweep: no strobing the rig
            else:
                for fid in self.all:
                    self.set(fid, True)
                time.sleep(20)
                for fid in self.all:
                    self.set(fid, False)
            self.next_rough = t + self.rng.randint(240, 420)
            note = "rough: %s" % kind
        return note


# ---- one phase ----------------------------------------------------------------

def run_phase(run, name, show, hours, port, ticklog, sample_s, rng):
    phase_dir = os.path.join(run.out, name)
    os.makedirs(phase_dir, exist_ok=True)
    end = time.time() + hours * 3600
    started = time.time()
    stats = {"phase": name, "show": os.path.basename(show), "crashes": 0, "hangs": 0,
             "restarts": 0, "reloads": 0, "samples": 0, "ticks_over": 0, "worst_tick_ms": 0.0,
             "rss_first": None, "rss_peak": 0.0, "rss_last": None, "cpu": []}
    launch = 0
    while not STOP and time.time() < end:
        launch += 1
        log_path = os.path.join(phase_dir, "app-%d.log" % launch)
        t0 = time.time()
        pid = start_app(port, log_path, ticklog)
        if pid is None or not wait_http(port):
            run.event(name, "app did not come up (launch %d)" % launch)
            stop_app(pid)
            time.sleep(10)
            continue
        if not load_project(port, show):
            run.event(name, "loadProject failed")
        time.sleep(15)                         # a big show takes a while to load
        ws = connect_ws(port)
        funcs = functions(ws) if ws else []
        run.event(name, "pid %d up, %d functions (launch %d)" % (pid, len(funcs), launch))
        drv = Driver(ws, funcs, rng, name)
        if name == "scaled":
            run.event(name, drv.start_block())
        tl = TickLog(log_path)
        misses, next_sample, next_reload = 0, time.time(), time.time() + 3600
        while not STOP and time.time() < end:
            if pid not in app_pids():
                stats["crashes"] += 1
                run.event(name, "CRASH: pid %d gone; reports: %s" % (pid, newest_crash(t0)))
                break
            try:
                note = drv.tick()
                if note:
                    run.event(name, note)
                if name == "rough" and time.time() >= next_reload:
                    load_project(port, show)
                    stats["reloads"] += 1
                    time.sleep(15)
                    ws = connect_ws(port) or ws
                    drv = Driver(ws, functions(ws), rng, name)
                    next_reload = time.time() + 3600
                    run.event(name, "reloaded the project")
            except (OSError, WSError) as e:
                run.event(name, "websocket: %s; reconnecting" % e)
                ws = connect_ws(port, tries=20)
                if ws:
                    drv.ws = ws
            if time.time() >= next_sample:
                next_sample += sample_s
                cpu, rss, th = ps(pid)
                answers = bool(ws and ws.request("QLC+API|getFunctionsNumber",
                                                 expect_prefix="QLC+API|getFunctionsNumber", wait=5.0))
                misses = 0 if answers else misses + 1
                over, worst, summ = tl.read()
                stats["samples"] += 1
                stats["ticks_over"] += over
                stats["worst_tick_ms"] = max(stats["worst_tick_ms"], worst)
                if rss is not None:
                    stats["rss_first"] = stats["rss_first"] or rss
                    stats["rss_peak"] = max(stats["rss_peak"], rss)
                    stats["rss_last"] = rss
                    stats["cpu"].append(cpu)
                run.csv.writerow([now(), name, int(time.time() - started), pid, cpu,
                                  None if rss is None else round(rss, 1), th, int(answers), over,
                                  summ[0] if summ else "", summ[1] if summ else "",
                                  summ[2] if summ else "", len(drv.on)])
                if misses >= 3:
                    stats["hangs"] += 1
                    run.event(name, "HANG: no answer for 3 samples; sampling the stack")
                    subprocess.run(["sample", str(pid), "5", "-file",
                                    os.path.join(phase_dir, "hang-%d.txt" % int(time.time()))],
                                   capture_output=True)
                    stop_app(pid)
                    break
            time.sleep(3)
        stop_app(pid)
        if os.path.exists(UNTITLED_AUTOSAVE):
            os.remove(UNTITLED_AUTOSAVE)        # this run's, never the user's (set aside)
        if not STOP and time.time() < end:
            stats["restarts"] += 1
            time.sleep(10)
    for p in glob.glob(os.path.join(phase_dir, "app-*.log")):
        with open(p, "rb") as src, gzip.open(p + ".gz", "wb") as dst:
            shutil.copyfileobj(src, dst)
        os.remove(p)
    cpu = stats.pop("cpu")
    stats["cpu_mean"] = round(sum(cpu) / len(cpu), 1) if cpu else None
    stats["hours"] = round((time.time() - started) / 3600, 2)
    run.event(name, "phase done: %s" % json.dumps(stats))
    return stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--show", required=True)
    ap.add_argument("--scaled-show", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--hours", type=float, default=8.0, help="per phase")
    ap.add_argument("--phases", default="real,scaled,rough")
    ap.add_argument("--port", type=int, default=9999)
    ap.add_argument("--ticklog", type=int, default=500)
    ap.add_argument("--sample", type=int, default=30)
    ap.add_argument("--seed", type=int, default=24)
    a = ap.parse_args()

    if app_pids():
        sys.exit("qlcconsole is already running: quit it first (this run starts its own)")

    def stop(*_):
        global STOP
        STOP = True
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    run = Run(a.out)
    rng = random.Random(a.seed)
    # Unsaved work the user already had: kept out of the way, put back after.
    kept = os.path.join(a.out, "untitled.qxw.autosave.users")
    if os.path.exists(UNTITLED_AUTOSAVE):
        shutil.move(UNTITLED_AUTOSAVE, kept)
        run.event("soak", "set aside the existing untitled autosave: %s" % kept)
    shows = {"real": a.show, "scaled": a.scaled_show, "rough": a.show}
    run.event("soak", "start: phases %s, %.1f h each" % (a.phases, a.hours))
    results = []
    for name in a.phases.split(","):
        if STOP:
            break
        results.append(run_phase(run, name, shows[name], a.hours, a.port, a.ticklog, a.sample, rng))
    json.dump(results, open(os.path.join(a.out, "summary.json"), "w"), indent=2)
    if os.path.exists(kept):
        shutil.move(kept, UNTITLED_AUTOSAVE)
        run.event("soak", "put the untitled autosave back")
    run.event("soak", "finished%s" % (" (stopped early)" if STOP else ""))


if __name__ == "__main__":
    main()
