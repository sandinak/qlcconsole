#!/usr/bin/env python3
"""
Make a macOS .app fully self-contained.

Homebrew's Qt5 uses ABSOLUTE install names (/opt/homebrew/...) rather than the
@rpath that official Qt uses, so the standard macdeployqt flow leaves absolute
references behind (in the executables AND inside the Qt framework binaries) and
misses transitive deps (harfbuzz, glib, pcre2, md4c, the libsndfile codec
chain, ...). This walker seeds from the app's executables + plugins, then
recursively:
  * copies every non-system dependency into Contents/Frameworks
    (frameworks as X.framework/..., flat libs as their basename),
  * rewrites every absolute non-system reference (deps AND install-ids) to
    @rpath/<bundle-relative>,
  * ensures each executable has an LC_RPATH of @executable_path/../Frameworks.
Finally it verifies that no absolute non-system reference remains.

Usage: bundlefix.py /path/to/App.app
"""
import os, sys, shutil, subprocess

APP = os.path.abspath(sys.argv[1])
FW  = os.path.join(APP, "Contents", "Frameworks")
os.makedirs(FW, exist_ok=True)

def sh(*a):
    return subprocess.run(a, capture_output=True, text=True)

def is_macho(p):
    if os.path.islink(p) or not os.path.isfile(p):
        return False
    r = sh("file", "-b", p).stdout
    return "Mach-O" in r

def install_id(p):
    out = sh("otool", "-D", p).stdout.splitlines()
    # line 0 is the "path:" header; line 1 (if present) is the id
    return out[1].strip() if len(out) > 1 and out[1].strip() else None

def deps(p):
    out = sh("otool", "-L", p).stdout.splitlines()[1:]  # drop header
    res = []
    for ln in out:
        ln = ln.strip()
        if not ln:
            continue
        res.append(ln.split(" (")[0])
    return res

def is_system(dep):
    return dep.startswith("/usr/lib/") or dep.startswith("/System/")

def needs_fix(dep):
    # absolute path that is not a system lib and not already bundle-relative
    return dep.startswith("/") and not is_system(dep)

def make_writable(path):
    for root, dirs, files in os.walk(path):
        for name in dirs + files:
            fp = os.path.join(root, name)
            try:
                if not os.path.islink(fp):
                    os.chmod(fp, os.stat(fp).st_mode | 0o200)
            except OSError:
                pass

def framework_split(dep):
    """For a path like /opt/.../QtCore.framework/Versions/5/QtCore return
    (src_framework_dir, 'QtCore.framework/Versions/5/QtCore'). Else None."""
    if ".framework/" not in dep:
        return None
    idx = dep.rfind(".framework/")
    # back up to the start of the '<Name>.framework' component
    start = dep.rfind("/", 0, idx) + 1
    rel = dep[start:]                       # QtCore.framework/Versions/5/QtCore
    src_fw = dep[:idx] + ".framework"       # /opt/.../QtCore.framework
    return src_fw, rel

def rpath_ref(dep):
    """The bundle-relative @rpath reference for an absolute path, WITHOUT
    copying anything (used for rewriting a file's own install-id)."""
    fw = framework_split(dep)
    if fw:
        return "@rpath/" + fw[1]
    return "@rpath/" + os.path.basename(dep)

def ensure_copied(dep):
    """Copy dep into Frameworks if missing. Return (@rpath-ref, dest_path)."""
    fw = framework_split(dep)
    if fw:
        src_fw, rel = fw
        dest_fw = os.path.join(FW, os.path.basename(src_fw))
        if not os.path.exists(dest_fw):
            if os.path.isdir(src_fw):
                shutil.copytree(src_fw, dest_fw, symlinks=True)
                make_writable(dest_fw)
                # prune bulky, unneeded parts
                for junk in ("Headers", "Versions/Current/Headers",
                             "Versions/5/Headers"):
                    jp = os.path.join(dest_fw, junk)
                    if os.path.isdir(jp) and not os.path.islink(jp):
                        shutil.rmtree(jp, ignore_errors=True)
        return "@rpath/" + rel, os.path.join(FW, rel)
    else:
        base = os.path.basename(dep)
        dest = os.path.join(FW, base)
        if not os.path.exists(dest):
            if os.path.isfile(dep):
                shutil.copy2(dep, dest)
                os.chmod(dest, os.stat(dest).st_mode | 0o200)
        return "@rpath/" + base, dest

def process(path, seen):
    # Resolve symlinks to the real file so a symlink (e.g. libsndfile.1.dylib
    # -> libsndfile.1.0.37.dylib) doesn't mark the target 'seen' and skip it.
    path = os.path.realpath(path)
    if path in seen:
        return
    seen.add(path)
    if not is_macho(path):
        return
    os.chmod(path, os.stat(path).st_mode | 0o200)
    myid = install_id(path)
    # Fix own install id if it points outside the bundle — rewrite the string
    # only; do NOT copy (copying a plugin's own id would clone it into
    # Frameworks as an unprocessed duplicate).
    if myid and needs_fix(myid):
        sh("install_name_tool", "-id", rpath_ref(myid), path)
    # fix each dependency edge
    for dep in deps(path):
        if dep == myid or not needs_fix(dep):
            continue
        newref, dest = ensure_copied(dep)
        sh("install_name_tool", "-change", dep, newref, path)
        if dest and os.path.exists(dest):
            process(dest, seen)

def add_rpath(exe):
    have = sh("otool", "-l", exe).stdout
    if "@executable_path/../Frameworks" not in have:
        sh("install_name_tool", "-add_rpath", "@executable_path/../Frameworks", exe)

def all_macho():
    out = []
    for base in ("Contents/MacOS", "Contents/PlugIns", "Contents/Frameworks"):
        d = os.path.join(APP, base)
        for root, _, files in os.walk(d):
            for name in files:
                p = os.path.join(root, name)
                if is_macho(p):
                    out.append(p)
    return out

def main():
    seen = set()
    # Seed from executables and plugins (frameworks get pulled in transitively).
    seeds = []
    macos = os.path.join(APP, "Contents", "MacOS")
    for name in os.listdir(macos):
        seeds.append(os.path.join(macos, name))
    plug = os.path.join(APP, "Contents", "PlugIns")
    for root, _, files in os.walk(plug):
        for name in files:
            seeds.append(os.path.join(root, name))
    # also anything already sitting in Frameworks (QLC+ libs, pre-placed libs)
    for root, _, files in os.walk(FW):
        for name in files:
            seeds.append(os.path.join(root, name))

    for s in seeds:
        process(s, seen)

    # rpath on the executables
    for name in os.listdir(macos):
        p = os.path.join(macos, name)
        if is_macho(p):
            add_rpath(p)

    # Verify: no absolute non-system reference should remain anywhere.
    bad = []
    for p in all_macho():
        myid = install_id(p)
        for dep in deps(p):
            if dep == myid:
                continue
            if needs_fix(dep):
                bad.append((p, dep))
    if bad:
        print("FAIL: %d absolute references remain:" % len(bad))
        for p, dep in bad[:40]:
            print("  %s -> %s" % (p.replace(APP + "/", ""), dep))
        return 1
    print("OK: bundle is self-contained (%d Mach-O files, %d copied into Frameworks)"
          % (len(all_macho()), len([1 for _ in os.listdir(FW)])))
    return 0

sys.exit(main())
