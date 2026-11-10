#!/usr/bin/env python3
#
#  Q Light Controller Plus
#  check-course-ui-names.py
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
"""Fail when a lesson tells students to click a name the app doesn't have.

The course writes every UI name in bold (**Add Fixture…**), and a menu path
as bold names joined by ▸ or → (**File ▸ Open**); a trailing ▾ marks a
drop-down button. This reads those names out of docs/course/level-*/lesson-*
(student, handout, teacher guide, index, ...) and checks each one, with its
case and its trailing "…", against every string the app can show: the string
literals in ui/src, engine/src and webaccess/res, and the .ui files
(& accelerators removed, %1 as a wildcard). Lesson inline code (`...`) is
file names, show content and typed text, not UI, so it is not checked.

The leading words of a longer label count (a lesson says Auto-open
shutters for "Auto-open shutters when a light is given a level").
A name with no match is listed as file:line with the nearest app string.
Names that are not app UI -- keyboard keys, fixture/cue/group names of the
course shows, labels, real-world hardware -- go in ALLOW below, with a
reason. EXPECTED_STALE names are known-wrong lesson text whose fix is on
another branch: they only warn, and an entry that is no longer found fails
(delete it).

Usage: tools/check-course-ui-names.py [repo-root] | --self-test
       --raw  ignore the two stale lists: every unmatched name
Exit 1 on any unmatched name.
"""
import difflib
import glob
import os
import re
import sys

# Not app UI. One reason per group; keep it short.
ALLOW_GROUPS = {
    "keyboard keys": ["Ctrl", "Esc", "T", "Shift", "Alt", "Cmd", "Option",
                      "Tab", "Enter", "Return", "Space", "Delete", "⇧", "⌘", "⌥"],
    "the operating system or GitHub, not qlcconsole": ["Documents"],
    "real-world hardware and file formats": ["USITT ASCII"],
    "names in the course shows (groups, palettes, cues, fixtures, layers)": [
        "TrussC-RGB", "Lanterns", "Trusses SR", "Truss-Spots",
        "1. Truss-Spots", "01-Everybody Scream", "Aim Center", "Center",
        "SR Boom", "Spots, washes, beams, lanterns", "Chorus Colour",
        "Truss Sides (Trusses SR + Trusses SL)",
    ],
    "a section header the lesson spells out, not a button": ["1 — …"],
}
ALLOW = set()
for _names in ALLOW_GROUPS.values():
    ALLOW.update(_names)
# Cue numbers with their names (1.0.2.5 Flash) and bare values (−5, 40%).
ALLOW_RE = re.compile(r"^(\d+(\.\d+)+ .+|[−+-]?\d+%?)$")

# Stale lesson text that this check found and that no course branch fixes
# yet. Fix the lesson, then delete the entry (the check fails once an entry
# no longer occurs in the lessons).
STALE_UNFIXED = set()

# Known-wrong lesson text whose fix is on a course branch not yet merged.
# Delete each entry when its branch merges.
EXPECTED_STALE = set()

SEP = re.compile(r"\s*[▸→]\s*")
BOLD = re.compile(r"\*\*((?:(?!\n[ \t]*\n).)+?)\*\*", re.S)
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
# A comment or a string literal, whichever starts first: comments are dropped
# ("Add fixture..." in a // note is not an app string), literals kept.
COMMENT = re.compile(r'"(?:[^"\\\n]|\\.)*"|//[^\n]*|/\*.*?\*/', re.S)
UI_STRING = re.compile(r"<string[^>]*>(.*?)</string>", re.S)


def norm_app(s):
    """An app string as the user reads it."""
    s = s.replace("\\u2026", "…").replace("\\342\\200\\246", "…").replace("...", "…")
    s = s.replace("\\n", " ").replace('\\"', '"').replace("\\\\", "\\")
    s = s.replace("&&", "\0").replace("&", "").replace("\0", "&")
    s = s.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
    return re.sub(r"\s+", " ", s).strip()


def norm_name(s):
    s = s.replace("...", "…").replace("&amp;", "&")
    s = s.replace("▾", "").replace("⋯", "").replace(" ", " ")
    return re.sub(r"\s+", " ", s).strip()


def _keep_literals(m):
    return m.group(0) if m.group(0).startswith('"') else " "


def collect_app(root):
    """(exact set, placeholder regexes, list of all strings for suggestions)"""
    files = []
    for sub in ("ui/src", "engine/src", "webaccess/res", "main", "plugins"):
        for dp, dn, fn in os.walk(os.path.join(root, sub)):
            dn[:] = [d for d in dn if d not in ("test", "build")]
            for f in fn:
                if f.endswith((".cpp", ".h", ".ui", ".html", ".js")):
                    files.append(os.path.join(dp, f))
    exact, pats = set(), []
    for path in files:
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        if not path.endswith((".ui", ".html", ".js")):
            text = COMMENT.sub(_keep_literals, text)
        found = UI_STRING.findall(text) if path.endswith(".ui") \
            else LITERAL.findall(text)
        for lit in found:
            s = norm_app(lit)
            if not s or len(s) > 120:
                continue
            if re.search(r"%L?\d|%n", s):
                if len(re.sub(r"%L?\d+|%n", "", s).strip()) < 10:
                    continue  # a short pattern such as "Edit %1…" would match anything
                rx = "".join(".+" if p is None else re.escape(p)
                             for p in _split_ph(s))
                pats.append(re.compile("^" + rx + "$"))
            exact.add(s)
    return exact, pats


