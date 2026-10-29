#!/usr/bin/env python3
"""Check the student pages, and make their QR codes.

Each lesson's student page (/class/N-N/ on the course site) is built by
Jekyll from the lesson's student*.md with the layout _layouts/student.html.
This script checks that every one of them:

- uses the student layout, has a /class/ permalink of its own and is kept
  out of the site's navigation and search;
- points only at pictures and show files that exist;
- is linked from its teacher guide ("Give students: ...") with a QR code;
- gives every "In the app" box (_includes/inapp.html) what it needs: what
  it's for, and a menu path or a shortcut.

It also reports each page's word count against its handout.

    python3 docs/course/tools/student-pages.py          # check
    python3 docs/course/tools/student-pages.py --qr     # (re)make images/qr/*.svg
    python3 docs/course/tools/student-pages.py --site _site   # also check a build

--site checks a Jekyll build of the course (built with --baseurl /qlcconsole,
as Pages does): no teacher material in it or in its search index (teacher
guides, slides outlines, the course plan, volunteer pages and _template/ are
read on GitHub only), and no internal link or picture that goes nowhere.

--qr needs segno (pip install segno). Exits 1 on any problem.
"""

import os
import re
import sys

SITE = "https://sandinak.github.io/qlcconsole"
BASEURL = "/qlcconsole"
# Source pages kept off the site (_config.yml exclude:), read on GitHub.
TEACHER = re.compile(r"(^|/)(teacher-guide|slides|COURSE_PLAN|VOLUNTEER)\.(md|html)$|(^|/)_?template/")
COURSE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Student pages that aren't lessons: no teacher guide, no QR code.
NOT_LESSONS = ("install", "basics")
INAPP = re.compile(r"\{%-?\s*include\s+inapp\.html\s+(.*?)-?%\}", re.S)
INAPP_KEYS = {"do", "path", "winpath", "mac", "win", "see"}


def front_matter(text):
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not m:
        return {}, text
    keys = {}
    for line in m.group(1).splitlines():
        km = re.match(r"([a-z_]+):\s*(.*)$", line)
        if km:
            keys[km.group(1)] = km.group(2).strip().strip('"')
    return keys, text[m.end():]


def words(text):
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)  # pictures
    text = re.sub(r"\]\([^)]*\)", "]", text)           # link targets
    return len(re.findall(r"[A-Za-z0-9][\w'’.-]*", text))


def student_pages():
    for root, dirs, files in os.walk(COURSE):
        dirs[:] = sorted(d for d in dirs if not d.startswith("_"))  # _template/
        for f in sorted(files):
            if f.endswith(".md") and (f.startswith("student") or root.endswith("/class")):
                yield os.path.join(root, f)


def slug(permalink):
    return permalink.strip("/").split("/")[-1]


def check_site(site):
    """Problems in a built site: teacher material, or links that go nowhere."""
    import html.parser
    import json
    from urllib.parse import unquote, urlsplit

    problems = []
    for root, dirs, files in os.walk(COURSE):
        dirs[:] = [d for d in dirs if d not in ("tools",)]
        for f in files:
            rel = os.path.relpath(os.path.join(root, f), COURSE)
            built = os.path.join(site, rel[:-3] + ".html") if rel.endswith(".md") else None
            if TEACHER.search(rel) and built and os.path.exists(built):
                problems.append(f"site: teacher page {rel} is built ({os.path.relpath(built, site)})")
    index = os.path.join(site, "assets", "js", "search-data.json")
    for doc in json.load(open(index, encoding="utf-8")).values():
        if TEACHER.search(doc["url"].split("#")[0]) or re.search(r"Teacher guide · Level|Give students:", doc["content"]):
            problems.append(f"site: search index has teacher text ({doc['url']})")

    class Links(html.parser.HTMLParser):
        def __init__(self):
            super().__init__()
            self.targets = []

        def handle_starttag(self, tag, attrs):
            for k, v in attrs:
                if k in ("href", "src") and v:
                    self.targets.append(v)

    for root, dirs, files in os.walk(site):
        dirs[:] = [d for d in dirs if d != "assets"]
        for f in files:
            if not f.endswith(".html"):
                continue
            page = os.path.join(root, f)
            parser = Links()
            parser.feed(open(page, encoding="utf-8").read())
            for target in parser.targets:
                url = urlsplit(target)
                if url.scheme or url.netloc or not url.path:
                    continue
                path = unquote(url.path)
                if path.startswith(BASEURL + "/"):
                    path = os.path.join(site, path[len(BASEURL) + 1:])
                elif path.startswith("/"):
                    path = os.path.join(site, path[1:])
                else:
                    path = os.path.join(root, path)
                if path.endswith("/"):
                    path += "index.html"
                if not (os.path.isfile(path) or os.path.isfile(path + ".html")
                        or os.path.isfile(os.path.join(path, "index.html"))):
                    problems.append(f"site: {os.path.relpath(page, site)}: {target} goes nowhere")
    return problems


