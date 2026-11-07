#!/usr/bin/env python3
"""The UX walker's driver (docs/dev/UX_WALKER.md): fans the walk out over
cores and Macs, then merges it into one report.

Every (show x Settings ▸ Units) pair is independent, and inside a pair the
walk splits by screen group -- tabs, menus, menu items' dialogs ("actions"),
the screen registry ("screens"), right-click menus ("context"), Show Tree
pages, effect pickers, the command line, and once the core flows of
docs/dev/ux_flows.json on their own show -- with the big groups split again
round-robin (--shard I/N). Each shard is one uxwalk process with its own
settings and HOME. A queue of shards is served by workers: this Mac's (as
many as --jobs), and each pool host's once its build of the same commit is
ready (--hosts). A crash or hang costs one screen, not the shard: the
driver notes it (the stack goes into the report) and starts the shard again
past that screen.

    tools/uxwalk/run.py --show A.qlcc --show B.qlcc --units feet,mm --out DIR
                        [--binary build-uxwalk/tools/uxwalk/uxwalk]
                        [--jobs N] [--hosts brm,dink | --hosts auto] [--no-local]
                        [--rev SHA] [--no-flows | --flows-only]

Remote hosts come from platforms/macos/gate-hosts.conf (or $GATE_HOSTS).
On each: the gate pool's lock (~/gates/.gatelock via gate-lock.sh, held
only for the walk), the commit's tree synced into ~/uxwalk/src (rsync by
checksum, so a rebuild is incremental), built in ~/uxwalk/build with
-DQLC_UXWALK=ON, the shows copied to ~/uxwalk/shows, results copied back.
"""

import argparse
import json
import os
import queue
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

# The shards of one pair: (groups, number of round-robin pieces). Sized from
# a DanceHysteria walk: each piece about the same wall time.
SHARDS = [
    ("tabs", 2),
    ("menus,context,showtree,pickers,cmdline", 1),
    ("actions", 3),
    ("contextdialogs", 3),
    ("screens", 4),
]

SHARD_TIMEOUT = 900       # s: a shard that runs past this is stuck
RESTARTS = 8              # crashes or hangs survived per shard


def log(msg):
    print("[%s] %s" % (time.strftime("%H:%M:%S"), msg), flush=True)


def perf_cores():
    for k in ("hw.perflevel0.logicalcpu", "hw.ncpu"):
        try:
            return int(subprocess.check_output(["sysctl", "-n", k], text=True).strip())
        except (OSError, subprocess.CalledProcessError, ValueError):
            pass
    return os.cpu_count() or 4


class Shard:
    def __init__(self, show, units, groups, index, count):
        self.show = show            # local path
        self.units = units
        self.groups = groups
        self.index = index
        self.count = count
        self.skip = 0
        self.restarts = 0

    def label(self):
        return "%s/%s %s %d/%d" % (os.path.basename(self.show), self.units, self.groups,
                                   self.index, self.count)

    def args(self, binary, show, out):
        return [binary, "--show", show, "--units", self.units, "--out", out,
                "--groups", self.groups, "--shard", "%d/%d" % (self.index, self.count),
                "--skip", str(self.skip)]


def write_note(out, shard, name, rule, text):
    d = os.path.join(out, "screens")
    os.makedirs(d, exist_ok=True)
    tag = "%s-%s" % (os.path.splitext(os.path.basename(shard.show))[0], shard.units)
    ident = "%s__%s__%s_%s" % (tag, rule, re.sub(r"[^A-Za-z0-9_.-]+", "_", name)[:80], int(time.time() * 1000))
    rec = {"id": ident, "show": os.path.basename(shard.show), "units": shard.units, "tag": tag,
           "group": name.split("|")[0], "name": name, "kind": "note", "rule": rule, "text": text}
    with open(os.path.join(d, ident + ".json"), "w", encoding="utf-8") as fh:
        json.dump(rec, fh, indent=1, ensure_ascii=False)


def after_run(out, shard, rc, stdout, stderr, timed_out):
    """Work out where a shard stopped; True: run it again past there."""
    at = re.findall(r"^uxwalk-at: (\d+) (.*)$", stdout, re.M)
    done = re.search(r"^uxwalk-shard: .*$", stdout, re.M)
    if rc == 0 and done and not timed_out:
        log("done  %s: %s" % (shard.label(), done.group(0).split(": ", 1)[1]))
        return False
    last_n, last_name = (int(at[-1][0]), at[-1][1]) if at else (shard.skip, "start")
    stack = ""
    m = re.search(r"uxwalk-crash: stack follows\n((?:.*\n){0,25})", stderr)
    if m:
        stack = m.group(1)
    what = "hung (killed after %d s)" % SHARD_TIMEOUT if timed_out else "crashed (exit %s)" % rc
    log("FAIL  %s: %s at %s" % (shard.label(), what, last_name))
    write_note(out, shard, last_name, "walker-crash",
               "%s at this step%s" % (what, ("\n" + stack) if stack else
                                       ("\n" + "\n".join(stderr.strip().splitlines()[-12:]))))
    shard.restarts += 1
    if shard.restarts > RESTARTS or not at:
        return False
    shard.skip = last_n             # past the one it died on
    return True


