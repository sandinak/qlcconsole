#!/usr/bin/env python3
"""
Fail when a Mach-O file in a macOS .app references a library that is neither
in the bundle nor in /usr/lib or /System.

Every Mach-O file under Contents/ is read with otool -L; each reference is
resolved the way dyld would:
  @executable_path/X   against Contents/MacOS
  @loader_path/X       against the file's own folder
  @rpath/X             against the file's LC_RPATHs (and, for a library, the
                       executables' in Contents/MacOS), then Contents/Frameworks
  /usr/lib, /System    system: fine
  any other absolute   outside the bundle: a fail (it exists on the build
                       machine and nowhere else)
A reference that does not resolve to an existing file inside the bundle fails.

This is the check that would have caught the 2026-11 CI build whose libsndfile
named libogg, libvorbis, libFLAC, libopus, libmpg123 and libmp3lame under
@executable_path/../Frameworks while none of them had been copied in: the
audio plugin could not load, so no sound played, WAV included.

Usage: bundle-check.py [--signed] /path/to/App.app   (exit 0 clean, 1 on a miss)
       --signed also fails a file whose code signature is invalid
Run by: .github/workflows/build.yml (macOS job) after bundling;
        platforms/macos/make-release.sh before signing;
        platforms/macos/package-local.sh (make test, make rc, make dmg).
"""
import os
import subprocess
import sys


def sh(*a):
    return subprocess.run(a, capture_output=True, text=True).stdout


def is_macho(p):
    return (not os.path.islink(p) and os.path.isfile(p)
            and "Mach-O" in sh("file", "-b", p))


def deps(p):
    refs = []
    for ln in sh("otool", "-L", p).splitlines()[1:]:
        ln = ln.strip()
        # A universal file prints a header per arch ("<path> (architecture x):").
        if not ln or ln.endswith(":"):
            continue
        refs.append(ln.split(" (")[0])
    return refs


def install_id(p):
    out = sh("otool", "-D", p).splitlines()
    return out[1].strip() if len(out) > 1 else None


def rpaths(p, macos):
    out, res = sh("otool", "-l", p).splitlines(), []
    for i, ln in enumerate(out):
        if ln.strip() == "cmd LC_RPATH" and i + 2 < len(out):
            r = out[i + 2].strip()
            if r.startswith("path "):
                r = r[5:].split(" (offset")[0]
                r = r.replace("@loader_path", os.path.dirname(p))
                r = r.replace("@executable_path", macos)
                res.append(os.path.normpath(r))
    return res


def check(app):
    app = os.path.abspath(app)
    macos = os.path.join(app, "Contents", "MacOS")
    frameworks = os.path.join(app, "Contents", "Frameworks")
    files = []
    for root, _, names in os.walk(os.path.join(app, "Contents")):
        for n in names:
            p = os.path.join(root, n)
            if is_macho(p):
                files.append(p)
    exe_rpaths = []
    if os.path.isdir(macos):
        for n in os.listdir(macos):
            p = os.path.join(macos, n)
            if is_macho(p):
                exe_rpaths += rpaths(p, macos)
    bad = []
    for p in files:
        myid = install_id(p)
        mine = rpaths(p, macos)
        for dep in deps(p):
            if dep == myid or dep.startswith(("/usr/lib/", "/System/")):
                continue
            if dep.startswith("@executable_path/"):
                cands = [os.path.join(macos, dep[len("@executable_path/"):])]
            elif dep.startswith("@loader_path/"):
                cands = [os.path.join(os.path.dirname(p), dep[len("@loader_path/"):])]
            elif dep.startswith("@rpath/"):
                rel = dep[len("@rpath/"):]
                cands = [os.path.join(d, rel) for d in mine + exe_rpaths]
            else:
                bad.append((p, dep, "absolute path outside the bundle"))
                continue
            cands = [os.path.normpath(c) for c in cands]
            inside = [c for c in cands
                      if os.path.exists(c) and (c + os.sep).startswith(app + os.sep)]
            if not inside:
                bad.append((p, dep, "not in the bundle"))
    if "--signed" in sys.argv:
        # A library whose signature install_name_tool broke is killed at load
        # on an arm64 Mac (exit 137), however well it resolves.
        for p in files:
            r = subprocess.run(["codesign", "--verify", p], capture_output=True, text=True)
            if r.returncode != 0:
                bad.append((p, "(its own signature)", "invalid: " + r.stderr.strip().splitlines()[0]))
    return files, bad


def main():
    args = [a for a in sys.argv[1:] if a != "--signed"]
    if len(args) != 1 or not os.path.isdir(args[0]):
        print(__doc__)
        return 2
    app = os.path.abspath(args[0])
    files, bad = check(app)
    if bad:
        print("FAIL: %d problems in the bundle:" % len(bad))
        for p, dep, why in bad[:60]:
            print("  %s -> %s  (%s)" % (os.path.relpath(p, app), dep, why))
        return 1
    print("OK: every reference of %d Mach-O files resolves inside the bundle"
          % len(files))
    return 0


sys.exit(main())
