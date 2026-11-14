#!/usr/bin/env python3
"""Merge-train helpers: resolve "both added here" conflicts, and check a merge.

    tools/merge-check.py resolve FILE...  during a conflicted merge: every
                                hunk where both sides only ADDED lines at the
                                same place (two tests appended to one file,
                                two slots declared after the same one) gets
                                ours then theirs; any other hunk is left as a
                                conflict. A file with none left is checked
                                (below) and `git add`ed.
    tools/merge-check.py check [REV]   the merge in progress, or merge commit
                                REV (default HEAD): no conflict marker in any
                                file it changed, and every C/C++/JS file it
                                changed still balances { } ( ) [ ] when both
                                parents did.
    tools/merge-check.py --self-test

Why: on 2026-11-13 train conflicts were "resolved" by deleting marker lines
with sed; a zsh word-splitting slip skipped one file, and one resolution
dropped a closing brace that only the full gate (20 min) found. Resolve
with this, run `check` before committing the merge (tools/githooks/
pre-commit does it when core.hooksPath=tools/githooks), and build the
merged train with `check-all.sh --strict` before its full gate.

CHANGELOG.md and docs/dev/TEST_MAP.md need none of this: .gitattributes
merges them with git's union driver (both sides' lines kept).
"""
import os
import re
import subprocess
import sys
import tempfile

CODE = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".mm", ".m", ".js", ".qml")
MARK = re.compile(r"^(<{7} |>{7} |\|{7} |<{7}$|>{7}$)", re.M)
SEP = re.compile(r"^={7}$", re.M)
SELF = os.path.abspath(__file__)


def git(*args, check=False):
    r = subprocess.run(["git", *args], capture_output=True, text=True, errors="replace")
    if check and r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r


def markers(text):
    """Line numbers of conflict markers (a lone ======= only with others)."""
    lines = []
    found = MARK.search(text) is not None
    for n, line in enumerate(text.split("\n"), 1):
        if MARK.match(line) or (found and SEP.match(line)):
            lines.append(n)
    return lines


def balance(text):
    """{ } ( ) [ ] balance outside comments, strings and char literals."""
    out = {"{}": 0, "()": 0, "[]": 0}
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == "R" and text.startswith('R"', i):  # C++ raw string R"d(...)d"
            m = re.match(r'R"([^ ()\\]{0,16})\(', text[i:])
            if m:
                end = text.find(")" + m.group(1) + '"', i)
                i = n if end < 0 else end + len(m.group(1)) + 2
                continue
        if c in "\"'`":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                elif text[j] == "\n" and c != "`":
                    break
                j += 1
            i = j + 1
            continue
        for k in out:
            if c == k[0]:
                out[k] += 1
            elif c == k[1]:
                out[k] -= 1
        i += 1
    return out


def show(rev, path):
    r = git("show", f"{rev}:{path}")
    if r.returncode != 0 or "\0" in r.stdout or "\ufffd" in r.stdout[:4096]:
        return None   # gone on that side, or binary
    return r.stdout


def problems_in(path, merged, ours, theirs):
    probs = []
    m = markers(merged)
    if m:
        probs.append(f"{path}: conflict marker(s) at line(s) {', '.join(map(str, m[:8]))}")
    elif path.endswith(CODE) and ours is not None and theirs is not None:
        bo, bt, bm = balance(ours), balance(theirs), balance(merged)
        for k in bm:
            if bo[k] == bt[k] and bm[k] != bo[k]:
                probs.append(f"{path}: '{k[0]}' '{k[1]}' balance is {bm[k]:+d} after the merge, "
                             f"{bo[k]:+d} on both sides before it (a lost or extra '{k[1] if bm[k] > bo[k] else k[0]}'?)")
    return probs


