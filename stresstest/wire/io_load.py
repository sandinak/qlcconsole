#!/usr/bin/env python3
"""
The full app under input load, its Art-Net output captured on the wire.
LOOPBACK ONLY: every packet goes to 127.0.0.1 (on Linux run it inside
`unshare -n`, where lo is the only interface; see run_io.sh).

The app is started offscreen with a sandbox HOME, QLC_IOPLUGIN_DIR holding
only the Art-Net and OSC plugins, and QLC_TICKLOG. A synthetic show (from
qlcstress emit: 32 universes x 170 RGB, the 'mixed' load) gets 24 extra
universes: 32..47 take Art-Net input, 48..55 OSC input. Universes 0..31 are
patched to Art-Net output on the 127.0.0.1 line (Art-Net universes 0..31).

Phases (each --phase-sec): idle (functions running, no input), artnet4,
artnet16 (that many universes of ArtDMX at 44 fps, every channel changing
every frame), osc (OSC messages at --osc-rate/s), web (--web-clients
WebSocket clients + HTTP GETs), all (artnet16 + osc + web).

Per phase: process CPU%, RSS, ticks over budget (the engine's tick log),
WebSocket round trip p50/p99, inputs sent. The capture (pcap) is cut into
the same phases by wire_stats.py (phases.json).

  io_load.py --exe APP --qlcstress Q --resources R --out DIR [--mac-capture]
"""
import argparse
import http.client
import json
import os
import random
import re
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "driver"))
from wsclient import WSClient, WSError  # noqa: E402

MAC = sys.platform == "darwin"
OVER = re.compile(r"\[TICKLOG\] tick compute ([\d.]+) ms > budget")


def log(msg):
    print("%s %s" % (time.strftime("%H:%M:%S"), msg), flush=True)


def make_show(a, path):
    cmd = [a.qlcstress, "emit", path, "--universes", "32", "--fixtures-per-uni", "170",
           "--scenes", "32", "--chasers", "8", "--matrices", "8", "--efx", "8", "--collections", "0",
           "--cuelists", "4", "--scene-fixtures", "170", "--efx-per-universe",
           "--fixtures-dir", os.path.join(a.resources, "fixtures"),
           "--scripts-dir", os.path.join(a.resources, "rgbscripts")]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    # Patches go in the show itself (by line name, UID="127.0.0.1"), not
    # through QLC+IO: that one also saves them as the app's I/O defaults,
    # i.e. into its settings -- which on macOS are the user's own (cfprefsd
    # ignores a sandbox HOME).
    x = open(path).read()
    if getattr(a, "no_io_patch", False):
        return
    def patched(m):
        u = int(m.group(1))
        return ('<Universe Name="Universe %d" ID="%d">\n    <Output Plugin="ArtNet" UID="127.0.0.1" Line="0"/>\n'
                '   </Universe>\n' % (u + 1, u))
    x, n = re.subn(r'<Universe Name="[^"]*" ID="(\d+)"/>\n', patched, x)
    assert n == 32, n
    at = x.index("</InputOutputMap>")
    at = x.rindex("\n", 0, at) + 1
    extra = ""
    for u in range(32, 56):
        plug = "ArtNet" if u < 48 else "OSC"
        extra += ('   <Universe Name="IN %d" ID="%d">\n    <Input Plugin="%s" UID="127.0.0.1" Line="0"/>\n'
                  '   </Universe>\n' % (u, u, plug))
    x = x[:at] + extra + x[at:]
    open(path, "w").write(x)


def wait_http(port, tries=480):
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


def http_get(port, path, timeout=30):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    c.request("GET", path)
    r = c.getresponse()
    return r.status, r.read()


def load_project(port, path):
    xml = open(path, "rb").read()
    b = "----ioloadboundary"
    body = (("--%s\r\nContent-Disposition: form-data; name=\"qlc\"; filename=\"project.qxw\"\r\n\r\n" % b)
            .encode() + xml + ("\n\r\n--%s--\r\n" % b).encode())
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=900)
    c.request("POST", "/loadProject", body=body,
              headers={"Content-Type": "multipart/form-data; boundary=" + b,
                       "Content-Length": str(len(body))})
    r = c.getresponse()
    r.read()
    return r.status in (200, 302)


