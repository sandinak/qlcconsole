#!/usr/bin/env python3
"""Row ids in docs/dev/TEST_MAP.md (M001, M002, ...) without collisions.

    tools/test-map-ids.py next          the next free id, over this file and
                                        every branch here (local and origin/*)
    tools/test-map-ids.py assign        give each `| Mnew |` row the next free id
    tools/test-map-ids.py check         no id used twice (the gate runs this)
    tools/test-map-ids.py --self-test

A branch adds its rows as `| Mnew | ...` and never picks a number: on
2026-11-13 four branches each took M989 and the train renumbered them by
hand. Whoever merges the train runs `assign` once, on the merged tree, and
commits it. (`next` is for a lone change that goes straight to main.)

`check` fails on an id used by two rows -- except the duplicates that were
on main before this check existed (LEGACY), which nothing refers to by
number any more. It allows `Mnew` rows (a branch's) unless
--no-placeholders (for main).
"""
import os
import re
import subprocess
import sys
import tempfile

MAP = "docs/dev/TEST_MAP.md"
ROW = re.compile(r"^\| (M\d+[a-z]?|Mnew) \|", re.M)
LEGACY = {"M984", "M984b"}


def ids(text):
    return ROW.findall(text)


def highest(text):
    nums = [int(re.match(r"M(\d+)", i).group(1)) for i in ids(text) if i != "Mnew"]
    return max(nums) if nums else 0


def git(repo, *args):
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)


def highest_everywhere(repo):
    best = 0
    path = os.path.join(repo, MAP)
    if os.path.exists(path):
        best = highest(open(path, encoding="utf-8").read())
    refs = git(repo, "for-each-ref", "--format=%(refname)", "refs/heads", "refs/remotes").stdout.split()
    for ref in refs:
        r = git(repo, "show", f"{ref}:{MAP}")
        if r.returncode == 0:
            best = max(best, highest(r.stdout))
    return best


def assign(repo):
    path = os.path.join(repo, MAP)
    text = open(path, encoding="utf-8").read()
    n = highest_everywhere(repo)
    given = []

    def repl(m):
        nonlocal n
        n += 1
        given.append(f"M{n:03d}")
        return f"| M{n:03d} |"

    text = re.sub(r"^\| Mnew \|", repl, text, flags=re.M)
    open(path, "w", encoding="utf-8").write(text)
    return given


def check(text, placeholders_ok=True):
    problems = []
    seen = {}
    for i in ids(text):
        seen[i] = seen.get(i, 0) + 1
    for i, c in sorted(seen.items()):
        if i == "Mnew":
            if not placeholders_ok:
                problems.append(f"{c} row(s) still `Mnew`: run tools/test-map-ids.py assign")
        elif c > 1 and i not in LEGACY:
            problems.append(f"{i} is used by {c} rows: give the newer row `Mnew` and run "
                            "tools/test-map-ids.py assign (or `next`)")
    return problems


def selftest():
    fails = 0

    def t(name, cond):
        nonlocal fails
        print(("ok - " if cond else "not ok - ") + name)
        fails += 0 if cond else 1

    base = "| ID | a |\n|---|---|\n| M001 | x |\n| M984 | y |\n| M984 | y2 |\n| M010 | z |\n"
    t("legacy duplicates pass", check(base) == [])
    t("a new duplicate fails", len(check(base + "| M010 | again |\n")) == 1)
    t("Mnew passes on a branch", check(base + "| Mnew | n |\n") == [])
    t("Mnew fails with --no-placeholders", len(check(base + "| Mnew | n |\n", False)) == 1)
    t("highest", highest(base) == 984)
    with tempfile.TemporaryDirectory() as d:
        env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", HOME=d, GIT_AUTHOR_NAME="t",
                   GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")

        def g(*a):
            subprocess.run(["git", "-C", d, *a], check=True, capture_output=True, env=env)

        os.makedirs(os.path.join(d, "docs/dev"))
        p = os.path.join(d, MAP)
        g("init", "-q", "-b", "main")
        open(p, "w").write(base)
        g("add", "."); g("commit", "-qm", "base")
        g("checkout", "-qb", "other")
        open(p, "w").write(base + "| M990 | elsewhere |\n")
        g("commit", "-qam", "other branch takes M990")
        g("checkout", "-q", "main")
        open(p, "w").write(base + "| Mnew | one |\n| Mnew | two |\n")
        given = assign(d)
        t("assign skips ids another branch holds", given == ["M991", "M992"])
        text = open(p).read()
        t("assign leaves no Mnew", "Mnew" not in text and "| M992 | two |" in text)
    print("test-map-ids.py self-test: " + ("ok" if fails == 0 else f"{fails} failed"))
    return 1 if fails else 0


def main(argv):
    repo = os.getcwd()
    if "--repo" in argv:
        repo = argv[argv.index("--repo") + 1]
    cmd = argv[1] if len(argv) > 1 else "--help"
    if cmd == "--self-test":
        return selftest()
    if cmd == "next":
        print(f"M{highest_everywhere(repo) + 1:03d}")
        return 0
    if cmd == "assign":
        given = assign(repo)
        print("assigned: " + (" ".join(given) if given else "nothing (no Mnew rows)"))
        return 0
    if cmd == "check":
        path = os.path.join(repo, MAP)
        problems = check(open(path, encoding="utf-8").read(), "--no-placeholders" not in argv)
        for p in problems:
            print(f"{MAP}: {p}", file=sys.stderr)
        return 1 if problems else 0
    print(__doc__)
    return 0 if cmd in ("-h", "--help") else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
