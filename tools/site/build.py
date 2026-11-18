#!/usr/bin/env python3
#
# Builds the content of qlcconsole.org (a WordPress site) from this repo:
# README.md, SUPPORT.md, docs/features/coming-from-qlc4.md, the pages in
# tools/site/pages/, the gallery in tools/site/gallery.txt and the release
# posts in tools/site/news/ (one short post per GitHub release; the release's
# date and links come from GitHub through `gh`).
#
#   python3 tools/site/build.py OUTDIR
#
# Writes OUTDIR/site.json and an .html preview of each page. Pictures stay
# repo paths ({{IMG:path}}); tools/site/load.py uploads them and fills them in.
# tools/site/publish.sh runs both.
#
# Needs the `markdown` package (publish.sh makes a venv for it).

import json, os, posixpath, re, subprocess, sys
import markdown

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = sys.argv[1]
GH = "https://github.com/sandinak/qlcconsole"
BLOB = GH + "/blob/main/"
COURSE = "https://sandinak.github.io/qlcconsole/"

# Repo paths that have a page of their own on the site.
LOCAL = {
    "SUPPORT.md": "/support/",
    "docs/features/coming-from-qlc4.md": "/coming-from-qlc-4/",
    "docs/course/README.md": COURSE,
    "docs/course/web/index.html": COURSE + "web/",
    "docs/course/GLOSSARY.md": COURSE + "GLOSSARY.html",
    "docs/course/rig.md": COURSE + "rig.html",
}
README_ANCHORS = {
    "major-features": "/features/", "download-and-install": "/download/",
    "getting-started": "/getting-started/", "for-teachers": "/teachers/",
    "documentation": "/getting-started/#documentation", "for-developers": "/about/#for-developers",
    "support": "/support/", "built-on-qlc": "/about/",
}
images = []

STYLE = """<style>
.qc-tag{font-size:1.25em;font-style:italic;opacity:.85}
.qc-cta{display:flex;flex-wrap:wrap;gap:.6em;margin:1.2em 0}
.qc-btn{display:inline-block;padding:.55em 1.1em;border:1px solid currentColor;border-radius:6px;text-decoration:none}
.qc-primary{background:#1f6feb;border-color:#1f6feb;color:#fff!important}
.qc-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:1.5em;margin:1.5em 0}
.qc-card{border:1px solid rgba(127,127,127,.3);border-radius:8px;overflow:hidden}
.qc-card img{width:100%;height:170px;object-fit:cover;object-position:top left;display:block}
.qc-card div{padding:.8em 1em}
.qc-card h3{margin:.2em 0 .4em;font-size:1.1em}
.qc-card p{margin:0;font-size:.95em}
.qc-toc{display:grid;grid-template-columns:repeat(auto-fill,minmax(250px,1fr));gap:.8em;margin:1.2em 0 2.5em;padding:0;list-style:none}
.qc-toc li{border:1px solid rgba(127,127,127,.3);border-radius:8px;padding:.7em .9em;margin:0}
.qc-toc a{font-weight:600}
.qc-toc span{display:block;font-size:.9em;opacity:.8;margin-top:.25em}
.qc-feature{scroll-margin-top:2em;margin-top:3em;padding-top:1.5em;border-top:1px solid rgba(127,127,127,.25)}
.qc-top{font-size:.85em}
.qc-example img{max-height:420px;width:auto}
.wp-block-post-content img{max-width:100%;height:auto}
.wp-block-post-content table{border-collapse:collapse}
.wp-block-post-content td,.wp-block-post-content th{border:1px solid rgba(127,127,127,.35);padding:.35em .6em;vertical-align:top}
</style>"""


def read(p):
    with open(os.path.join(REPO, p), encoding="utf-8") as f:
        return f.read()


def front_matter(text):
    m = re.match(r"\A---\n(.*?)\n---\n", text, re.S)
    if not m:
        return {}, text
    meta = {}
    for line in m.group(1).splitlines():
        k, _, v = line.partition(":")
        meta[k.strip()] = v.strip().strip('"')
    return meta, text[m.end():]