def ps(pid):
    try:
        out = subprocess.check_output(["ps", "-o", "rss=", "-p", str(pid)], text=True).split()
        rss = float(out[0]) / 1024.0
    except (subprocess.CalledProcessError, IndexError, ValueError):
        return None, None
    if MAC:
        try:
            o = subprocess.check_output(["ps", "-o", "time=", "-p", str(pid)], text=True).strip()
            p = [float(x) for x in o.replace("-", ":").split(":")]
            secs = 0.0
            for x in p:
                secs = secs * 60 + x
            return secs, rss
        except (subprocess.CalledProcessError, ValueError):
            return None, rss
    with open("/proc/%d/stat" % pid) as f:
        st = f.read().rsplit(")", 1)[1].split()
    return (int(st[11]) + int(st[12])) / os.sysconf("SC_CLK_TCK"), rss


class Loads:
    def __init__(self, port):
        self.port = port
        self.artnet_unis = 0
        self.osc_rate = 0
        self.web_clients = 0
        self.stop = False
        self.sent_artnet = 0
        self.sent_osc = 0
        self.web_rtts = []
        self.web_errors = 0
        self.lock = threading.Lock()

    def artnet(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        seq = 1
        nxt = time.time()
        while not self.stop:
            n = self.artnet_unis
            if n:
                for u in range(32, 32 + n):
                    data = os.urandom(512)
                    pkt = b"Art-Net\x00" + struct.pack("<H", 0x5000) + struct.pack("!H", 14) \
                        + bytes([seq, 0]) + struct.pack("<H", u) + struct.pack("!H", 512) + data
                    s.sendto(pkt, ("127.0.0.1", 6454))
                    self.sent_artnet += 1
                seq = 1 if seq == 255 else seq + 1
            nxt += 1.0 / 44.0
            d = nxt - time.time()
            if d > 0:
                time.sleep(d)
            else:
                nxt = time.time()

    def osc(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rnd = random.Random(7)
        while not self.stop:
            r = self.osc_rate
            if not r:
                time.sleep(0.05)
                continue
            t0 = time.time()
            batch = max(1, r // 100)
            for _ in range(batch):
                u = rnd.randrange(48, 56)
                path = ("/qlc/fader%d" % rnd.randrange(64)).encode()
                path += b"\x00" * (4 - len(path) % 4)
                msg = path + b",f\x00\x00" + struct.pack("!f", rnd.random())
                s.sendto(msg, ("127.0.0.1", 7700 + u))
                self.sent_osc += 1
            d = 0.01 - (time.time() - t0)
            if d > 0:
                time.sleep(d)

    def web(self, idx):
        ws = None
        n = 0
        while not self.stop:
            if idx >= self.web_clients:
                if ws:
                    ws.close()
                    ws = None
                time.sleep(0.1)
                continue
            try:
                if ws is None:
                    ws = WSClient("127.0.0.1", self.port, timeout=10)
                    ws.connect()
                t0 = time.time()
                r = ws.request("QLC+API|getFunctionsNumber", expect_prefix="QLC+API|getFunctionsNumber", wait=10)
                rtt = (time.time() - t0) * 1000.0
                with self.lock:
                    if r:
                        self.web_rtts.append(rtt)
                    else:
                        self.web_errors += 1
                n += 1
                if n % 10 == 0:
                    ws.request("QLC+API|getFunctionsList", expect_prefix="QLC+API|getFunctionsList", wait=10)
                if n % 25 == 0:
                    http_get(self.port, "/", timeout=10)
                time.sleep(0.02)
            except (OSError, WSError, http.client.HTTPException):
                with self.lock:
                    self.web_errors += 1
                if ws:
                    try:
                        ws.close()
                    except OSError:
                        pass
                ws = None
                time.sleep(0.5)


def start_capture(kind, path):
    if kind == "mac":
        cmd = ["sudo", "-n", "tcpdump", "-i", "lo0", "-n", "-B", "65536", "-w", path, "udp port 6454"]
    elif kind == "linux":
        cmd = ["sudo", "-n", sys.executable, os.path.join(HERE, "sniff_lo.py"), path, "--ports", "6454"]
    else:
        return None
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(path + ".log", "w"))
    time.sleep(2)
    return p


def stop_capture(p):
    if p is None:
        return
    # the capture runs as root (sudo): signal it through sudo
    subprocess.run(["sudo", "-n", "pkill", "-INT", "-P", str(p.pid)])
    subprocess.run(["sudo", "-n", "kill", "-INT", str(p.pid)])
    try:
        p.wait(15)
    except subprocess.TimeoutExpired:
        subprocess.run(["sudo", "-n", "kill", "-KILL", str(p.pid)])


def collect_crash(a, app, since, result):
    """A crash: the macOS crash report, or a backtrace from the core (gdb)."""
    import glob
    import shutil
    found = []
    if MAC:
        time.sleep(5)
        for p in glob.glob(os.path.expanduser("~/Library/Logs/DiagnosticReports/qlcconsole*.ips")):
            if os.path.getmtime(p) >= since:
                shutil.copy(p, a.out)
                found.append(os.path.basename(p))
    else:
        for core in glob.glob(os.path.join(a.out, "core*")):
            bt = os.path.join(a.out, "crash-backtrace.txt")
            r = subprocess.run(["gdb", "-batch", "-ex", "thread apply all bt 30", a.exe, core],
                               capture_output=True, text=True, timeout=600)
            open(bt, "w").write(r.stdout + r.stderr)
            found += [os.path.basename(core), os.path.basename(bt)]
    result["crash_artifacts"] = found
    log("CRASH (exit %s): %s" % (app.returncode, found or "no core / report"))


def mac_footprint(pid):
    try:
        out = subprocess.check_output(["footprint", "-f", "bytes", "-p", str(pid)],
                                      stderr=subprocess.DEVNULL, text=True, timeout=60)
        mo = re.search(r"Footprint:\s*([\d.]+)\s*(B|KB|MB|GB)", out)
        if mo:
            return float(mo.group(1)) / {"B": 1048576.0, "KB": 1024.0, "MB": 1.0, "GB": 1 / 1024.0}[mo.group(2)]
    except (OSError, subprocess.SubprocessError, ValueError):
        pass
    return None


def pct(v, p):
    if not v:
        return None
    v = sorted(v)
    return round(v[max(0, min(len(v) - 1, int(-(-p / 100.0 * len(v) // 1)) - 1))], 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--qlcstress", required=True)
    ap.add_argument("--resources", required=True)
    ap.add_argument("--plugins", required=True, help="dir with only the artnet + osc plugins")
    ap.add_argument("--home", required=True, help="sandbox HOME for the app")
    ap.add_argument("--out", required=True)
    ap.add_argument("--port", type=int, default=9871)
    ap.add_argument("--phase-sec", type=int, default=180)
    ap.add_argument("--settle-sec", type=int, default=60)
    ap.add_argument("--osc-rate", type=int, default=2000)
    ap.add_argument("--web-clients", type=int, default=8)
    ap.add_argument("--plan", default="",
                    help="phases as name:artnetUniverses:oscRate:webClients,... (default: the standard six)")
    ap.add_argument("--no-io-patch", action="store_true", help="the show without any output/input patch")
    ap.add_argument("--capture", choices=("none", "mac", "linux"), default="none",
                    help="mac: sudo tcpdump on lo0; linux: sudo sniff_lo.py on lo (inside unshare -n)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    show = os.path.join(a.out, "io-show.qxw")
    make_show(a, show)
    log("show: %s" % show)

    cap = start_capture(a.capture, os.path.join(a.out, "wire.pcap"))

    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", HOME=a.home, CFFIXED_USER_HOME=a.home,
               QLC_IOPLUGIN_DIR=a.plugins, QLC_TICKLOG="500", QLC_NO_RECOVERY_PROMPT="1",
               QT_FORCE_STDERR_LOGGING="1")
    applog = open(os.path.join(a.out, "app.log"), "wb")
    def allow_core():
        # a crash leaves a core in the run folder (Linux core_pattern "core")
        import resource
        try:
            resource.setrlimit(resource.RLIMIT_CORE, (resource.getrlimit(resource.RLIMIT_CORE)[1],) * 2)
        except (ValueError, OSError):
            pass
    t_launch = time.time()
    app = subprocess.Popen([a.exe, "-d", "1", "-w", "-wp", str(a.port)], env=env, cwd=a.out,
                           stdin=subprocess.DEVNULL, stdout=applog, stderr=subprocess.STDOUT,
                           start_new_session=True, preexec_fn=allow_core)
    phases = []
    result = {"host": socket.gethostname().split(".")[0], "phases": []}
    loads = Loads(a.port)
    threads = []
    try:
        if not wait_http(a.port):
            raise SystemExit("app did not come up")
        t0 = time.time()
        ok = load_project(a.port, show)
        log("loadProject %s in %.0f s" % (ok, time.time() - t0))
        ws = WSClient("127.0.0.1", a.port, timeout=30)
        ws.connect()
        for _ in range(120):
            r = ws.request("QLC+API|getFunctionsNumber", expect_prefix="QLC+API|getFunctionsNumber", wait=10)
            if r and int(r.split("|")[-1] or 0) > 0:
                break
            time.sleep(1)
        st, html = http_get(a.port, "/config")
        html = html.decode("utf-8", "replace")
        def line_of(plugin):
            for m in re.finditer(r'<option value="%s\|(\d+)"[^>]*>([^<]*)<' % plugin, html):
                if "127.0.0.1" in m.group(2):
                    return int(m.group(1))
            return None
        an, osc = line_of("ArtNet"), line_of("OSC")
        log("lines on 127.0.0.1: ArtNet %s, OSC %s" % (an, osc))
        result["artnet_line"], result["osc_line"] = an, osc
        if an is None:
            log("WARNING: no ArtNet line named 127.0.0.1 on /config")
        # what the page says is patched (the show carries the patches)
        result["config_selected"] = re.findall(r'<option value="((?:ArtNet|OSC)\|\d+)"[^>]*selected', html)[:8]
        msg = ws.request("QLC+API|getFunctionsList", expect_prefix="QLC+API|getFunctionsList", wait=30)
        f = msg.split("|")[2:] if msg else []
        started = 0
        for i in range(0, len(f) - 1, 2):
            name = f[i + 1]
            if re.fullmatch(r"(Scene|Chaser|Matrix|EFX|CueList|UniverseWave) \d+", name):
                ws.send("QLC+API|setFunctionStatus|%s|1" % f[i])
                started += 1
        log("started %d functions" % started)
        result["started"] = started
        ws.close()
        for fn in (loads.artnet, loads.osc):
            th = threading.Thread(target=fn, daemon=True)
            th.start()
            threads.append(th)
        for i in range(max(a.web_clients, 1)):
            th = threading.Thread(target=loads.web, args=(i,), daemon=True)
            th.start()
            threads.append(th)
        time.sleep(a.settle_sec)
        logpos = os.path.getsize(os.path.join(a.out, "app.log"))
        plan = [("idle", 0, 0, 0), ("artnet4", 4, 0, 0), ("artnet16", 16, 0, 0),
                ("osc", 0, a.osc_rate, 0), ("web", 0, 0, a.web_clients),
                ("all", 16, a.osc_rate, a.web_clients)]
        if a.plan:
            plan = []
            for item in a.plan.split(","):
                n_, au_, or_, wc_ = item.split(":")
                plan.append((n_, int(au_), int(or_), int(wc_)))
        memf = open(os.path.join(a.out, "mem.csv"), "w", buffering=1)
        memf.write("t_s,phase,rss_mb,footprint_mb\n")
        t_mem0 = time.time()
        for name, au, orate, wc in plan:
            loads.artnet_unis, loads.osc_rate, loads.web_clients = au, orate, wc
            time.sleep(3)
            with loads.lock:
                loads.web_rtts, loads.web_errors = [], 0
            sa, so = loads.sent_artnet, loads.sent_osc
            # a probe client of our own: latency of the idle-phase too
            probe = None
            for _try in range(10):
                try:
                    probe = WSClient("127.0.0.1", a.port, timeout=10)
                    probe.connect()
                    break
                except (OSError, WSError):
                    probe = None
                    time.sleep(1)
            rtts, cpus = [], []
            probe_fail = 0
            c0, _ = ps(app.pid)
            tp0 = time.time()
            rss = []
            last_c, last_t = c0, tp0
            while time.time() < tp0 + a.phase_sec - 1.0:
                t = time.time()
                try:
                    r = probe.request("QLC+API|getFunctionsNumber", expect_prefix="QLC+API|getFunctionsNumber",
                                      wait=10) if probe else None
                except (OSError, WSError):
                    r, probe = None, None
                if r:
                    rtts.append((time.time() - t) * 1000.0)
                else:
                    probe_fail += 1
                time.sleep(1.0)
                c, m = ps(app.pid)
                if c is not None:
                    cpus.append(100.0 * (c - last_c) / (time.time() - last_t))
                    last_c, last_t = c, time.time()
                if m:
                    rss.append(m)
                if int(time.time() - tp0) % 10 == 0:
                    fp = mac_footprint(app.pid) if MAC else None
                    memf.write("%.1f,%s,%s,%s\n" % (time.time() - t_mem0, name, m, "" if fp is None else round(fp, 1)))
                if app.poll() is not None:
                    result["crashed_in_phase"] = name
                    raise SystemExit("app died: %s (phase %s)" % (app.returncode, name))
            tp1 = time.time()
            c1, _ = ps(app.pid)
            if probe:
                probe.close()
            over, worst = 0, 0.0
            with open(os.path.join(a.out, "app.log"), "r", errors="replace") as lf:
                lf.seek(logpos)
                for ln in lf:
                    mo = OVER.search(ln)
                    if mo:
                        over += 1
                        worst = max(worst, float(mo.group(1)))
                logpos = lf.tell()
            with loads.lock:
                wr = list(loads.web_rtts)
                we = loads.web_errors
            ph = {"name": name, "t0": tp0, "t1": tp1, "artnet_unis": au, "osc_rate": orate,
                  "web_clients": wc, "cpu_pct": round(100.0 * (c1 - c0) / (tp1 - tp0), 1) if c1 and c0 else None,
                  "cpu_pct_p95": pct(cpus, 95), "rss_mb_end": rss[-1] if rss else None,
                  "rss_mb_max": max(rss) if rss else None,
                  "ticks_over": over, "ticks_over_per_hour": round(over * 3600.0 / (tp1 - tp0), 1),
                  "worst_over_ms": worst,
                  "probe_rtt_p50": pct(rtts, 50), "probe_rtt_p99": pct(rtts, 99), "probe_rtt_max": pct(rtts, 100), "probe_fail": probe_fail,
                  "web_req": len(wr), "web_req_per_s": round(len(wr) / (tp1 - tp0), 1),
                  "web_rtt_p50": pct(wr, 50), "web_rtt_p99": pct(wr, 99), "web_errors": we,
                  "artnet_pkts": loads.sent_artnet - sa, "osc_msgs": loads.sent_osc - so}
            result["phases"].append(ph)
            phases.append({"name": name, "t0": tp0, "t1": tp1})
            log(json.dumps(ph))
    finally:
        loads.stop = True
        time.sleep(0.5)
        app.terminate()
        try:
            app.wait(20)
        except subprocess.TimeoutExpired:
            app.kill()
        result["app_exit"] = app.returncode
        if app.returncode is not None and app.returncode < 0 and app.returncode != -15:
            collect_crash(a, app, t_launch, result)
        stop_capture(cap)
        json.dump(phases, open(os.path.join(a.out, "phases.json"), "w"), indent=1)
        json.dump(result, open(os.path.join(a.out, "io_result.json"), "w"), indent=1)
        log("done, app exit %s" % app.returncode)


if __name__ == "__main__":
    main()