def _split_ph(s):
    out = []
    for i, part in enumerate(re.split(r"%L?\d+|%n", s)):
        if i:
            out.append(None)
        out.append(part)
    return out


def lesson_files(root):
    out = []
    for p in sorted(glob.glob(os.path.join(root, "docs/course/level-*/lesson-*/*.md"))):
        out.append(p)
    return out


def extract(path):
    """Yield (line number, name, strict) for every UI name in one lesson file.
    Bold may wrap over lines; it never crosses a blank line."""
    text = open(path, encoding="utf-8").read()
    blank = lambda m: re.sub(r"[^\n]", " ", m.group(0))  # keeps line numbers
    text = re.sub(r"^\s*```.*?^\s*```", blank, text, flags=re.S | re.M)
    text = re.sub(r"`[^`\n]*`", blank, text)
    for m in BOLD.finditer(text):
        n = text.count("\n", 0, m.start()) + 1
        parts = SEP.split(m.group(1))
        for seg in parts:
            strict = len(parts) > 1 or seg.rstrip().endswith(("…", "▾", "..."))
            seg = norm_name(seg)
            if seg:
                yield n, seg, strict
    # the "path=" attributes of the keys table, if a lesson has one
    for n, line in enumerate(text.split("\n"), 1):
        for m in re.finditer(r'\bpath="([^"]+)"', line):
            for seg in SEP.split(m.group(1)):
                seg = norm_name(seg)
                if seg:
                    yield n, seg, True


def is_structural(name):
    """Bold that labels the page, not the app: 'Steps', 'Stuck?', 'Paper:'."""
    return name.endswith((":", "?")) or name in STRUCTURAL


STRUCTURAL = {
    "Steps", "Teacher guide", "Student handout", "Give students", "Paper",
    "Hands-on", "Teacher demo", "Design idea", "Exit ticket", "App",
    "Builds on", "Key idea to land", "Check / share", "On Windows or Linux",
    "On a Mac", "Driver", "Navigator", "Designer", "Driver / Navigator / Designer",
}


def fold(s):
    """Case and punctuation dropped: the shape of a name, not its spelling."""
    return re.sub(r"[^\w]+", " ", s.lower()).strip()


def matches(name, exact, pats):
    if name in exact or name + "…" in exact or name + ":" in exact:
        return True   # a lesson may leave off the "…" of "Save As…", the ":" of "Room:"
    if " " in name and any(a.startswith(name) and a[len(name)] in " :(…"
                           for a in exact if len(a) > len(name)):
        return True   # two or more words of a longer label (Auto-open shutters)
    return any(p.match(name) for p in pats)


def check(root, use_lists=True):
    expected = (EXPECTED_STALE | STALE_UNFIXED) if use_lists else set()
    exact, pats = collect_app(root)
    folded = {}
    for a in exact:
        folded.setdefault(fold(a), a)
    hits, seen_stale = [], set()
    for path in lesson_files(root):
        rel = os.path.relpath(path, root)
        for n, name, strict in extract(path):
            if name in ALLOW or ALLOW_RE.match(name) or is_structural(name) or matches(name, exact, pats):
                continue
            if not strict:
                continue
            if name in expected:
                seen_stale.add(name)
                continue
            near = folded.get(fold(name)) or next(iter(difflib.get_close_matches(name, exact, 1, 0.6)), "")
            hits.append((rel, n, name, near))
    return hits, expected - seen_stale


def self_test():
    assert norm_app("&Add Fixture...") == "Add Fixture…"
    assert norm_app("Save && Quit") == "Save & Quit"
    assert norm_name("Add ▾") == "Add"
    assert [s for s in SEP.split("File ▸ Open → Recent")] == ["File", "Open", "Recent"]
    ex, pats = {"Add Fixture…"}, [re.compile("^Patch .+ Fixtures$")]
    assert matches("Add Fixture…", ex, pats) and not matches("Add fixture…", ex, pats)
    assert matches("Add Fixture", ex, pats)  # the "…" may be left off
    assert not matches("Add Fixture…", {"Add Fixture"}, [])  # not added
    assert matches("Patch 4 Fixtures", ex, pats)
    assert is_structural("Stuck?") and not is_structural("Show Tree")
    print("check-course-ui-names: self-test passed")


def main():
    if "--self-test" in sys.argv:
        self_test()
        return 0
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    root = os.path.abspath(args[0] if args else
                           os.path.join(os.path.dirname(__file__), ".."))
    hits, gone = check(root, "--raw" not in sys.argv)
    for rel, n, name, near in hits:
        print("%s:%d: no app string \"%s\"%s" % (
            rel, n, name, (" -- nearest: \"%s\"" % near) if near else ""))
    for name in sorted(gone):
        print("EXPECTED_STALE \"%s\" no longer occurs in the lessons: delete it" % name)
    if hits or gone:
        print("check-course-ui-names: %d unmatched lesson UI name(s)" % len(hits))
        return 1
    print("check-course-ui-names: every lesson UI name exists in the app")
    return 0


if __name__ == "__main__":
    sys.exit(main())