def rewrite(html, src_file):
    base = posixpath.dirname(src_file)

    def resolve(target):
        if re.match(r"^(https?:|mailto:|/|\{\{)", target):
            return target
        path, _, anchor = target.partition("#")
        if not path:
            if src_file == "README.md" and anchor in README_ANCHORS:
                return README_ANCHORS[anchor]
            return "#" + anchor
        rp = posixpath.normpath(posixpath.join(base, path))
        if rp in LOCAL:
            url = LOCAL[rp]
            return url + ("#" + anchor if anchor and not url.startswith("http") else "")
        return BLOB + rp + ("#" + anchor if anchor else "")

    def img(m):
        target = m.group(2)
        if re.match(r"^(https?:|\{\{)", target):
            return m.group(0)
        rp = posixpath.normpath(posixpath.join(base, target))
        if rp not in images:
            images.append(rp)
        return m.group(1) + "{{IMG:" + rp + "}}" + m.group(3)

    html = re.sub(r'(<img[^>]*?src=")([^"]+)(")', img, html)
    return re.sub(r'(<a[^>]*?href=")([^"]+)(")', lambda m: m.group(1) + resolve(m.group(2)) + m.group(3), html)


def md(text, src_file):
    h = markdown.markdown(text, extensions=["tables", "fenced_code", "toc", "md_in_html", "sane_lists"])
    return rewrite(h, src_file)


def section(text, heading, level=2):
    """Body of the heading, up to the next heading of the same or a higher level."""
    m = re.search(r"^%s %s\s*$" % ("#" * level, re.escape(heading)), text, re.M)
    if not m:
        raise SystemExit("missing section: " + heading)
    rest = text[m.end():]
    nxt = re.search(r"^#{1,%d} " % level, rest, re.M)
    return rest[: nxt.start()] if nxt else rest


def strip_title(text):
    return re.sub(r"\A\s*(---.*?---\s*)?#\s+[^\n]*\n", "", text, flags=re.S)


def slug(t):
    return re.sub(r"[^a-z0-9]+", "-", t.lower()).strip("-")


def first_sentences(text, n):
    para = next((p.strip() for p in re.split(r"\n\s*\n", text)
                 if p.strip() and not p.strip().startswith(("<img", "→", "-"))), "")
    return " ".join(re.split(r"(?<=[.!?])\s+", para.replace("\n", " "))[:n])


def inline(text):
    return md(text, "README.md").removeprefix("<p>").removesuffix("</p>")


def wrap(html):
    return "<!-- wp:html -->\n" + html + "\n<!-- /wp:html -->"


readme = read("README.md")
features_md = section(readme, "Major features")

# --- the features, each with its title, picture, one-line summary and body
features = []
for m in re.finditer(r"^### (.+)$", features_md, re.M):
    title = m.group(1).strip()
    body = section(features_md, title, 3)
    im = re.search(r'<img src="([^"]+)" alt="([^"]*)"', body)
    features.append({"title": title, "id": slug(title), "img": im.groups() if im else None,
                     "summary": first_sentences(body, 1), "blurb": first_sentences(body, 2), "body": body})

# --- Features page: the list at the top, the details below
toc = ['<ul class="qc-toc" id="top">']
for f in features:
    toc.append(f'<li><a href="#{f["id"]}">{f["title"]}</a><span>{inline(f["summary"])}</span></li>')
toc.append("</ul>")
details = []
for f in features:
    details.append(f'<section class="qc-feature" id="{f["id"]}"><h2>{f["title"]}</h2>'
                   + md(f["body"], "README.md")
                   + '<p class="qc-top"><a href="#top">↑ All features</a></p></section>')
features_page = (STYLE + "<p>Everything QLC+ 4 does, plus a console-style way to work. Pick a feature "
                 "for the details, or see it all in the <a href=\"/gallery/\">gallery</a>.</p>"
                 + rewrite("".join(toc), "README.md") + "".join(details))

# --- Home page
intro = readme.split("**qlcconsole** is a lighting console", 1)[1]
intro = "**qlcconsole** is a lighting console" + intro.split("**Contents:**", 1)[0]
cards = ['<div class="qc-grid">']
for f in features:
    if f["img"]:
        cards.append(f'<div class="qc-card"><a href="/features/#{f["id"]}"><img src="{f["img"][0]}" alt="{f["img"][1]}" /></a>'
                     f'<div><h3><a href="/features/#{f["id"]}">{f["title"]}</a></h3><p>{inline(f["blurb"])}</p></div></div>')
