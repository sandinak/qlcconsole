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

Stdlib only (wsclient.py from ../driver). Stop it with Ctrl-C or a TERM, or
by creating the file DIR/STOP (polled every few seconds): it stops the app
and writes the summary.

Windows
-------
The same script, other means (Python 3.9 or later; the layout is the repo's,
stresstest/soak24 next to stresstest/driver):

  * The app is C:\\qlcconsole\\qlcconsole.exe (--win-exe). It has to be on the
    logged-on user's desktop, and SSH runs in a session of its own, so each
    launch goes through a scheduled task, "qlcsoak-app", that runs
    interactively as that user (schtasks /IT): it runs DIR\\launch-app.cmd,
    written by this script, which sets QLC_TICKLOG, QLC_NO_RECOVERY_PROMPT
    and QT_FORCE_STDERR_LOGGING and starts the app with -d 1 -w -wp PORT, its
    stdout/stderr to the phase's app-N.log. The user must stay logged on.
    The task runs at normal priority (a task's default is below normal, and
    the app would inherit it). --win-launch direct starts the app from the
    runner instead, in the runner's own session (from SSH: on no desktop, a
    headless soak): for a PC whose Task Scheduler refuses to run tasks
    (0x800710E0, which events.log then reports); the runner must then be kept
    alive some other way (an SSH session kept open).
  * Samples read the process directly (kernel32): CPU % (CPU seconds over the
    interval, 100 = one core, as ps), working set (rss_mb), private bytes
    (private_mb: the leak signal), threads and handles (handle leaks are a
    Windows thing of their own).
  * Stop: taskkill /PID (the app is asked to close), taskkill /F after 20 s.
  * A crash: the exit code, the qlcconsole dumps in %LOCALAPPDATA%\\CrashDumps,
    the WER report and the Application event log entries since the phase
    started are copied into the phase folder. A hang: procdump -mp if procdump
    is on the PATH, else only noted (nothing is installed).
  * The untitled autosave is %USERPROFILE%\\qlcconsole\\untitled.qxw.autosave.
  * No DMX leaves the machine: on Windows the shows are copied into DIR\\shows
    with every <Output> patch removed (--keep-outputs to send DMX after all;
    --strip-outputs strips them on macOS too). A soak on a test PC must not
    drive the show's real lights.
  * The runner has to outlive the SSH session. Start it with --detach: it
    writes DIR\\start-runner.cmd and starts it through a second interactive
    scheduled task, "qlcsoak-runner" (pythonw: no window), and returns; the
    runner's own output goes to DIR\\runner.log. Stop it with --stop (or by
    creating DIR\\STOP): the runner stops the app, writes summary.json and
    ends. If the runner is gone and the app still runs: taskkill /IM
    qlcconsole.exe. From SSH (PowerShell), with $py the python.exe:

      & $py run_soak.py --show S.qlcc --scaled-show S-STRESS.qlcc --out C:\\soak\\run1 --detach
      & $py run_soak.py --out C:\\soak\\run1 --stop

    The tasks stay registered (schtasks /Delete /TN qlcsoak-app /F, and
    qlcsoak-runner, removes them).

Linux
-----
For a headless box or a container (Python 3.9 or later, same layout):

  * The app is a build tree's binary (--linux-exe, default
    /build/linux/main/qlcconsole), started by the runner with LD_LIBRARY_PATH
    pointing at the build's engine/ui/webaccess libraries, QLC_TICKLOG,
    QLC_NO_RECOVERY_PROMPT and QT_FORCE_STDERR_LOGGING set, -d 1 -w -wp PORT,
    its stdout/stderr to the phase's app-N.log, in a session of its own, with
    the phase folder as its working directory (a core file lands there).
  * It draws on a virtual display: one Xvfb (:99 or the next free number,
    1920x1080x24) is started for the whole run, QT_QPA_PLATFORM=xcb, and
    stopped at the end.
  * Samples: CPU % from /proc/PID/stat over the interval (100 = one core; ps
    would give the average since the start), rss from ps, threads from
    /proc/PID/status, and after the columns macOS has: private_mb
    (Private_Clean + Private_Dirty of /proc/PID/smaps_rollup: the leak signal)
    and fds (open file descriptors).
  * Stop: SIGTERM, SIGKILL after 10 s. A crash: the exit status or signal,
    and a core file in the phase folder or the coredumpctl entry, when the
    system keeps them (nothing is changed to make it). A hang: gdb's
    "thread apply all bt" if gdb is installed, else only noted.
  * The untitled autosave is ~/.qlcconsole/untitled.qxw.autosave.
  * The shows are stripped of their <Output> patches by default, as on
    Windows: a soak box must never send DMX.
  * --detach starts the runner again under setsid nohup, its output to
    DIR/runner.log, and returns; --stop (or DIR/STOP) ends it:

      python3 run_soak.py --show S.qlcc --scaled-show S-STRESS.qlcc --out DIR --detach
      python3 run_soak.py --out DIR --stop
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

WIN = sys.platform == "win32"
LINUX = sys.platform.startswith("linux")
MAC = not WIN and not LINUX
# --heap-track (macOS): the app runs with MallocStackLogging=lite, and the
# phase folder gets a heap summary and a vmmap summary 10 minutes in and then
# every hour -- diff two of them to see what grew.
HEAP_TRACK = False
if WIN:
    EXE = r"C:\qlcconsole\qlcconsole.exe"        # --win-exe
    UNTITLED_AUTOSAVE = os.path.join(os.environ.get("USERPROFILE", os.path.expanduser("~")),
                                     "qlcconsole", "untitled.qxw.autosave")
elif LINUX:
    EXE = "/build/linux/main/qlcconsole"          # --linux-exe
    UNTITLED_AUTOSAVE = os.path.expanduser("~/.qlcconsole/untitled.qxw.autosave")
else:
    APP = "/Applications/qlcconsole.app"
    EXE = APP + "/Contents/MacOS/qlcconsole"
    # A show posted to /loadProject has no file name: it autosaves as "untitled",
    # and a start after the run stopped the app would offer it back.
    UNTITLED_AUTOSAVE = os.path.expanduser(
        "~/Library/Application Support/qlcconsole/untitled.qxw.autosave")
