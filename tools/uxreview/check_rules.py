#!/usr/bin/env python3
#
#  Q Light Controller Plus
#  check_rules.py
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0.txt
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
"""Keep docs/dev/UX_RULES.md and docs/dev/ux_rules.json in step.

Checks, with no build:
  - both files hold the same rule IDs, in the same order, each once;
  - per rule: the same title, rule sentence, checks (in order), enforced
    flag and Right/Wrong examples; the main `check` is one of `checks`;
  - every `test:X` / `test:X::Y` names a test (or script) that exists in
    the repo, and Y is a function of that test;
  - every forbid_regex compiles, matches its rule's Wrong example and does
    not match its Right one;
  - the Markdown summary counts (per theme, main check, enforced) are true.

Usage: tools/uxreview/check_rules.py [--source <repo>] [--self-test]
Exit 1 lists every problem.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

KINDS = ("review", "walker", "manual")


def parse_markdown(text):
    """Rules from UX_RULES.md as {id: {...}}, in file order, plus the summary."""
    rules = []
    cur = None
    para = []
    for line in text.splitlines():
        m = re.match(r"^### (UX-\d\d) (.+?)\s*$", line)
        if m:
            cur = {"id": m.group(1), "title": m.group(2), "rule": None,
                   "checks": None, "enforced": None, "right": None, "wrong": None}
            rules.append(cur)
            para = []
            continue
        if line.startswith("## "):
            cur = None
            continue
        if cur is None:
            continue
        b = re.match(r"^- \*\*(Why|Check|Right|Wrong):\*\* (.*)$", line)
        if b:
            key, val = b.group(1), b.group(2)
            if key == "Check":
                em = re.search(r"\((enforced|not yet enforced)\)\s*$", val)
                cur["enforced"] = None if not em else em.group(1) == "enforced"
                cur["checks"] = re.findall(r"`([^`]+)`", val)
            elif key in ("Right", "Wrong"):
                cm = re.match(r"^`(.*)`\s*$", val)
                cur[key.lower()] = cm.group(1) if cm else val
            continue
        if cur["rule"] is None:
            if line.strip():
                para.append(line.strip())
            elif para:
                cur["rule"] = " ".join(para)
                para = []
    summary = {}
    for line in text.splitlines():
        m = re.match(r"^\| ([^|]+?) \| (UX-[^|]+?) \| (\d+) \| (\d+) \|$", line)
        if m:
            summary[m.group(1)] = {"rules": [s.strip() for s in m.group(2).split(",")],
                                   "count": int(m.group(3)), "enforced": int(m.group(4))}
    mm = re.search(r"Main check: (\d+) tests, (\d+) review, (\d+) walker, (\d+) manual\. "
                   r"(\d+) of (\d+) enforced\.", text)
    main = None
    if mm:
        main = dict(zip(("test", "review", "walker", "manual", "enforced", "total"),
                        map(int, mm.groups())))
    return rules, summary, main


def md_check_to_json(c):
    return c if c in KINDS else "test:" + c


class TestIndex:
    """Which tests, test functions and scripts exist in the repo."""

    def __init__(self, repo):
        self.repo = repo
        try:
            out = subprocess.run(["git", "-C", repo, "ls-files"], capture_output=True,
                                 text=True, check=True).stdout
            self.files = out.splitlines()
        except (OSError, subprocess.CalledProcessError):
            self.files = []
            # A gate's git archive copy: walk it, skipping build trees.
            for root, dirs, names in os.walk(repo):
                dirs[:] = [d for d in dirs if not d.startswith(".")
                           and not (root == repo and d.startswith(("build", "dist")))]
                for n in names:
                    self.files.append(os.path.relpath(os.path.join(root, n), repo))
        self.by_base = {}
        for f in self.files:
            self.by_base.setdefault(os.path.basename(f), []).append(f)

    def sources(self, test):
        """The source files of a test called `test` (units_test -> units_test.cpp/.h)."""
        out = []
        for ext in (".cpp", ".h"):
            out += self.by_base.get(test + ext, [])
        return out

    def problem(self, ref):
        name, _, func = ref.partition("::")
        if name.endswith((".py", ".sh")):
            if not self.by_base.get(name):
                return "no script %s in the repo" % name
            return None
        srcs = self.sources(name)
        if not srcs:
            return "no test %s (no %s.cpp in the repo)" % (name, name)
        if func:
            pat = re.compile(r"::%s\s*\(" % re.escape(func))
            for s in srcs:
                try:
                    with open(os.path.join(self.repo, s), encoding="utf-8", errors="replace") as fh:
                        if pat.search(fh.read()):
                            return None
                except OSError:
                    pass
            return "%s has no function %s" % (name, func)
        return None


def check(repo):
    problems = []
    md_path = os.path.join(repo, "docs/dev/UX_RULES.md")
    js_path = os.path.join(repo, "docs/dev/ux_rules.json")
    try:
        with open(md_path, encoding="utf-8") as fh:
            md_rules, summary, main = parse_markdown(fh.read())
        with open(js_path, encoding="utf-8") as fh:
            js_rules = json.load(fh)
    except (OSError, ValueError) as e:
        return ["cannot read the rules: %s" % e]

    md_ids = [r["id"] for r in md_rules]
    js_ids = [r.get("id") for r in js_rules]
    for name, ids in (("UX_RULES.md", md_ids), ("ux_rules.json", js_ids)):
        dup = sorted({i for i in ids if ids.count(i) > 1})
        if dup:
            problems.append("%s: an ID given twice: %s" % (name, ", ".join(dup)))
    only_md = [i for i in md_ids if i not in js_ids]
    only_js = [i for i in js_ids if i not in md_ids]
    if only_md:
        problems.append("only in UX_RULES.md: " + ", ".join(only_md))
    if only_js:
        problems.append("only in ux_rules.json: " + ", ".join(map(str, only_js)))
    if not only_md and not only_js and md_ids != js_ids:
        problems.append("the two files list the rules in a different order")

    md = {r["id"]: r for r in md_rules}
    tests = TestIndex(repo)
    for j in js_rules:
        rid = j.get("id")
        m = md.get(rid)
        if m is None:
            continue
        for key, mkey in (("title", "title"), ("rule", "rule")):
            # Markdown may put code in backticks (`<tag>`); the JSON can't.
            if (j.get(key) or "").strip() != (m[mkey] or "").replace("`", "").strip():
                problems.append("%s: %s differs\n    md:   %s\n    json: %s"
                                % (rid, key, m[mkey], j.get(key)))
        ex = j.get("example") or {}
        for side in ("right", "wrong"):
            if (ex.get(side) or "") != (m[side] or ""):
                problems.append("%s: %s example differs\n    md:   %s\n    json: %s"
                                % (rid, side.capitalize(), m[side], ex.get(side)))
        jchecks = j.get("checks") or []
        mchecks = [md_check_to_json(c) for c in (m["checks"] or [])]
        if jchecks != mchecks:
            problems.append("%s: checks differ\n    md:   %s\n    json: %s"
                            % (rid, ", ".join(mchecks), ", ".join(jchecks)))
        if not jchecks:
            problems.append("%s: no checks" % rid)
        else:
            # `check` is the main check: one of `checks`, a test by its
            # name alone (test:units_test for test:units_test::x).
            main_check = j.get("check")
            if main_check not in jchecks and \
                    main_check not in [c.split("::")[0] for c in jchecks]:
                problems.append("%s: the main check %r is not one of its checks"
                                % (rid, main_check))
        if m["enforced"] is None:
            problems.append("%s: the Check line ends in neither (enforced) nor (not yet enforced)" % rid)
        elif bool(j.get("enforced")) != m["enforced"]:
            problems.append("%s: enforced is %s in the JSON, %s in the Markdown"
                            % (rid, j.get("enforced"), m["enforced"]))
        for c in jchecks:
            if c in KINDS:
                continue
            if not c.startswith("test:"):
                problems.append("%s: unknown check %r (test:X, review, walker or manual)" % (rid, c))
                continue
            p = tests.problem(c[5:])
            if p:
                problems.append("%s: %s" % (rid, p))
        pat = (j.get("patterns") or {}).get("forbid_regex")
        if pat is not None:
            try:
                rx = re.compile(pat, re.M)
            except re.error as e:
                problems.append("%s: forbid_regex does not compile: %s" % (rid, e))
                continue
            if not rx.search(ex.get("wrong") or ""):
                problems.append("%s: forbid_regex misses its own Wrong example: %s"
                                % (rid, ex.get("wrong")))
            if rx.search(ex.get("right") or ""):
                problems.append("%s: forbid_regex matches its own Right example: %s"
                                % (rid, ex.get("right")))

    # The Markdown summary.
    by_theme = {}
    for j in js_rules:
        by_theme.setdefault(j.get("theme"), []).append(j)
    for theme, rs in by_theme.items():
        s = summary.get(theme)
        if s is None:
            problems.append("summary: no row for theme %r" % theme)
            continue
        ids = [r["id"] for r in rs]
        if s["rules"] != ids or s["count"] != len(rs) or \
                s["enforced"] != sum(1 for r in rs if r.get("enforced")):
            problems.append("summary: the %r row is %s / %d / %d enforced; the rules say %s / %d / %d"
                            % (theme, ",".join(s["rules"]), s["count"], s["enforced"],
                               ",".join(ids), len(rs), sum(1 for r in rs if r.get("enforced"))))
    for theme in summary:
        if theme not in by_theme:
            problems.append("summary: a row for theme %r that no rule has" % theme)
    if main is None:
        problems.append("summary: no 'Main check: N tests, N review, N walker, N manual. N of N enforced.' line")
    else:
        kinds = {"test": 0, "review": 0, "walker": 0, "manual": 0}
        for j in js_rules:
            k = (j.get("check") or "").split(":")[0]
            if k in kinds:
                kinds[k] += 1
        want = dict(kinds, enforced=sum(1 for j in js_rules if j.get("enforced")), total=len(js_rules))
        if want != main:
            problems.append("summary: the Main check line says %s; the rules say %s" % (main, want))
    return problems


def self_test(repo):
    """The check passes on the real files and catches each kind of drift."""
    if check(repo):
        print("check_rules self-test: the real rules do not pass; run without --self-test")
        return False
    tmp = tempfile.mkdtemp(prefix="uxrules-selftest-")
    try:
        os.makedirs(os.path.join(tmp, "docs/dev"))
        md_src = open(os.path.join(repo, "docs/dev/UX_RULES.md"), encoding="utf-8").read()
        js_src = json.load(open(os.path.join(repo, "docs/dev/ux_rules.json"), encoding="utf-8"))
        # Tests the fake repo knows: every real test source, empty but for
        # its functions' names, so test lookups behave as in the repo.
        idx = TestIndex(repo)
        for f in idx.files:
            if f.endswith(("_test.cpp", "_test.h", ".py", ".sh")):
                dst = os.path.join(tmp, f)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copyfile(os.path.join(repo, f), dst)

        def run(md, js):
            open(os.path.join(tmp, "docs/dev/UX_RULES.md"), "w", encoding="utf-8").write(md)
            open(os.path.join(tmp, "docs/dev/ux_rules.json"), "w", encoding="utf-8").write(json.dumps(js))
            return check(tmp)

        cases = []
        j = json.loads(json.dumps(js_src)); j[0]["title"] += " x"
        cases.append(("title drift", md_src, j))
        j = json.loads(json.dumps(js_src)); j.pop()
        cases.append(("rule missing from json", md_src, j))
        j = json.loads(json.dumps(js_src))
        j[0]["checks"] = [j[0]["checks"][0] + "Nope"] + j[0]["checks"][1:]
        cases.append(("unknown test function", md_src.replace(js_src[0]["checks"][0][5:] + "`",
                                                              js_src[0]["checks"][0][5:] + "Nope`", 1), j))
        j = json.loads(json.dumps(js_src))
        withre = next(r for r in j if (r.get("patterns") or {}).get("forbid_regex"))
        withre["patterns"]["forbid_regex"] = "(unclosed"
        cases.append(("regex does not compile", md_src, j))
        j = json.loads(json.dumps(js_src))
        withre = next(r for r in j if (r.get("patterns") or {}).get("forbid_regex"))
        withre["patterns"]["forbid_regex"] = "nothing-matches-this"
        cases.append(("regex misses its wrong example", md_src, j))
        cases.append(("summary count", md_src.replace("Main check: ", "Main check: 1", 1), js_src))
        ok = True
        for name, md, js in cases:
            if not run(md, js):
                print("check_rules self-test: missed %s" % name)
                ok = False
        return ok
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv):
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    if "--source" in argv:
        repo = os.path.abspath(argv[argv.index("--source") + 1])
    if "--self-test" in argv:
        return 0 if self_test(repo) else 1
    problems = check(repo)
    if problems:
        print("check_rules: docs/dev/UX_RULES.md and ux_rules.json are out of step "
              "(%d problem%s):" % (len(problems), "" if len(problems) == 1 else "s"))
        for p in problems:
            print("  " + p)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
