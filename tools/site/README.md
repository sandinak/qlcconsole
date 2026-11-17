# qlcconsole.org

The project site is a WordPress site on the web server (sandsite.org's EC2
host, DNS in Route 53). Its content is built from this repo, so it stays in
step with the README and the docs:

| Site page | Comes from |
|---|---|
| Home, Features, Download, Getting started, About | `README.md` (Features: the "Major features" section, a contents list at the top) |
| Coming from QLC+ 4 | `docs/features/coming-from-qlc4.md` |
| Support | `SUPPORT.md` |
| Examples (and any other page) | `tools/site/pages/*.md` (front matter: slug, title, menu_order) |
| Gallery | `tools/site/gallery.txt` (groups of pictures; captions are the docs' alt text) |
| News | `tools/site/news/<tag>.md`, one short post per GitHub release, dated from GitHub |

The top bar keeps six tabs; the rest are dropdowns under them (a page's
`parent` in build.py, or `parent:` in a page's front matter).

Links to other docs go to the file on GitHub; pictures are uploaded to the
site's media library.

## Publishing

```sh
tools/site/publish.sh            # build and load
tools/site/publish.sh --build    # build only, to look at the HTML first
```

It needs `gh` (signed in), ssh with sudo on the server (`SITE_HOST`, default
`sandsite.org`) and WP-CLI there at `~/bin/wp-cli.phar`. Run it again
whenever the README or a page changes; it updates pages and posts in place.

## After a release

Add `tools/site/news/vX.Y.Z.md`: a few sentences and three to five
highlights, written for someone deciding whether to update, not the
changelog. The full notes are one link away. Then publish.