STOP = False
STOP_FILE = None            # DIR/STOP: a stop asked for from outside
# Headless runs (perf campaign): the app in a sandbox HOME (--home), its I/O
# plugins from --ioplugin-dir (QLC_IOPLUGIN_DIR; an empty folder loads none),
# offscreen on every platform (--qpa), started directly on macOS too (no
# LaunchServices, nothing on screen). RSS slope fitted after --warmup-min.
SOAK_HOME = None
IOPLUGIN_DIR = None
QPA = "offscreen"
WARMUP_MIN = 30.0
MAC_CHILDREN = []
START_ERROR = [None]        # Windows: why the app could not be started


def stopping():
    global STOP
    if not STOP and STOP_FILE and os.path.exists(STOP_FILE):
        STOP = True
    return STOP


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
            # On Windows rss_mb is the working set; private bytes and handles
            # come after the columns macOS has.
            self.csv.writerow(["time", "phase", "elapsed_s", "pid", "cpu_pct", "rss_mb",
                               "threads", "answers", "ticks_over", "tick_mean_ms",
                               "tick_max_ms", "overrun_pct", "running_note"]
                              + (["private_mb", "handles"] if WIN else [])
                              + (["private_mb", "fds"] if LINUX else [])
                              + (["footprint_mb", "nice"] if MAC else [])
                              + ["interval_worst_ms"])

    def event(self, phase, msg):
        line = "%s [%s] %s" % (now(), phase, msg)
        print(line, flush=True)
        self.events.write(line + "\n")


# ---- the app on Windows --------------------------------------------------------