def check(rev="HEAD"):
    if git("rev-parse", "-q", "--verify", "MERGE_HEAD").returncode == 0:
        ours, theirs = "HEAD", "MERGE_HEAD"
        files = set(git("diff", "--name-only", "HEAD").stdout.split())
        files |= set(git("diff", "--name-only", "--cached", "HEAD").stdout.split())
        files |= set(git("diff", "--name-only", "--diff-filter=U").stdout.split())
        read = lambda p: open(p, encoding="utf-8", errors="replace").read() if os.path.isfile(p) else None
        what = "the merge in progress"
    else:
        if git("rev-parse", "-q", "--verify", f"{rev}^2").returncode != 0:
            print(f"merge-check: {rev} is not a merge commit (and no merge is in progress)")
            return 0
        ours, theirs = f"{rev}^1", f"{rev}^2"
        files = set(git("diff", "--name-only", ours, rev).stdout.split())
        read = lambda p: show(rev, p)
        what = f"merge {git('rev-parse', '--short', rev).stdout.strip()}"
    probs = []
    for p in sorted(files):
        merged = read(p)
        if merged is None:
            continue
        probs += problems_in(p, merged, show(ours, p), show(theirs, p))
    for p in probs:
        print("merge-check: " + p, file=sys.stderr)
    print(f"merge-check: {what}: {len(files)} file(s), " + ("ok" if not probs else f"{len(probs)} problem(s)"))
    return 1 if probs else 0


HUNK = re.compile(r"^<{7}[^\n]*\n(.*?)^\|{7}[^\n]*\n(.*?)^={7}\n(.*?)^>{7}[^\n]*(?:\n|$)", re.S | re.M)


def resolve_text(text):
    """Both-added hunks -> ours + theirs. Returns (text, resolved, left)."""
    resolved = 0

    def repl(m):
        nonlocal resolved
        ours, base, theirs = m.group(1), m.group(2), m.group(3)
        if base.strip() == "":
            resolved += 1
            return ours + theirs
        return m.group(0)

    text = HUNK.sub(repl, text)
    left = len(re.findall(r"^<{7}", text, re.M))
    return text, resolved, left


def resolve(paths):
    rc = 0
    for p in paths:
        if git("checkout", "--conflict=diff3", "--", p).returncode != 0:
            print(f"merge-check: {p}: not a conflicted file in a merge", file=sys.stderr)
            rc = 1
            continue
        text = open(p, encoding="utf-8").read()
        text, done, left = resolve_text(text)
        open(p, "w", encoding="utf-8").write(text)
        if left:
            print(f"merge-check: {p}: {done} both-added hunk(s) resolved, {left} real conflict(s) left for you")
            rc = 1
            continue
        probs = problems_in(p, text, show("HEAD", p), show("MERGE_HEAD", p))
        if probs:
            for q in probs:
                print("merge-check: " + q, file=sys.stderr)
            print(f"merge-check: {p}: resolved but not added -- look at it")
            rc = 1
            continue
        git("add", "--", p, check=True)
        print(f"merge-check: {p}: {done} hunk(s) resolved (ours, then theirs), added")
    return rc