cards.append("</div>")
home = (STYLE + '<p class="qc-tag">Design the rig, write the show as cues, focus it in every room, and run it live.</p>'
        + rewrite('<p><img src="docs/course/images/lesson-1-1/rig-chorus.png" alt="The Rig Overview: a show choir stage '
                  'lit by the show, with risers, trusses, towers and the cyc" /></p>', "README.md")
        + md(intro, "README.md")
        + '<p class="qc-cta"><a class="qc-btn qc-primary" href="/download/">Download qlcconsole</a> '
          '<a class="qc-btn" href="/examples/">Try an example show</a> '
          '<a class="qc-btn" href="/gallery/">See it running</a></p>'
        + "<h2>Major features</h2>" + rewrite("".join(cards), "README.md")
        + '<p><a href="/features/">All features →</a></p>')

# --- Download page, with the latest release's files from GitHub
def gh_json(*args):
    return json.loads(subprocess.check_output(["gh", *args, "-R", "sandinak/qlcconsole"]))

rel = gh_json("release", "view", "--json", "tagName,name,publishedAt,assets,url")
rows = []
for label, suf in [("Mac (Apple silicon or Intel, macOS 12+)", ".dmg"), ("Windows (64-bit)", "-setup.exe"),
                   ("Linux (x86_64)", ".AppImage"), ("Checksums", "SHA256SUMS")]:
    a = next((a for a in rel["assets"] if a["name"].endswith(suf)), None)
    if a:
        rows.append(f'<li><strong>{label}:</strong> <a href="{a["url"]}">{a["name"]}</a></li>')
download = (STYLE + f'<h2>Latest release: {rel["name"]}</h2><p>Published {rel["publishedAt"][:10]}. '
            f'<a href="{rel["url"]}">Release notes on GitHub</a></p><ul>' + "".join(rows) + "</ul>"
            + md(section(readme, "Download and install"), "README.md")
            + f'<p>Older builds and pre-releases: <a href="{GH}/releases">all releases</a>. '
              'Want something to open first? Try an <a href="/examples/">example show</a>.</p>')

# --- Gallery: groups of pictures; load.py turns them into gallery blocks
alts = {}
for root, _, files in os.walk(os.path.join(REPO, "docs")):
    for fn in files:
        if fn.endswith(".md"):
            src = os.path.relpath(os.path.join(root, fn), REPO)
            t = read(src)
            for m in re.finditer(r"!\[([^\]]*)\]\(([^)\s]+)", t):
                alts.setdefault(posixpath.normpath(posixpath.join(posixpath.dirname(src), m.group(2))), m.group(1))
            for m in re.finditer(r'<img src="([^"]+)" alt="([^"]*)"', t):
                alts.setdefault(posixpath.normpath(posixpath.join(posixpath.dirname(src), m.group(1))), m.group(2))
for m in re.finditer(r'<img src="([^"]+)" alt="([^"]*)"', readme):
    alts.setdefault(m.group(1), m.group(2))

gallery = []
for line in read("tools/site/gallery.txt").splitlines():
    line = line.strip()
    if not line or line.startswith("#") and not line.startswith("## "):
        continue
    if line.startswith("## "):
        gallery.append({"title": line[3:], "items": []})
        continue
    path, _, cap = (s.strip() for s in line.partition("|"))
    if not os.path.exists(os.path.join(REPO, path)):
        raise SystemExit("gallery.txt: no such picture: " + path)
    cap = cap or alts.get(path) or ""
    if not cap:
        raise SystemExit("gallery.txt: no caption for " + path + " (add one after a |)")
    if path not in images:
        images.append(path)
    gallery[-1]["items"].append({"path": path, "caption": cap})

# --- pages from tools/site/pages/*.md
extra_pages = []
for fn in sorted(os.listdir(os.path.join(HERE, "pages"))):
    if fn.endswith(".md"):
        meta, body = front_matter(read("tools/site/pages/" + fn))
        html = md(body, "tools/site/pages/" + fn).replace("<p><img", '<p class="qc-example"><img')
        extra_pages.append({"slug": meta["slug"], "title": meta["title"],
                            "menu_order": int(meta.get("menu_order", 50)), "parent": meta.get("parent", ""),
                            "content": wrap(STYLE + html)})