if WIN:
    import ctypes
    from ctypes import wintypes

    _k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _k32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    _k32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    _k32.OpenProcess.restype = wintypes.HANDLE
    _k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    _k32.CloseHandle.argtypes = [wintypes.HANDLE]
    _k32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    _k32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    _k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.c_void_p] * 4
    _k32.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD]
    _k32.GetProcessHandleCount.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    _k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    _k32.ProcessIdToSessionId.argtypes = [wintypes.DWORD, ctypes.c_void_p]
    _INVALID_HANDLE = wintypes.HANDLE(-1).value
    _QUERY_LIMITED, _VM_READ, _SYNCHRONIZE = 0x1000, 0x0010, 0x00100000
    _STILL_ACTIVE = 259
    _NO_WINDOW = 0x08000000          # CREATE_NO_WINDOW: no console flashing up

    class _PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                    ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                    ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                    ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", ctypes.c_long),
                    ("dwFlags", wintypes.DWORD), ("szExeFile", ctypes.c_wchar * 260)]

    class _MEMCOUNTERS(ctypes.Structure):       # PROCESS_MEMORY_COUNTERS_EX
        _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
                    ("PrivateUsage", ctypes.c_size_t)]

    _handles = {}        # pid -> a handle kept open from the start: the exit code
    _cpu_prev = {}       # pid -> (wall time, CPU seconds) at the last sample

    def _wrun(args, **kw):
        return subprocess.run(args, capture_output=True, text=True,
                              creationflags=_NO_WINDOW, **kw)

    def _win_run_task(name, command):
        """(Re)register the scheduled task `name` to run `command` interactively
        as the logged-on user, on the desktop, and run it. What schtasks /IT
        does, but from XML: at normal priority (a task's default, 7, is below
        normal and the app would inherit it), and whether or not Windows thinks
        it runs on battery (a VM's virtual battery is enough to refuse it).
        Returns None, or what went wrong."""
        from xml.sax.saxutils import escape
        user = "%s\\%s" % (os.environ.get("COMPUTERNAME", ""), os.environ.get("USERNAME", ""))
        xml = ('<?xml version="1.0" encoding="UTF-16"?>\n'
               '<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">\n'
               ' <RegistrationInfo><Description>run_soak.py</Description></RegistrationInfo>\n'
               ' <Principals><Principal id="Author"><UserId>%s</UserId>'
               '<LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel>'
               '</Principal></Principals>\n'
               ' <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>'
               '<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>'
               '<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>'
               '<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>4</Priority>'
               '<AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled></Settings>\n'
               ' <Actions Context="Author"><Exec><Command>%s</Command></Exec></Actions>\n'
               '</Task>\n' % (escape(user), escape(command)))
        path = os.path.splitext(command)[0] + ".task.xml"
        with open(path, "w", encoding="utf-16") as f:
            f.write(xml)
        for cmd in (["schtasks", "/Create", "/TN", name, "/XML", path, "/F"],
                    ["schtasks", "/Run", "/TN", name]):
            r = _wrun(cmd)
            if r.returncode != 0:
                return "%s %s: %s" % (cmd[0], cmd[1], (r.stdout + r.stderr).strip())
        # /Run only asks; whether the Task Scheduler did is its "Last Result".
        # 0x800710E0 (-2147020576): it refused (seen on a VM whose scheduler
        # refused every task, Windows' own too, until it was looked after).
        time.sleep(2)
        r = _wrun(["schtasks", "/Query", "/TN", name, "/V", "/FO", "LIST"])
        m = re.search(r"Last Result:\s*(-?\d+)", r.stdout)
        if m and int(m.group(1)) == -2147020576:
            return ("the Task Scheduler refused to run %s (0x800710E0); is the user logged "
                    "on, and does the scheduler run other tasks?" % name)
        return None

    def _win_procs():
        """{pid: threads} of every process running EXE's file name."""
        name = os.path.basename(EXE).lower()
        res = {}
        snap = _k32.CreateToolhelp32Snapshot(2, 0)        # TH32CS_SNAPPROCESS
        if not snap or snap == _INVALID_HANDLE:
            return res
        try:
            e = _PROCESSENTRY32W()
            e.dwSize = ctypes.sizeof(e)
            ok = _k32.Process32FirstW(snap, ctypes.byref(e))
            while ok:
                if e.szExeFile.lower() == name:
                    res[e.th32ProcessID] = e.cntThreads
                ok = _k32.Process32NextW(snap, ctypes.byref(e))
        finally:
            _k32.CloseHandle(snap)
        return res

    def _win_session(pid):
        s = wintypes.DWORD()
        return s.value if _k32.ProcessIdToSessionId(pid, ctypes.byref(s)) else None

    def _win_exit_code(pid):
        h = _handles.get(pid)
        code = wintypes.DWORD()
        if not h or not _k32.GetExitCodeProcess(h, ctypes.byref(code)):
            return None
        return None if code.value == _STILL_ACTIVE else "0x%08X" % code.value

    def _win_forget(pid):
        h = _handles.pop(pid, None)
        if h:
            _k32.CloseHandle(h)
        _cpu_prev.pop(pid, None)

    WIN_LAUNCH = "task"      # --win-launch

    def _win_start_app(port, log_path, ticklog):
        if WIN_LAUNCH == "direct":
            # Started by the runner itself, in the runner's session: not on
            # the desktop when that is an SSH session (a headless soak).
            env = dict(os.environ, QLC_TICKLOG=str(ticklog), QLC_NO_RECOVERY_PROMPT="1",
                       QT_FORCE_STDERR_LOGGING="1")
            with open(log_path, "wb") as logf:
                p = subprocess.Popen([EXE, "-d", "1", "-w", "-wp", str(port)],
                                     cwd=os.path.dirname(EXE), env=env, stdin=subprocess.DEVNULL,
                                     stdout=logf, stderr=subprocess.STDOUT,
                                     creationflags=0x00000008 | 0x00000200)  # DETACHED, new group
            h = _k32.OpenProcess(_QUERY_LIMITED | _SYNCHRONIZE, False, p.pid)
            if h:
                _handles[p.pid] = h
            return p.pid
        # out/<phase>/app-N.log -> out/launch-app.cmd, rewritten every launch.
        wrapper = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(log_path))),
                               "launch-app.cmd")
        with open(wrapper, "w") as f:
            f.write("@echo off\n"
                    "rem Written by run_soak.py: starts qlcconsole on the desktop.\n"
                    "set QLC_TICKLOG=%d\n"
                    "set QLC_NO_RECOVERY_PROMPT=1\n"
                    "set QT_FORCE_STDERR_LOGGING=1\n"
                    "cd /d \"%s\"\n"
                    "start \"\" /b \"%s\" -d 1 -w -wp %d > \"%s\" 2>&1\n"
                    % (ticklog, os.path.dirname(EXE), EXE, port, os.path.abspath(log_path)))
        err = _win_run_task("qlcsoak-app", wrapper)
        if err:
            START_ERROR[0] = err
            return None
        for _ in range(120):
            pids = app_pids()
            if pids:
                pid = pids[0]
                h = _k32.OpenProcess(_QUERY_LIMITED | _SYNCHRONIZE, False, pid)
                if h:
                    _handles[pid] = h
                return pid
            time.sleep(0.5)
        return None

    def _win_stop_app(pid):
        """taskkill (WM_CLOSE: the app closes itself), /F after 20 s."""
        how = "closed"
        if pid in app_pids():
            _wrun(["taskkill", "/PID", str(pid)])
            for _ in range(80):
                time.sleep(0.25)
                if pid not in app_pids():
                    break
            else:
                _wrun(["taskkill", "/F", "/PID", str(pid)])
                how = "killed (/F)"
                for _ in range(40):
                    time.sleep(0.25)
                    if pid not in app_pids():
                        break
        else:
            how = "already gone"
        code = _win_exit_code(pid)
        _win_forget(pid)
        return "%s, exit code %s" % (how, code)

    def _win_ps(pid):
        """CPU %, working set MB, threads, private MB, handles."""
        th = _win_procs().get(pid)
        h = _k32.OpenProcess(_QUERY_LIMITED | _VM_READ, False, pid)
        if not h or th is None:
            if h:
                _k32.CloseHandle(h)
            return None, None, None, None, None
        try:
            ft = [wintypes.FILETIME() for _ in range(4)]
            cpu = None
            if _k32.GetProcessTimes(h, *[ctypes.byref(x) for x in ft]):
                secs = [((x.dwHighDateTime << 32) | x.dwLowDateTime) / 1e7 for x in ft]
                busy, wall = secs[2] + secs[3], time.time()
                prev = _cpu_prev.get(pid)
                if prev is None:            # the first: since the process started
                    prev = (secs[0] - 11644473600.0, 0.0)
                if wall > prev[0]:
                    cpu = round(100.0 * (busy - prev[1]) / (wall - prev[0]), 1)
                _cpu_prev[pid] = (wall, busy)
            m = _MEMCOUNTERS()
            m.cb = ctypes.sizeof(m)
            ws = priv = None
            if _k32.K32GetProcessMemoryInfo(h, ctypes.byref(m), m.cb):
                ws, priv = m.WorkingSetSize / 1048576.0, m.PrivateUsage / 1048576.0
            hc = wintypes.DWORD()
            handles = hc.value if _k32.GetProcessHandleCount(h, ctypes.byref(hc)) else None
            return cpu, ws, th, priv, handles
        finally:
            _k32.CloseHandle(h)

    def _win_crash_reports(pid, since, phase_dir):
        """Copy what Windows kept of a crash into the phase folder."""
        saved = []
        stamp = time.strftime("%Y%m%d-%H%M%S")
        time.sleep(5)                       # WER writes its report after the exit
        local = os.environ.get("LOCALAPPDATA", "")
        for p in glob.glob(os.path.join(local, "CrashDumps", "qlcconsole*.dmp")):
            if os.path.getmtime(p) >= since:
                dst = os.path.join(phase_dir, os.path.basename(p))
                shutil.copy2(p, dst)
                saved.append(dst)
        for d in glob.glob(os.path.join(local, "Microsoft", "Windows", "WER", "Report*",
                                        "AppCrash_qlcconsole*")):
            if os.path.getmtime(d) >= since:
                dst = os.path.join(phase_dir, "wer-" + os.path.basename(d))
                shutil.copytree(d, dst, dirs_exist_ok=True)
                saved.append(dst)
        # Filtered on the provider after the query: a provider name the log
        # has never seen fails the whole Get-WinEvent ("parameter is incorrect").
        ps_cmd = ("$t = [datetime]::ParseExact('%s', 'yyyy-MM-dd HH:mm:ss', $null); "
                  "try { Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=$t} "
                  "-ErrorAction Stop | Where-Object { $_.ProviderName -in "
                  "'Application Error','Windows Error Reporting','Application Hang' -and "
                  "$_.Message -match 'qlcconsole' } | "
                  "Format-List TimeCreated,ProviderName,Id,Message | Out-String -Width 400 } catch {}"
                  % time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(since)))
        r = _wrun(["powershell", "-NoProfile", "-NonInteractive", "-Command", ps_cmd])
        if r.stdout.strip():
            dst = os.path.join(phase_dir, "crash-%s-eventlog.txt" % stamp)
            with open(dst, "w") as f:
                f.write(r.stdout)
            saved.append(dst)
        return saved

    def _win_hang_dump(pid, phase_dir):
        exe = shutil.which("procdump64") or shutil.which("procdump")
        if not exe:
            return "no procdump on the PATH: no stack"
        dst = os.path.join(phase_dir, "hang-%d.dmp" % int(time.time()))
        _wrun([exe, "-accepteula", "-mp", str(pid), dst], timeout=300)
        return "dump: %s" % dst