# ---------------------------------------------------------------------------
# Workers
# ---------------------------------------------------------------------------

class LocalWorker(threading.Thread):
    def __init__(self, q, binary, out, name):
        super().__init__(daemon=True)
        self.q, self.binary, self.out, self.name_ = q, binary, out, name

    def run(self):
        while True:
            try:
                shard = self.q.get_nowait()
            except queue.Empty:
                return
            while True:
                env = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           TMPDIR=tempfile.mkdtemp(prefix="uxwalk-"))
                cmd = shard.args(self.binary, shard.show, self.out)
                log("start %s on %s" % (shard.label(), self.name_))
                timed_out = False
                try:
                    p = subprocess.run(["nice", "-n", "10"] + cmd, capture_output=True, text=True,
                                       env=env, timeout=SHARD_TIMEOUT, errors="replace")
                    rc, so, se = p.returncode, p.stdout, p.stderr
                except subprocess.TimeoutExpired as e:
                    timed_out, rc = True, -9
                    so = e.stdout.decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
                    se = e.stderr.decode(errors="replace") if isinstance(e.stderr, bytes) else (e.stderr or "")
                shutil.rmtree(env["TMPDIR"], ignore_errors=True)
                if not after_run(self.out, shard, rc, so, se, timed_out):
                    break
            self.q.task_done()


def read_hosts(conf):
    hosts = {}
    with open(conf) as fh:
        for line in fh:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 4:
                hosts[f[0]] = {"name": f[0], "dest": f[1], "qt": f[2],
                               "path": "" if f[3] == "-" else f[3]}
    return hosts