# The top bar is the theme's page list: a page with a "parent" (a slug that
# comes before it below) is a dropdown under that page, which keeps the bar
# to six tabs (Home, Features, Download, Getting started, Support, About).
pages = [
    {"slug": "home", "title": "qlcconsole", "menu_order": 0, "content": wrap(home)},
    {"slug": "features", "title": "Features", "menu_order": 1, "content": wrap(features_page)},
    {"slug": "gallery", "title": "Gallery", "menu_order": 2, "parent": "features", "gallery": gallery,
     "content": wrap(STYLE + "<p>qlcconsole running, every picture taken from the app itself. "
                     "Click a picture to see it full size.</p>")},
    {"slug": "download", "title": "Download", "menu_order": 4, "content": wrap(download)},
    {"slug": "getting-started", "title": "Getting started", "menu_order": 5,
     "content": wrap(STYLE + md(section(readme, "Getting started"), "README.md")
                     + '<h2 id="documentation">Documentation</h2>' + md(section(readme, "Documentation"), "README.md"))},
    {"slug": "coming-from-qlc-4", "title": "Coming from QLC+ 4", "menu_order": 6, "parent": "getting-started",
     "content": wrap(STYLE + md(strip_title(read("docs/features/coming-from-qlc4.md")), "docs/features/coming-from-qlc4.md"))},
    {"slug": "teachers", "title": "For teachers", "menu_order": 7, "parent": "getting-started",
     "content": wrap(STYLE + f'<p class="qc-cta"><a class="qc-btn qc-primary" href="{COURSE}">Open the course site</a></p>'
                     + md(section(readme, "For teachers"), "README.md"))},
    {"slug": "support", "title": "Support", "menu_order": 8, "content": wrap(STYLE + md(strip_title(read("SUPPORT.md")), "SUPPORT.md"))},
    {"slug": "about", "title": "About", "menu_order": 9,
     "content": wrap(STYLE + md(section(readme, "Built on QLC+"), "README.md")
                     + '<h2 id="for-developers">For developers</h2>' + md(section(readme, "For developers"), "README.md")
                     + "<h2>Licence</h2>" + md(section(readme, "License"), "README.md"))},
    {"slug": "news", "title": "News", "menu_order": 10, "parent": "about", "content": ""},
] + extra_pages

# --- News: one short post per release in tools/site/news/
releases = {r["tagName"]: r for r in gh_json("release", "list", "-L", "100", "--json", "tagName,publishedAt,isPrerelease,isDraft")}
posts = []
for fn in sorted(os.listdir(os.path.join(HERE, "news"))):
    if not fn.endswith(".md"):
        continue
    meta, body = front_matter(read("tools/site/news/" + fn))
    r = releases.get(meta["tag"])
    if not r or r["isDraft"]:
        print("skipping", fn, "(no published GitHub release", meta["tag"] + ")")
        continue
    url = f"{GH}/releases/tag/{meta['tag']}"
    html = md(body, "README.md") + (f'<p><a href="{url}">Full release notes and downloads →</a></p>')
    posts.append({"slug": "release-" + meta["tag"].replace(".", "-"), "title": meta["title"],
                  "date_gmt": r["publishedAt"].rstrip("Z").replace("T", " "), "content": wrap(html),
                  "category": "Pre-releases" if r["isPrerelease"] else "Releases"})
missing = sorted(set(releases) - {m for m in (front_matter(read("tools/site/news/" + f))[0].get("tag")
                                              for f in os.listdir(os.path.join(HERE, "news")) if f.endswith(".md"))}
                 - {t for t, r in releases.items() if r["isDraft"]})
if missing:
    print("note: releases with no post in tools/site/news/:", ", ".join(missing))

# The logo: the header's site logo and the browser-tab icon (site icon).
# Rendered from resources/icons/svg/qlcconsole.svg at 512 px (WordPress
# asks for a square of at least 512 for the site icon):
#   rsvg-convert -w 512 -h 512 resources/icons/svg/qlcconsole.svg -o tools/site/assets/qlcconsole-logo.png
LOGO = "tools/site/assets/qlcconsole-logo.png"
if LOGO not in images:
    images.append(LOGO)

missing_imgs = [i for i in images if not os.path.exists(os.path.join(REPO, i))]
if missing_imgs:
    raise SystemExit("missing pictures: " + ", ".join(missing_imgs))

os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "site.json"), "w") as f:
    json.dump({"images": images, "logo": LOGO, "pages": pages, "posts": posts}, f, indent=1)
for p in pages:
    with open(os.path.join(OUT, p["slug"] + ".html"), "w") as f:
        f.write(p["content"])
print(f"{len(pages)} pages, {len(posts)} posts, {len(images)} pictures")