# ---- the app on Linux ----------------------------------------------------------

_lx_children = {}       # pid -> Popen: the app is the runner's child (exit status)
_lx_cpu_prev = {}       # pid -> (wall time, CPU seconds) at the last sample
_xvfb = {"proc": None, "display": None}


def _lx_start_xvfb(run):
    """One virtual display for the whole run: Xvfb on :99 or the next free."""
    n = 99
    while os.path.exists("/tmp/.X%d-lock" % n) or os.path.exists("/tmp/.X11-unix/X%d" % n):
        n += 1
    log = open(os.path.join(run.out, "xvfb.log"), "ab")
    p = subprocess.Popen(["Xvfb", ":%d" % n, "-screen", "0", "1920x1080x24", "-nolisten", "tcp"],
                         stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                         start_new_session=True)
    for _ in range(100):
        if os.path.exists("/tmp/.X11-unix/X%d" % n) or p.poll() is not None:
            break
        time.sleep(0.1)
    if p.poll() is not None:
        sys.exit("Xvfb :%d did not start (exit %s): see %s" % (n, p.returncode, log.name))
    _xvfb.update(proc=p, display=":%d" % n)
    run.event("soak", "Xvfb :%d up (pid %d)" % (n, p.pid))


def _lx_stop_xvfb(run):
    p = _xvfb["proc"]
    if p is None:
        return
    p.terminate()
    try:
        p.wait(10)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
    run.event("soak", "Xvfb %s stopped" % _xvfb["display"])
    _xvfb["proc"] = None


def _lx_lib_path():
    """The build tree's own libraries (what ui/test/*/test.sh points at)."""
    root = os.path.dirname(os.path.dirname(os.path.abspath(EXE)))
    dirs = [os.path.join(root, d) for d in ("engine/src", "ui/src", "webaccess/src",
                                            "engine/audio/src", "hotplugmonitor/src")]
    return ":".join([d for d in dirs if os.path.isdir(d)]
                    + [p for p in os.environ.get("LD_LIBRARY_PATH", "").split(":") if p])


def _lx_start_app(port, log_path, ticklog):
    env = dict(os.environ, QLC_TICKLOG=str(ticklog), QLC_NO_RECOVERY_PROMPT="1",
               QT_FORCE_STDERR_LOGGING="1", QT_QPA_PLATFORM=QPA,
               DISPLAY=_xvfb["display"] or os.environ.get("DISPLAY", ""),
               LD_LIBRARY_PATH=_lx_lib_path())
    if SOAK_HOME:
        env["HOME"] = SOAK_HOME
    if IOPLUGIN_DIR is not None:
        env["QLC_IOPLUGIN_DIR"] = IOPLUGIN_DIR
    with open(log_path, "wb") as logf:
        try:
            def allow_core():   # a crash leaves a core in the phase folder
                import resource
                try:
                    resource.setrlimit(resource.RLIMIT_CORE, (resource.getrlimit(resource.RLIMIT_CORE)[1],) * 2)
                except (ValueError, OSError):
                    pass
            p = subprocess.Popen([EXE, "-d", "1", "-w", "-wp", str(port)],
                                 cwd=os.path.dirname(os.path.abspath(log_path)), env=env,
                                 stdin=subprocess.DEVNULL, stdout=logf,
                                 stderr=subprocess.STDOUT, start_new_session=True,
                                 preexec_fn=allow_core)
        except OSError as e:
            START_ERROR[0] = str(e)
            return None
    _lx_children[p.pid] = p
    return p.pid


def _lx_exit(pid):
    """How the app ended: 'exit status N', 'signal SIGSEGV', or None (running)."""
    p = _lx_children.get(pid)
    if p is None:
        return None
    try:
        rc = p.wait(5)
    except subprocess.TimeoutExpired:
        return None
    _lx_children.pop(pid, None)
    _lx_cpu_prev.pop(pid, None)
    if rc < 0:
        try:
            return "signal %s" % signal.Signals(-rc).name
        except ValueError:
            return "signal %d" % -rc
    return "exit status %d" % rc


def _lx_stop_app(pid):
    """SIGTERM, SIGKILL after 10 s."""
    if pid not in _lx_children and pid not in app_pids():
        return "already gone"
    how = "terminated"
    try:
        os.kill(pid, signal.SIGTERM)
    except ProcessLookupError:
        how = "already gone"
    for _ in range(40):
        time.sleep(0.25)
        if pid not in app_pids():
            break
    else:
        how = "killed (SIGKILL)"
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    return "%s, %s" % (how, _lx_exit(pid))