def main():
    make_qr = "--qr" in sys.argv
    problems = []
    seen = {}
    pages = list(student_pages())
    for path in pages:
        rel = os.path.relpath(path, COURSE)
        text = open(path, encoding="utf-8").read()
        fm, body = front_matter(text)
        link = fm.get("permalink", "")
        if fm.get("layout") != "student":
            problems.append(f"{rel}: layout is not 'student'")
        if not re.fullmatch(r"/class/[a-z0-9-]+/", link):
            problems.append(f"{rel}: permalink {link!r} is not /class/<name>/")
        if link in seen:
            problems.append(f"{rel}: permalink {link} also used by {seen[link]}")
        seen[link] = rel
        for key in ("nav_exclude", "search_exclude"):
            if fm.get(key) != "true":
                problems.append(f"{rel}: needs {key}: true")
        for f in re.findall(r"\[([^\]]+)\]", fm.get("files", "")):
            for name in f.split(","):
                if not os.path.isfile(os.path.join(COURSE, "shows", name.strip())):
                    problems.append(f"{rel}: files: shows/{name.strip()} is missing")
        for target in re.findall(r"\]\(([^)\s]+)\)", body):
            if target.startswith(("http", "#")) or target.endswith("/"):
                continue
            if not os.path.isfile(os.path.normpath(os.path.join(os.path.dirname(path), target))):
                problems.append(f"{rel}: {target} is missing")
        for box in INAPP.findall(body):
            args = dict(re.findall(r'(\w+)="([^"]*)"', box))
            if set(args) - INAPP_KEYS:
                problems.append(f"{rel}: inapp box: unknown {sorted(set(args) - INAPP_KEYS)}")
            if not args.get("do") or not (args.get("path") or args.get("mac") or args.get("win")):
                problems.append(f"{rel}: inapp box needs do= and a path= or a shortcut ({box.strip()[:40]}…)")
        if re.search(r"\]\([^)]*(teacher-guide|slides|COURSE_PLAN|VOLUNTEER)", body):
            problems.append(f"{rel}: links to teacher material")

        name = slug(link)
        handout = os.path.join(os.path.dirname(path), "handout.md")
        n = words(body)
        if os.path.basename(path) == "student.md" and os.path.isfile(handout):
            h = words(front_matter(open(handout, encoding="utf-8").read())[1])
            print(f"{link:16} {n:4} words (handout {h})")
        else:
            print(f"{link:16} {n:4} words")

        qr = os.path.join(COURSE, "images", "qr", f"class-{name}.svg")
        if make_qr and name not in NOT_LESSONS:
            import segno
            os.makedirs(os.path.dirname(qr), exist_ok=True)
            segno.make(f"{SITE}{link}", error="m").save(qr, scale=4, border=2, light="white", xmldecl=False)
        if name not in NOT_LESSONS:
            if not os.path.isfile(qr):
                problems.append(f"{rel}: images/qr/class-{name}.svg is missing (run with --qr)")
            guide = os.path.join(os.path.dirname(path), "teacher-guide.md")
            if os.path.isfile(guide) and f"{SITE}{link}" not in open(guide, encoding="utf-8").read():
                problems.append(f"{os.path.relpath(guide, COURSE)}: no 'Give students' link to {link}")

    if "--site" in sys.argv:
        problems += check_site(sys.argv[sys.argv.index("--site") + 1])

    for p in problems:
        print("PROBLEM:", p)
    print(f"{len(pages)} student pages, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
