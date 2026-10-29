#!/usr/bin/env python3
#
#  Q Light Controller Plus
#  check-doc-links.py
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
"""Fail on a dead relative link in any tracked Markdown file.

Checks every relative link target ([text](path), ![alt](path), [ref]: path,
href="path", src="path") in every .md file git tracks, outside code spans
and fenced code blocks, and reports each one whose file or folder doesn't
exist. Anchors (#...) are not checked here: helpbrowser_test checks the user
guide's headings. Links to the course site's lesson pages (a Jekyll
permalink such as ../1-1/) and the course's lesson template (placeholder
links such as lesson-N-N-start.qlcc) are skipped.

Usage: tools/check-doc-links.py [repo-root]     exit 1 on any dead link
"""
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                       os.path.join(os.path.dirname(__file__), ".."))

LINK = re.compile(r'\]\(\s*<?([^)\s>]+)>?(?:\s+"[^"]*")?\s*\)'
                  r'|^\s*\[[^\]]+\]:\s+<?(\S+?)>?(?:\s|$)'
                  r'|(?:href|src)="([^"]+)"', re.M)
FENCE = re.compile(r'^\s*(```|~~~)')
CODESPAN = re.compile(r'(`+)(?:(?!\1).)+?\1')


SKIP_DIRS = (".git", "dist", "scratchpad", "node_modules", "test-workspaces")


def tracked_md():
    """The tracked .md files; in a git archive (the gate pool's copy), every
    .md outside build dirs."""
    git = subprocess.run(["git", "-C", ROOT, "rev-parse", "--show-toplevel"],
                         capture_output=True, text=True)
    if git.returncode == 0 and os.path.samefile(git.stdout.strip(), ROOT):
        out = subprocess.run(["git", "-C", ROOT, "ls-files", "-z", "*.md"],
                             check=True, capture_output=True, text=True).stdout
        return [f for f in out.split("\0") if f]
    found = []
    for d, subdirs, files in os.walk(ROOT):
        subdirs[:] = [s for s in subdirs
                      if not s.startswith("build") and s not in SKIP_DIRS]
        found += [os.path.relpath(os.path.join(d, f), ROOT)
                  for f in files if f.endswith(".md")]
    return sorted(found)


def strip_code(text):
    lines, fenced = [], False
    for line in text.split("\n"):
        if FENCE.match(line):
            fenced = not fenced
            lines.append("")
            continue
        lines.append("" if fenced else CODESPAN.sub("", line))
    return "\n".join(lines)


def is_permalink(target):
    # The course site's lesson pages: Jekyll permalinks, not files.
    return re.match(r'^(\.\./)*\d+-[a-z0-9-]+/?$', target) is not None


def main():
    dead = []
    mds = tracked_md()
    for md in mds:
        if "/_template/" in md:
            continue  # the course's lesson template: placeholder links
        path = os.path.join(ROOT, md)
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            text = strip_code(f.read())
        for m in LINK.finditer(text):
            target = next(g for g in m.groups() if g is not None)
            if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:', target) or \
               target.startswith(("#", "/", "{{", "{%")):
                continue
            target = target.split("#", 1)[0].split("?", 1)[0]
            if not target or is_permalink(target):
                continue
            target = re.sub(r'%20', ' ', target)
            full = os.path.normpath(os.path.join(os.path.dirname(path), target))
            if not os.path.exists(full):
                line = text.count("\n", 0, m.start()) + 1
                dead.append("%s:%d: %s" % (md, line, target))
    for d in dead:
        print("dead link:", d)
    print("check-doc-links: %d dead link(s) in %d Markdown files"
          % (len(dead), len(mds)))
    return 1 if dead else 0


if __name__ == "__main__":
    sys.exit(main())