def _lx_ps(pid):
    """CPU %, rss MB, threads, private MB, fds."""
    proc = "/proc/%d" % pid
    try:
        with open(proc + "/stat") as f:
            st = f.read().rsplit(")", 1)[1].split()
        tck = os.sysconf("SC_CLK_TCK")
        busy = (int(st[11]) + int(st[12])) / tck        # utime + stime
        wall = time.time()
        prev = _lx_cpu_prev.get(pid)
        if prev is None:                                # the first: since the start
            with open("/proc/stat") as f:
                btime = int(re.search(r"^btime (\d+)", f.read(), re.M).group(1))
            prev = (btime + int(st[19]) / tck, 0.0)
        cpu = round(100.0 * (busy - prev[1]) / (wall - prev[0]), 1) if wall > prev[0] else None
        _lx_cpu_prev[pid] = (wall, busy)
        out = subprocess.check_output(["ps", "-o", "rss=", "-p", str(pid)],
                                      stderr=subprocess.DEVNULL, text=True).split()
        rss = float(out[0]) / 1024.0
        with open(proc + "/status") as f:
            th = int(re.search(r"^Threads:\s*(\d+)", f.read(), re.M).group(1))
        priv = 0
        with open(proc + "/smaps_rollup") as f:
            for ln in f:
                if ln.startswith(("Private_Clean:", "Private_Dirty:")):
                    priv += int(ln.split()[1])
        fds = len(os.listdir(proc + "/fd"))
        return cpu, rss, th, priv / 1024.0, fds
    except (OSError, subprocess.CalledProcessError, IndexError, ValueError, AttributeError):
        return None, None, None, None, None


