#!/usr/bin/env python3
"""
Project-robustness fuzzer: feed mutated / truncated / corrupt .qxw workspaces to
the loader (`qlcstress load`) and report crashes vs clean rejections. Loader
robustness against bad project files is a top source of production crashes.

Run against the ASan build (default if present) to also catch memory errors on
malformed input. Crashing inputs are saved for triage.

  python3 fuzz_projects.py --seed-workspace /tmp/big.qxw --count 500
  python3 fuzz_projects.py --count 300 --no-asan     # generate seed, plain build

Exit non-zero if any input crashes the loader (segv/abort), zero if all inputs
are either loaded or cleanly rejected.
"""

import argparse
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "..", "resources", "fixtures")
SCRIPTS = os.path.join(HERE, "..", "resources", "rgbscripts")
PLAIN = os.path.join(HERE, "engine", "build", "qlcstress")
ASAN = os.path.join(HERE, "engine", "build-asan", "qlcstress")
CRASH_DIR = os.path.join(HERE, "results", "fuzz-crashes")


def run_load(qlcstress, path, asan):
    env = dict(os.environ)
    if asan:
        env["MallocNanoZone"] = "0"
        env["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1:abort_on_error=1"
    try:
        p = subprocess.run([qlcstress, "load", path, "--fixtures-dir", FIXTURES],
                           capture_output=True, text=True, env=env, timeout=30)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return 124, "[timeout]"


# ---- mutators ---------------------------------------------------------------
def mutate(data, rng):
    b = bytearray(data)
    kind = rng.choice(["bitflip", "truncate", "byteset", "tagcorrupt", "numjunk", "dup"])
    if kind == "bitflip":
        for _ in range(rng.randint(1, 32)):
            if b: i = rng.randrange(len(b)); b[i] ^= (1 << rng.randint(0, 7))
    elif kind == "truncate":
        if b: b = b[:rng.randint(0, len(b))]
    elif kind == "byteset":
        for _ in range(rng.randint(1, 16)):
            if b: b[rng.randrange(len(b))] = rng.randint(0, 255)
    elif kind == "tagcorrupt":
        s = bytes(b).replace(b"</", b"<", rng.randint(1, 3)) \
                     .replace(b"Function", b"Funct\xe0\xa5n", 1)  # invalid UTF-8 in a tag
        b = bytearray(s)
    elif kind == "numjunk":  # break numeric attributes
        s = bytes(b).replace(b'="0"', b'="999999999999999999999"', rng.randint(1, 5)) \
                     .replace(b'Channels>', b'Channels>-1', 1)
        b = bytearray(s)
    elif kind == "dup":  # duplicate a chunk
        if len(b) > 100:
            i = rng.randrange(len(b) - 50); b[i:i] = b[i:i+50]
    return bytes(b), kind


def main():
    ap = argparse.ArgumentParser(description="QLC+ .qxw loader fuzzer")
    ap.add_argument("--seed-workspace", default=None, help="valid .qxw to mutate (generated if omitted)")
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--no-asan", action="store_true", help="use the plain build, not ASan")
    args = ap.parse_args()

    asan = (not args.no_asan) and os.path.exists(ASAN)
    qlcstress = ASAN if asan else PLAIN
    if not os.path.exists(qlcstress):
        print(f"ERROR: qlcstress not found at {qlcstress}"); return 2
    print(f"fuzz: using {'ASan' if asan else 'plain'} build: {qlcstress}")

    # seed workspace
    seed_ws = args.seed_workspace
    if not seed_ws:
        seed_ws = os.path.join(HERE, "results", "fuzz-seed.qxw")
        os.makedirs(os.path.dirname(seed_ws), exist_ok=True)
        subprocess.run([PLAIN, "emit", seed_ws, "--universes", "8", "--fixtures-per-uni", "40",
                        "--scenes", "30", "--chasers", "10", "--matrices", "6", "--efx", "6",
                        "--collections", "3", "--scripts", "4", "--sequences", "4", "--shows", "2",
                        "--fixtures-dir", FIXTURES, "--scripts-dir", SCRIPTS],
                       capture_output=True)
        print(f"fuzz: generated seed workspace {seed_ws}")

    rc, _ = run_load(qlcstress, seed_ws, asan)
    print(f"fuzz: baseline load of seed -> rc={rc} (expected 0)")

    with open(seed_ws, "rb") as f:
        base = f.read()
    rng = random.Random(args.seed)
    os.makedirs(CRASH_DIR, exist_ok=True)
    tmp = os.path.join(HERE, "results", "fuzz-case.qxw")

    crashes = 0; loaded = 0; rejected = 0; timeouts = 0; bykind = {}
    for i in range(args.count):
        data, kind = mutate(base, rng)
        with open(tmp, "wb") as f:
            f.write(data)
        rc, out = run_load(qlcstress, tmp, asan)
        # rc: 0 loaded; 1 cleanly rejected; <0 or 134/139 crash; 124 timeout
        if rc in (0,):
            loaded += 1
        elif rc == 1:
            rejected += 1
        elif rc == 124:
            timeouts += 1
        else:  # crash (139 segv, 134 abort/ASan, negative signals)
            crashes += 1
            bykind[kind] = bykind.get(kind, 0) + 1
            cf = os.path.join(CRASH_DIR, f"crash-{i:04d}-{kind}-rc{rc}.qxw")
            with open(cf, "wb") as f:
                f.write(data)
            print(f"fuzz: CRASH (rc={rc}, {kind}) -> saved {cf}")
            san = [l for l in out.splitlines() if "ERROR:" in l or "runtime error:" in l]
            if san:
                print("       " + san[0])
        if (i + 1) % 50 == 0:
            print(f"fuzz: {i+1}/{args.count}  loaded={loaded} rejected={rejected} crashes={crashes} timeouts={timeouts}")

    print(f"\nfuzz: done. {args.count} inputs -> loaded={loaded} rejected={rejected} "
          f"crashes={crashes} timeouts={timeouts}")
    if crashes:
        print(f"fuzz: RESULT FAIL - {crashes} crashing inputs by mutator: {bykind} (saved in {CRASH_DIR})")
        return 1
    print("fuzz: RESULT PASS - loader survived all malformed inputs (loaded or cleanly rejected)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