class Remote:
    """One pool host: its lock, its build, then its workers on the queue."""

    def __init__(self, host, rev, srcdir, shows, out, q):
        self.h, self.rev, self.srcdir, self.shows, self.out, self.q = host, rev, srcdir, shows, out, q
        self.id = "uxwalk-%s-%s-%d" % (rev[:12], time.strftime("%Y%m%d-%H%M%S"), os.getpid())
        self.run_dir = "uxwalk/runs/%s" % self.id
        self.ok = False
        self.finished = False

    def ssh(self, script, check=True, timeout=None, stdin=None):
        pre = "export PATH=%s/usr/local/bin:/opt/homebrew/bin:$PATH; " % (
            (self.h["path"] + ":") if self.h["path"] else "")
        p = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", self.h["dest"],
                            "bash -s"], input=(stdin or "") + pre + script, capture_output=True,
                           text=True, timeout=timeout, errors="replace")
        if check and p.returncode != 0:
            raise RuntimeError("%s: %s" % (self.h["name"], (p.stderr or p.stdout).strip()[-800:]))
        return p.stdout

    def lock_lib(self):
        with open(os.path.join(REPO, "platforms", "macos", "gate-lock.sh")) as fh:
            return fh.read() + "\n"

    def take_lock(self):
        while True:
            out = self.ssh("gl_try '%s' '%s' '%s'\n" % (self.id, self.rev, os.uname().nodename),
                           stdin=self.lock_lib())
            last = out.strip().splitlines()[-1] if out.strip() else ""
            if last == "@@ok":
                return
            log("%s busy (%s): waiting" % (self.h["name"], last.replace("@@busy ", "")))
            time.sleep(30)

    def release_lock(self):
        try:
            self.ssh("kill $(cat ~/gates/%s.pid 2>/dev/null) 2>/dev/null; rm -f ~/gates/%s.pid; gl_done '%s'\n"
                     % (self.id, self.id, self.id), check=False,
                     stdin=self.lock_lib())
        except Exception:               # noqa: BLE001 -- best effort on the way out
            pass

    def prepare(self):
        name = self.h["name"]
        self.take_lock()
        # A .pid that lives as long as this driver keeps the lock ours.
        self.ssh("mkdir -p ~/gates; (sleep %d) >/dev/null 2>&1 & echo $! > ~/gates/%s.pid\n"
                 % (SHARD_TIMEOUT * 3, self.id))
        t = time.time()
        self.ssh("mkdir -p ~/uxwalk/src ~/uxwalk/shows ~/%s/screens" % self.run_dir)
        subprocess.run(["rsync", "-rlc", "--delete", self.srcdir + "/", "%s:uxwalk/src/" % self.h["dest"]],
                       check=True, capture_output=True)
        subprocess.run(["rsync", "-t"] + list(self.shows) + ["%s:uxwalk/shows/" % self.h["dest"]],
                       check=True, capture_output=True)
        qt = self.h["qt"]
        log("%s: building %s" % (name, self.rev[:12]))
        self.ssh(("cd ~/uxwalk && QT=%s && QT=${QT/#\\~/$HOME} && "
                  "{ [ -f build/CMakeCache.txt ] || cmake -S src -B build -DQLC_UXWALK=ON "
                  "-DCMAKE_PREFIX_PATH=$QT/lib/cmake -DCMAKE_C_COMPILER_LAUNCHER=ccache "
                  "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_AUTOMOC_PATH_PREFIX=ON >cfg.log 2>&1; } && "
                  "J=$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu) && "
                  "nice cmake --build build -j$J --target uxwalk > build.log 2>&1 || "
                  "{ tail -30 build.log; exit 1; }") % shlex.quote(qt), timeout=3600)
        cores = int(self.ssh("sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu").strip())
        log("%s: built in %.0f s, %d perf cores" % (name, time.time() - t, cores))
        self.ok = True
        return max(1, cores // 2)

    def run_shard(self, shard):
        show = "uxwalk/shows/" + os.path.basename(shard.show)
        args = shard.args("build/tools/uxwalk/uxwalk", "$HOME/" + show, "$HOME/" + self.run_dir)
        cmd = "cd ~/uxwalk && T=$(mktemp -d) && TMPDIR=$T QT_QPA_PLATFORM=offscreen nice -n 10 " \
              + " ".join(a if a.startswith("$HOME") else shlex.quote(a) for a in args) \
              + "; rc=$?; rm -rf $T; exit $rc"
        timed_out = False
        try:
            p = subprocess.run(["ssh", "-o", "BatchMode=yes", self.h["dest"], cmd], capture_output=True,
                               text=True, timeout=SHARD_TIMEOUT, errors="replace")
            rc, so, se = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired as e:
            timed_out, rc = True, -9
            so = e.stdout.decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
            se = e.stderr.decode(errors="replace") if isinstance(e.stderr, bytes) else (e.stderr or "")
            self.ssh("pkill -f 'uxwalk/build/tools/uxwalk/uxwalk.*%s' || true" % self.run_dir, check=False)
        # Its records, back here.
        subprocess.run(["rsync", "-a", "%s:%s/screens/" % (self.h["dest"], self.run_dir),
                        os.path.join(self.out, "screens") + "/"], capture_output=True)
        return rc, so, se, timed_out

    def serve(self):
        try:
            n = self.prepare()
        except Exception as e:          # noqa: BLE001 -- a host that can't build sits out
            log("%s: not used: %s" % (self.h["name"], str(e)[-400:]))
            self.release_lock()
            self.finished = True
            return
        threads = [threading.Thread(target=self.worker, daemon=True) for _ in range(n)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.ssh("rm -rf ~/%s" % self.run_dir, check=False)
        self.release_lock()
        self.finished = True

    def abandon(self):
        """The walk is over before this host was ready: stop its build."""
        try:
            self.ssh("pkill -f 'cmake --build build .*--target uxwalk' || true; "
                     "pkill -f 'cmake -S src -B build -DQLC_UXWALK' || true; rm -rf ~/%s" % self.run_dir,
                     check=False, timeout=30)
        except Exception:               # noqa: BLE001 -- best effort on the way out
            pass
        self.release_lock()

    def worker(self):
        while True:
            try:
                shard = self.q.get_nowait()
            except queue.Empty:
                return
            while True:
                log("start %s on %s" % (shard.label(), self.h["name"]))
                rc, so, se, to = self.run_shard(shard)
                if not after_run(self.out, shard, rc, so, se, to):
                    break
            self.q.task_done()


def tree_of(rev):
    """The commit's tree in a temporary folder (what the hosts build)."""
    d = tempfile.mkdtemp(prefix="uxwalk-src-")
    a = subprocess.Popen(["git", "-C", REPO, "archive", rev], stdout=subprocess.PIPE)
    subprocess.run(["tar", "-x", "-C", d], stdin=a.stdout, check=True)
    a.wait()
    return d


def main():
    ap = argparse.ArgumentParser(description="Walk qlcconsole's UI over shows and unit settings.")
    ap.add_argument("--show", action="append", default=[])
    ap.add_argument("--units", default="feet,mm")
    ap.add_argument("--out", required=True)
    ap.add_argument("--binary", default=os.path.join(REPO, "build-uxwalk", "tools", "uxwalk", "uxwalk"))
    ap.add_argument("--jobs", type=int, default=0, help="local walkers (default: perf cores / 2)")
    ap.add_argument("--no-local", action="store_true", help="only the remote hosts walk")
    ap.add_argument("--hosts", default="", help="pool hosts by name, comma-separated, or 'auto' (all)")
    ap.add_argument("--hosts-conf", default=os.environ.get("GATE_HOSTS",
                                                         os.path.join(REPO, "platforms", "macos", "gate-hosts.conf")))
    ap.add_argument("--rev", default="HEAD")
    ap.add_argument("--keep", action="store_true", help="keep OUT/screens from an earlier run")
    ap.add_argument("--no-flows", action="store_true", help="leave out the core flows (docs/dev/ux_flows.json)")
    ap.add_argument("--flows-only", action="store_true", help="only the core flows, on their own show")
    a = ap.parse_args()

    start = time.time()
    # The core flows run once, on the show ux_flows.json names, in Feet.
    flows_show = None
    if not a.no_flows:
        try:
            with open(os.path.join(REPO, "docs", "dev", "ux_flows.json"), encoding="utf-8") as fh:
                flows_show = os.path.join(REPO, json.load(fh)["show"])
        except (OSError, ValueError, KeyError) as e:
            log("uxwalk: no flows (%s)" % e)
    if a.flows_only:
        a.show = []
    shows = [os.path.abspath(s) for s in a.show]
    for s in shows:
        if not os.path.exists(s):
            sys.exit("uxwalk: no show %s" % s)
    if not a.keep and os.path.isdir(os.path.join(a.out, "screens")):
        shutil.rmtree(os.path.join(a.out, "screens"))
    os.makedirs(os.path.join(a.out, "screens"), exist_ok=True)

    q = queue.Queue()
    # Biggest first: the screens of the biggest show start at once.
    for groups, n in sorted(SHARDS, key=lambda s: -s[1]):
        for show in sorted(shows, key=lambda s: -os.path.getsize(s)):
            for units in a.units.split(","):
                for i in range(n):
                    q.put(Shard(show, units, groups, i, n))
    if flows_show and os.path.exists(flows_show):
        q.put(Shard(flows_show, "feet", "flows", 0, 1))
    total = q.qsize()
    log("uxwalk: %d shards (%d shows x %d unit settings)" % (total, len(shows), len(a.units.split(","))))

    threads = []
    remotes = []
    if a.hosts:
        rev = subprocess.check_output(["git", "-C", REPO, "rev-parse", a.rev], text=True).strip()
        hosts = read_hosts(a.hosts_conf)
        names = [h for h in hosts if hosts[h]["dest"] != "local"] if a.hosts == "auto" else a.hosts.split(",")
        src = tree_of(rev)
        for n in names:
            if n not in hosts or hosts[n]["dest"] == "local":
                continue
            r = Remote(hosts[n], rev, src, shows + ([flows_show] if flows_show and flows_show not in shows else []),
                       a.out, q)
            remotes.append(r)
            t = threading.Thread(target=r.serve, daemon=True)
            t.start()
            threads.append(t)
    if not a.no_local:
        if not os.path.exists(a.binary):
            sys.exit("uxwalk: no walker at %s (cmake -DQLC_UXWALK=ON, build target uxwalk)" % a.binary)
        jobs = a.jobs or max(1, perf_cores() // 2)
        for i in range(jobs):
            t = LocalWorker(q, a.binary, a.out, "local#%d" % i)
            t.start()
            threads.append(t)
    # Done when every shard is: a host still building then has nothing to
    # take, so it is stopped and its lock let go (nobody waits for it).
    while q.unfinished_tasks and any(t.is_alive() for t in threads):
        time.sleep(1)
    for r in remotes:
        if not r.finished:
            log("%s: not needed any more%s" % (r.h["name"], "" if r.ok else " (stopped its build)"))
            r.abandon()
    if a.hosts:
        shutil.rmtree(src, ignore_errors=True)
    left = q.qsize()
    if left:
        log("uxwalk: %d shards were never run (no worker could take them)" % left)
    log("uxwalk: walked in %.0f s" % (time.time() - start))
    subprocess.run([sys.executable, os.path.join(HERE, "report.py"), a.out])
    return 0


if __name__ == "__main__":
    sys.exit(main())