def _lx_crash_reports(pid, since, phase_dir):
    """A core file in the phase folder (the app's working directory), or what
    coredumpctl kept, when the system keeps cores at all."""
    time.sleep(3)
    saved = [p for p in glob.glob(os.path.join(phase_dir, "core*"))
             if os.path.getmtime(p) >= since]
    try:
        with open("/proc/sys/kernel/core_pattern") as f:
            pattern = f.read().strip()
    except OSError:
        pattern = "?"
    if pattern.startswith("|") and shutil.which("coredumpctl"):
        r = subprocess.run(["coredumpctl", "info", str(pid)], capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            dst = os.path.join(phase_dir, "coredump-%d.txt" % pid)
            with open(dst, "w") as f:
                f.write(r.stdout)
            saved.append(dst)
    if not saved:
        import resource
        soft = resource.getrlimit(resource.RLIMIT_CORE)[0]
        saved.append("no core (core_pattern %r, ulimit -c %s)"
                     % (pattern, "unlimited" if soft == resource.RLIM_INFINITY else soft))
    return saved


def _lx_hang_stack(pid, phase_dir):
    gdb = shutil.which("gdb")
    if not gdb:
        return "no gdb installed: no stack"
    dst = os.path.join(phase_dir, "hang-%d.txt" % int(time.time()))
    try:
        r = subprocess.run([gdb, "-p", str(pid), "-batch", "-ex", "thread apply all bt"],
                           capture_output=True, text=True, timeout=300)
        with open(dst, "w") as f:
            f.write(r.stdout + r.stderr)
    except (OSError, subprocess.TimeoutExpired) as e:
        return "gdb failed: %s" % e
    return "stack: %s" % dst


def strip_outputs(src, dst):
    """A copy of the show without its <Output> patches: it drives no lights."""
    with open(src, encoding="utf-8", newline="") as f:
        text = f.read()
    text, n = re.subn(r"[ \t]*<Output\b[^>]*/>[ \t]*\r?\n?"
                      r"|[ \t]*<Output\b[^>]*>.*?</Output>[ \t]*\r?\n?", "", text, flags=re.S)
    with open(dst, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    return n


# ---- the app ----------------------------------------------------------------

def app_pids():
    if WIN:
        return sorted(_win_procs())
    for c in MAC_CHILDREN if MAC else []:
        c.poll()                                # reap: a zombie is not running
    out = subprocess.run(["pgrep", "-f", "^" + re.escape(EXE) + "( |$)"],
                         capture_output=True, text=True).stdout.split()
    return [int(p) for p in out]


def start_app(port, log_path, ticklog):
    if WIN:
        return _win_start_app(port, log_path, ticklog)
    if LINUX:
        return _lx_start_app(port, log_path, ticklog)
    if SOAK_HOME:
        env = dict(os.environ, QT_QPA_PLATFORM=QPA, QLC_TICKLOG=str(ticklog),
                   QLC_NO_RECOVERY_PROMPT="1", HOME=SOAK_HOME, CFFIXED_USER_HOME=SOAK_HOME)
        if IOPLUGIN_DIR is not None:
            env["QLC_IOPLUGIN_DIR"] = IOPLUGIN_DIR
        if HEAP_TRACK:
            env["MallocStackLogging"] = "lite"
        logf = open(log_path, "ab")
        MAC_CHILDREN.append(subprocess.Popen([EXE, "-d", "1", "-w", "-wp", str(port)], env=env,
                                             stdin=subprocess.DEVNULL, stdout=logf, stderr=subprocess.STDOUT,
                                             start_new_session=True, cwd=os.path.dirname(log_path)))
        logf.close()
        time.sleep(1)
        return MAC_CHILDREN[-1].pid if MAC_CHILDREN[-1].poll() is None else None
    # -d 1: warnings and up reach the log (the tick log is warnings; the app
    # shows only criticals otherwise). No recovery prompt on screen between
    # launches: the run's own autosave is not the user's work.
    subprocess.run(["open", "-n", "-a", APP,
                    "--stdout", log_path, "--stderr", log_path,
                    "--env", "QLC_TICKLOG=%d" % ticklog,
                    "--env", "QLC_NO_RECOVERY_PROMPT=1"]
                   + (["--env", "MallocStackLogging=lite"] if HEAP_TRACK else [])
                   + ["--args", "-d", "1", "-w", "-wp", str(port)], check=True)
    for _ in range(120):
        pids = app_pids()
        if pids:
            return pids[0]
        time.sleep(0.5)
    return None


def stop_app(pid):
    if pid is None:
        return
    if WIN:
        return _win_stop_app(pid)
    if LINUX:
        return _lx_stop_app(pid)
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
    # 600 s: with --heap-track (MallocStackLogging) the STRESS show takes ~4
    # min to load on kronk's i7. A failed or timed-out POST is a failed load
    # for the caller to log, not an exception that ends the runner.
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=600)
    try:
        c.request("POST", "/loadProject", body=body,
                  headers={"Content-Type": "multipart/form-data; boundary=" + boundary,
                           "Content-Length": str(len(body))})
        r = c.getresponse()
        r.read()
    except (OSError, http.client.HTTPException):
        return False
    finally:
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


def mac_footprint(pid):
    """phys_footprint in MB (footprint(1)): ps rss counts shared and
    reclaimable pages on Apple Silicon and drifts upward on its own."""
    try:
        out = subprocess.check_output(["footprint", "-f", "bytes", "-p", str(pid)],
                                      stderr=subprocess.DEVNULL, text=True, timeout=60)
        m = re.search(r"Footprint:\s*([\d.]+)\s*(B|KB|MB|GB)", out)
        if m:
            return float(m.group(1)) / {"B": 1048576.0, "KB": 1024.0, "MB": 1.0, "GB": 1 / 1024.0}[m.group(2)]
    except (OSError, subprocess.SubprocessError, ValueError):
        pass
    return None


def ps(pid):
    if WIN:
        return _win_ps(pid)[:3]
    if LINUX:
        return _lx_ps(pid)[:3]
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
    if WIN:     # rss is the working set; private bytes are the leak signal
        stats.update({"private_first": None, "private_peak": 0.0, "private_last": None,
                      "handles_first": None, "handles_peak": 0, "handles_last": None,
                      "exit_codes": []})
    if LINUX:   # private (USS) is the leak signal; fds as Windows' handles
        stats.update({"private_first": None, "private_peak": 0.0, "private_last": None,
                      "fds_first": None, "fds_peak": 0, "fds_last": None,
                      "exit_codes": []})
    launch = 0
    slope_pts = []              # (elapsed, rss, leak signal) after each launch's warm-up
    while not stopping() and time.time() < end:
        launch += 1
        log_path = os.path.join(phase_dir, "app-%d.log" % launch)
        t0 = time.time()
        pid = start_app(port, log_path, ticklog)
        if pid is None or not wait_http(port):
            if START_ERROR[0]:
                run.event(name, "app did not come up (launch %d): %s" % (launch, START_ERROR[0]))
                START_ERROR[0] = None
            else:
                run.event(name, "app did not come up (launch %d)" % launch)
            stop_app(pid)
            time.sleep(10)
            continue
        if not load_project(port, show):
            run.event(name, "loadProject failed")
        time.sleep(15)                         # a big show takes a while to load
        ws = connect_ws(port)
        funcs = functions(ws) if ws else []
        if WIN:
            run.event(name, "pid %d up in session %s, %d functions (launch %d)"
                      % (pid, _win_session(pid), len(funcs), launch))
        else:
            run.event(name, "pid %d up, %d functions (launch %d)" % (pid, len(funcs), launch))
        drv = Driver(ws, funcs, rng, name)
        if name == "scaled":
            run.event(name, drv.start_block())
        tl = TickLog(log_path)
        misses, next_sample, next_reload = 0, time.time(), time.time() + 3600
        next_heap = time.time() + 600
        while not stopping() and time.time() < end:
            if pid not in app_pids():
                stats["crashes"] += 1
                if WIN:
                    code = _win_exit_code(pid)
                    stats["exit_codes"].append(code)
                    run.event(name, "CRASH: pid %d gone, exit code %s; reports: %s"
                              % (pid, code, _win_crash_reports(pid, t0, phase_dir)))
                elif LINUX:
                    code = _lx_exit(pid)
                    stats["exit_codes"].append(code)
                    run.event(name, "CRASH: pid %d gone, %s; reports: %s"
                              % (pid, code, _lx_crash_reports(pid, t0, phase_dir)))
                else:
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
            if HEAP_TRACK and MAC and time.time() >= next_heap:
                next_heap = time.time() + 3600
                stamp = "%s-%05d" % (launch, int(time.time() - started))
                for tool, args in (("heap", ["heap", "-s", str(pid)]),
                                   ("vmmap", ["vmmap", "--summary", str(pid)])):
                    try:
                        r = subprocess.run(args, capture_output=True, text=True, timeout=300)
                        with open(os.path.join(phase_dir, "%s-%s.txt" % (tool, stamp)), "w") as f:
                            f.write(r.stdout)
                    except (OSError, subprocess.TimeoutExpired) as e:
                        run.event(name, "%s snapshot failed: %s" % (tool, e))
                run.event(name, "heap snapshot %s" % stamp)
            if time.time() >= next_sample:
                next_sample += sample_s
                if WIN:
                    cpu, rss, th, priv, handles = _win_ps(pid)
                elif LINUX:
                    cpu, rss, th, priv, handles = _lx_ps(pid)
                else:
                    cpu, rss, th = ps(pid)
                try:
                    answers = bool(ws and ws.request("QLC+API|getFunctionsNumber",
                                                     expect_prefix="QLC+API|getFunctionsNumber",
                                                     wait=5.0))
                except (OSError, WSError) as e:
                    # A timeout half way through a frame (a busy app, a big
                    # function list) leaves the stream out of step: start over.
                    answers = False
                    run.event(name, "websocket (sample): %s; reconnecting" % e)
                    if ws:
                        ws.close()
                    ws = connect_ws(port, tries=10)
                    if ws:
                        drv.ws = ws
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
                row = [now(), name, int(time.time() - started), pid, cpu,
                       None if rss is None else round(rss, 1), th, int(answers), over,
                       summ[0] if summ else "", summ[1] if summ else "",
                       summ[2] if summ else "", len(drv.on)]
                if WIN or LINUX:
                    hk = "handles" if WIN else "fds"
                    if priv is not None:
                        stats["private_first"] = stats["private_first"] or round(priv, 1)
                        stats["private_peak"] = max(stats["private_peak"], round(priv, 1))
                        stats["private_last"] = round(priv, 1)
                    if handles is not None:
                        stats[hk + "_first"] = stats[hk + "_first"] or handles
                        stats[hk + "_peak"] = max(stats[hk + "_peak"], handles)
                        stats[hk + "_last"] = handles
                    row += [None if priv is None else round(priv, 1), handles]
                fp = None
                if MAC:
                    fp = mac_footprint(pid)
                    try:
                        nic = subprocess.check_output(["ps", "-o", "nice=", "-p", str(pid)], text=True).strip()
                    except subprocess.CalledProcessError:
                        nic = ""
                    row += [None if fp is None else round(fp, 1), nic]
                el = time.time() - t0
                row.append(round(worst, 2) if over else "")
                leak_sig = fp if MAC else (priv if (WIN or LINUX) else None)
                if el >= WARMUP_MIN * 60 and rss is not None:
                    slope_pts.append((time.time() - started, rss, leak_sig))
                run.csv.writerow(row)
                if misses >= 3:
                    stats["hangs"] += 1
                    if WIN:
                        run.event(name, "HANG: no answer for 3 samples; %s"
                                  % _win_hang_dump(pid, phase_dir))
                        run.event(name, "stopped: %s" % stop_app(pid))
                    elif LINUX:
                        run.event(name, "HANG: no answer for 3 samples; %s"
                                  % _lx_hang_stack(pid, phase_dir))
                        run.event(name, "stopped: %s" % stop_app(pid))
                    else:
                        run.event(name, "HANG: no answer for 3 samples; sampling the stack")
                        subprocess.run(["sample", str(pid), "5", "-file",
                                        os.path.join(phase_dir, "hang-%d.txt" % int(time.time()))],
                                       capture_output=True)
                        stop_app(pid)
                    break
            time.sleep(3)
        how = stop_app(pid)
        if (WIN or LINUX) and how:
            run.event(name, "stopped: %s" % how)
        if os.path.exists(UNTITLED_AUTOSAVE):
            os.remove(UNTITLED_AUTOSAVE)        # this run's, never the user's (set aside)
        if not stopping() and time.time() < end:
            stats["restarts"] += 1
            time.sleep(10)
    for p in glob.glob(os.path.join(phase_dir, "app-*.log")):
        with open(p, "rb") as src, gzip.open(p + ".gz", "wb") as dst:
            shutil.copyfileobj(src, dst)
        try:
            os.remove(p)
        except OSError as e:                    # Windows: still held open
            run.event(name, "could not remove %s: %s" % (p, e))
    cpu = stats.pop("cpu")
    stats["cpu_mean"] = round(sum(cpu) / len(cpu), 1) if cpu else None

    def slope(pts, k):
        xs = [q[0] for q in pts if q[k] is not None]
        ys = [q[k] for q in pts if q[k] is not None]
        n = len(xs)
        if n < 3:
            return None
        mx, my = sum(xs) / n, sum(ys) / n
        d = sum((x - mx) ** 2 for x in xs)
        return round(sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / d * 3600.0, 3) if d else None
    stats["warmup_min"] = WARMUP_MIN
    stats["rss_slope_mb_per_h_after_warmup"] = slope(slope_pts, 1)
    stats["leak_signal"] = "footprint_mb" if MAC else ("private_mb" if (WIN or LINUX) else "rss_mb")
    stats["leak_signal_slope_mb_per_h_after_warmup"] = slope(slope_pts, 2)
    stats["hours"] = round((time.time() - started) / 3600, 2)
    run.event(name, "phase done: %s" % json.dumps(stats))
    return stats


def detach(a):
    """Windows: start this run through an interactive scheduled task, so it
    lives on after the SSH session (and starts the app on the desktop)."""
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    pyw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
    if not os.path.exists(pyw):
        pyw = sys.executable
    script = os.path.abspath(__file__)
    args = [pyw, script, "--show", os.path.abspath(a.show),
            "--scaled-show", os.path.abspath(a.scaled_show), "--out", out,
            "--hours", repr(a.hours), "--phases", a.phases, "--port", str(a.port),
            "--ticklog", str(a.ticklog), "--sample", str(a.sample), "--seed", str(a.seed),
            "--win-exe", a.win_exe, "--win-launch", a.win_launch] + ([] if a.strip_outputs else ["--keep-outputs"])
    wrapper = os.path.join(out, "start-runner.cmd")
    with open(wrapper, "w") as f:
        f.write("@echo off\n"
                "rem Written by run_soak.py --detach: the soak runner, without a window.\n"
                "cd /d \"%s\"\n"
                "start \"\" /b %s >> \"%s\" 2>&1\n"
                % (os.path.dirname(script), subprocess.list2cmdline(args),
                   os.path.join(out, "runner.log")))
    err = _win_run_task("qlcsoak-runner", wrapper)
    if err:
        sys.exit(err)
    print("soak started through the scheduled task qlcsoak-runner (%s)" % wrapper)
    print("  progress: %s" % os.path.join(out, "events.log"))
    print("  stop:     %s %s --out \"%s\" --stop" % (sys.executable, script, out))


def detach_linux(a):
    """Linux: the same run again under setsid nohup, past the end of the
    shell (or the podman exec) that started it; its output to DIR/runner.log."""
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    script = os.path.abspath(__file__)
    args = [sys.executable, script, "--show", os.path.abspath(a.show),
            "--scaled-show", os.path.abspath(a.scaled_show), "--out", out,
            "--hours", repr(a.hours), "--phases", a.phases, "--port", str(a.port),
            "--ticklog", str(a.ticklog), "--sample", str(a.sample), "--seed", str(a.seed),
            "--linux-exe", os.path.abspath(a.linux_exe)] \
        + ([] if a.strip_outputs else ["--keep-outputs"])
    log = open(os.path.join(out, "runner.log"), "ab")
    p = subprocess.Popen(["setsid", "nohup"] + args, cwd=os.path.dirname(script),
                         stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    print("soak started under setsid nohup (pid %d)" % p.pid)
    print("  progress: %s" % os.path.join(out, "events.log"))
    print("  stop:     %s %s --out %s --stop" % (sys.executable, script, out))


def detach_posix(a):
    """Linux / macOS: this run again in a session of its own (nohup), with
    every option it was given except --detach; output to DIR/runner.log."""
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    argv = [x for x in sys.argv[1:] if x != "--detach"]
    if a.nice is not None:
        i = argv.index("--nice")
        del argv[i:i + 2]
    args = [sys.executable, os.path.abspath(__file__)] + argv
    if a.nice is not None:
        args = ["nice", "-n", str(a.nice)] + args
    log = open(os.path.join(out, "runner.log"), "ab")
    p = subprocess.Popen(["nohup"] + args, cwd=os.path.dirname(os.path.abspath(__file__)),
                         stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                         start_new_session=True)
    print("soak started (pid %d)" % p.pid)
    print("  progress: %s" % os.path.join(out, "events.log"))
    print("  stop:     %s %s --out %s --stop" % (sys.executable, os.path.abspath(__file__), out))


def ask_stop(out):
    """Ask a running soak to stop (DIR/STOP) and wait for it to finish."""
    with open(os.path.join(out, "STOP"), "w") as f:
        f.write(now() + "\n")
    print("asked the soak in %s to stop; waiting for it" % out)
    events = os.path.join(out, "events.log")
    for _ in range(120):
        try:
            with open(events, errors="replace") as f:
                last = f.read().rstrip().splitlines()[-1:]
        except OSError:
            last = []
        if last and "[soak] finished" in last[0]:
            print(last[0])
            return
        time.sleep(1)
    sys.exit("no 'finished' in %s after 2 min: is the runner alive? (if not: %s)"
             % (events, "taskkill /IM qlcconsole.exe" if WIN else "pkill -f " + EXE))


def main():
    global EXE, STOP_FILE, WIN_LAUNCH
    ap = argparse.ArgumentParser()
    ap.add_argument("--show")
    ap.add_argument("--scaled-show")
    ap.add_argument("--out", required=True)
    ap.add_argument("--hours", type=float, default=8.0, help="per phase")
    ap.add_argument("--phases", default="real,scaled,rough")
    ap.add_argument("--port", type=int, default=9999)
    ap.add_argument("--ticklog", type=int, default=500)
    ap.add_argument("--sample", type=int, default=30)
    ap.add_argument("--seed", type=int, default=24)
    ap.add_argument("--strip-outputs", action="store_true", default=WIN or LINUX,
                    help="run copies of the shows without <Output> patches "
                         "(default on Windows and Linux)")
    ap.add_argument("--keep-outputs", dest="strip_outputs", action="store_false",
                    help="run the shows as they are, outputs and all")
    ap.add_argument("--stop", action="store_true", help="ask the soak in --out to stop")
    ap.add_argument("--app-exe", default=None, help="the app binary (any platform)")
    ap.add_argument("--home", default=None, help="sandbox HOME for the app (headless runs)")
    ap.add_argument("--ioplugin-dir", default=None, help="QLC_IOPLUGIN_DIR for the app (empty dir: no I/O plugins)")
    ap.add_argument("--qpa", default="offscreen", help="QT_QPA_PLATFORM (Linux xcb starts an Xvfb)")
    ap.add_argument("--warmup-min", type=float, default=30.0, help="minutes after each launch left out of the RSS slope")
    ap.add_argument("--heap-track", action="store_true",
                    help="macOS: MallocStackLogging=lite + hourly heap/vmmap summaries")
    if WIN:
        ap.add_argument("--win-exe", default=EXE)
        ap.add_argument("--win-launch", choices=("task", "direct"), default="task",
                        help="task: on the desktop through a scheduled task (the default); "
                             "direct: started by the runner, in its own session")
        ap.add_argument("--detach", action="store_true",
                        help="run through a scheduled task, past the end of the SSH session")
    if LINUX:
        ap.add_argument("--linux-exe", default=EXE, help="the app binary (a build tree's)")
    if LINUX or MAC:
        ap.add_argument("--detach", action="store_true",
                        help="run in a session of its own under nohup, past the end of the shell")
        ap.add_argument("--nice", type=int, default=None, help="with --detach: start at this nice")
    a = ap.parse_args()
    global HEAP_TRACK, SOAK_HOME, IOPLUGIN_DIR, QPA, WARMUP_MIN, UNTITLED_AUTOSAVE
    HEAP_TRACK = a.heap_track
    QPA, WARMUP_MIN = a.qpa, a.warmup_min
    if a.app_exe:
        EXE = os.path.abspath(a.app_exe)
    if a.home:
        SOAK_HOME = os.path.abspath(a.home)
        os.makedirs(SOAK_HOME, exist_ok=True)
        UNTITLED_AUTOSAVE = os.path.join(SOAK_HOME, ".qlcconsole/untitled.qxw.autosave") if LINUX else \
            os.path.join(SOAK_HOME, "Library/Application Support/qlcconsole/untitled.qxw.autosave")
    if a.ioplugin_dir is not None:
        IOPLUGIN_DIR = os.path.abspath(a.ioplugin_dir)

    if a.stop:
        ask_stop(a.out)
        return
    if not a.show or not a.scaled_show:
        ap.error("--show and --scaled-show are needed")
    if WIN:
        EXE, WIN_LAUNCH = a.win_exe, a.win_launch
    if LINUX and not a.app_exe:
        EXE = os.path.abspath(a.linux_exe)
    STOP_FILE = os.path.join(a.out, "STOP")

    if app_pids():
        sys.exit("qlcconsole is already running: quit it first (this run starts its own)")
    if WIN and a.detach:
        detach(a)
        return
    if (LINUX or MAC) and getattr(a, "detach", False):
        detach_posix(a)
        return
    if os.path.exists(STOP_FILE):
        os.remove(STOP_FILE)                     # from a run before

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
    if a.strip_outputs:
        os.makedirs(os.path.join(a.out, "shows"), exist_ok=True)
        for src in sorted(set(shows.values())):
            base, ext = os.path.splitext(os.path.basename(src))
            if not base.endswith(".nooutput"):
                base += ".nooutput"
            dst = os.path.join(a.out, "shows", base + ext)
            n = strip_outputs(src, dst)
            run.event("soak", "%s: %d output patches removed -> %s" % (src, n, dst))
            shows = {k: (dst if v == src else v) for k, v in shows.items()}
    run.event("soak", "start: phases %s, %.1f h each" % (a.phases, a.hours))
    if LINUX and QPA == "xcb":
        _lx_start_xvfb(run)
    results = []
    try:
        for name in a.phases.split(","):
            if stopping():
                break
            results.append(run_phase(run, name, shows[name], a.hours, a.port, a.ticklog,
                                     a.sample, rng))
    finally:
        if LINUX:
            _lx_stop_xvfb(run)
    json.dump(results, open(os.path.join(a.out, "summary.json"), "w"), indent=2)
    if os.path.exists(kept):
        shutil.move(kept, UNTITLED_AUTOSAVE)
        run.event("soak", "put the untitled autosave back")
    run.event("soak", "finished%s" % (" (stopped early)" if STOP else ""))


if __name__ == "__main__":
    main()