def selftest():
    fails = 0

    def t(name, cond):
        nonlocal fails
        print(("ok - " if cond else "not ok - ") + name)
        fails += 0 if cond else 1

    t("markers found", markers("a\n<<<<<<< HEAD\nb\n=======\nc\n>>>>>>> x\n") == [2, 4, 6])
    t("a setext ======= alone is not a marker", markers("Title\n=======\n") == [])
    t("balance ignores strings and comments",
      balance('f() { s = "}"; c = \'{\'; // }\n /* { */ R"x(})x"; }') == {"{}": 0, "()": 0, "[]": 0})
    t("a lost brace is seen", problems_in("a.cpp", "void f() {\n", "void f() {\n}\n", "void f() {\n}\n") != [])
    t("a file unbalanced on both sides is not blamed",
      problems_in("a.h", "#if A\n{ x\n", "#if A\n{\n", "#if A\n{ y\n") == [])
    with tempfile.TemporaryDirectory() as d:
        env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", HOME=d, GIT_AUTHOR_NAME="t",
                   GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
        cwd = os.getcwd()
        os.chdir(d)
        try:
            def g(*a):
                return subprocess.run(["git", *a], capture_output=True, text=True, env=env)
            g("init", "-q", "-b", "main")
            base = "void T::a()\n{\n    QVERIFY(1);\n}\n\nQTEST_MAIN(T)\n"
            open("t.cpp", "w").write(base)
            open("t.h", "w").write("private slots:\n    void a();\n};\n")
            g("add", "."); g("commit", "-qm", "base")
            g("checkout", "-qb", "b1")
            open("t.cpp", "w").write(base.replace("\nQTEST", "\nvoid T::b()\n{\n    QVERIFY(2);\n}\n\nQTEST"))
            open("t.h", "w").write("private slots:\n    void a();\n    void b();\n};\n")
            g("commit", "-qam", "b")
            g("checkout", "-q", "main"); g("checkout", "-qb", "c1")
            open("t.cpp", "w").write(base.replace("\nQTEST", "\nvoid T::c()\n{\n    QVERIFY(3);\n}\n\nQTEST"))
            open("t.h", "w").write("private slots:\n    void a();\n    void c();\n};\n")
            g("commit", "-qam", "c")
            g("checkout", "-q", "b1")
            env["GIT_EDITOR"] = "true"
            g("merge", "-q", "--no-edit", "c1")
            t("both appended: a conflict", "<<<<<<<" in open("t.cpp").read())
            out = subprocess.run([sys.executable, SELF,
                                  "resolve", "t.cpp", "t.h"], capture_output=True, text=True, env=env)
            merged = open("t.cpp").read()
            t("resolve keeps both functions, ours first",
              out.returncode == 0 and "<<<<<<<" not in merged and merged.index("T::b") < merged.index("T::c"))
            t("and the header's two slots", "void b();\n    void c();" in open("t.h").read())
            t("the result balances", balance(merged)["{}"] == 0)
            g("commit", "-qm", "merge")
            out = subprocess.run([sys.executable, SELF,
                                  "check"], capture_output=True, text=True, env=env)
            t("check passes the merge", out.returncode == 0)
            # A merge commit with a dropped brace.
            open("t.cpp", "w").write(merged.replace("    QVERIFY(3);\n}\n", "    QVERIFY(3);\n", 1))
            g("commit", "-q", "--amend", "-am", "merge, brace lost")
            out = subprocess.run([sys.executable, SELF,
                                  "check"], capture_output=True, text=True, env=env)
            t("check fails a merge that lost a brace", out.returncode == 1 and "balance" in out.stderr)
            # The repo's .gitattributes: two branches each add a CHANGELOG line.
            attrs = os.path.join(os.path.dirname(os.path.dirname(SELF)), ".gitattributes")
            g("checkout", "-q", "main")
            open(".gitattributes", "w").write(open(attrs).read())
            open("CHANGELOG.md", "w").write("### Fixed\n\n- one\n\n## [0.1]\n")
            g("add", "."); g("commit", "-qm", "cl")
            g("checkout", "-qb", "x1")
            open("CHANGELOG.md", "w").write("### Fixed\n\n- one\n- two\n\n## [0.1]\n")
            g("commit", "-qam", "two")
            g("checkout", "-q", "main"); g("checkout", "-qb", "x2")
            open("CHANGELOG.md", "w").write("### Fixed\n\n- one\n- three\n\n## [0.1]\n")
            g("commit", "-qam", "three")
            r = g("merge", "-q", "--no-edit", "x1")
            t("CHANGELOG.md: both lines kept, no conflict (.gitattributes union)",
              r.returncode == 0 and open("CHANGELOG.md").read() == "### Fixed\n\n- one\n- three\n- two\n\n## [0.1]\n")
        finally:
            os.chdir(cwd)
    print("merge-check.py self-test: " + ("ok" if fails == 0 else f"{fails} failed"))
    return 1 if fails else 0


def main(argv):
    cmd = argv[1] if len(argv) > 1 else "--help"
    if cmd == "--self-test":
        return selftest()
    if cmd == "check":
        return check(argv[2] if len(argv) > 2 else "HEAD")
    if cmd == "resolve":
        if len(argv) < 3:
            print("usage: tools/merge-check.py resolve FILE...", file=sys.stderr)
            return 2
        return resolve(argv[2:])
    print(__doc__)
    return 0 if cmd in ("-h", "--help") else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
