#!/usr/bin/env python3
#
# Loads what tools/site/build.py made into the qlcconsole.org WordPress with
# WP-CLI. Runs ON THE SERVER, as root (it runs WP-CLI as the web user):
#
#   sudo python3 load.py DIR        # DIR holds site.json and img/<repo paths>
#
# Every step can be re-run: pages and posts are matched by slug and updated,
# pictures by file name. WP_CLI (default ~branson/bin/wp-cli.phar) is copied
# to a temporary file the web user can read, for the run only.

import json, os, re, shutil, subprocess, sys, tempfile
from html import escape

D = sys.argv[1]
SITE = os.environ.get("WP_PATH", "/var/www/sites/qlcconsole.org")
WEBUSER = os.environ.get("WP_USER", "apache")
phar = tempfile.NamedTemporaryFile(suffix=".phar", delete=False)
shutil.copyfile(os.path.expanduser(os.environ.get("WP_CLI", "~branson/bin/wp-cli.phar")), phar.name)
os.chmod(phar.name, 0o755)
WP = ["sudo", "-u", WEBUSER, "php", phar.name, "--path=" + SITE, "--skip-plugins", "--skip-themes"]


def wp(*args):
    # cwd: the web user must be able to enter it, or WP-CLI's own
    # subprocesses (wp rewrite) fail to start.
    r = subprocess.run(WP + list(args), capture_output=True, text=True, cwd=SITE)
    if r.returncode:
        raise SystemExit(f"wp {' '.join(args)} failed:\n{r.stderr}")
    return r.stdout.strip()


try:
    site = json.load(open(os.path.join(D, "site.json")))

    wp("option", "update", "timezone_string", "America/New_York")
    wp("option", "update", "blogdescription", "A lighting console for show choir, school theatre and touring shows")
    wp("rewrite", "structure", "/%postname%/", "--hard")

    for name, ptype in (("hello-world", "post"), ("sample-page", "page")):
        for i in wp("post", "list", f"--post_type={ptype}", f"--name={name}", "--field=ID", "--post_status=any").split():
            wp("post", "delete", i, "--force")

    # --- pictures
    att = {}
    for rp in site["images"]:
        name = os.path.splitext(os.path.basename(rp))[0]
        found = wp("post", "list", "--post_type=attachment", f"--name={name}", "--field=ID").split()
        if found:
            aid = found[0]
        else:
            tmp = os.path.join(tempfile.mkdtemp(), os.path.basename(rp))
            shutil.copyfile(os.path.join(D, "img", rp), tmp)
            os.chmod(os.path.dirname(tmp), 0o755)
            os.chmod(tmp, 0o644)
            aid = wp("media", "import", tmp, "--porcelain", f"--title={name}")
            shutil.rmtree(os.path.dirname(tmp))
        urls = json.loads(wp("eval", f"echo json_encode([wp_get_attachment_url({int(aid)}), "
                                     f"wp_get_attachment_image_url({int(aid)}, 'large')]);"))
        att[rp] = {"id": int(aid), "url": urls[0], "large": urls[1] or urls[0]}
    print(len(att), "pictures")

    def fill(html):
        return re.sub(r"\{\{IMG:([^}]+)\}\}", lambda m: att[m.group(1)]["url"], html)

    def gallery_blocks(groups):
        out = []
        for g in groups:
            out.append(f'<!-- wp:heading -->\n<h2 class="wp-block-heading">{escape(g["title"])}</h2>\n<!-- /wp:heading -->')
            out.append('<!-- wp:gallery {"columns":2,"linkTo":"none","sizeSlug":"large"} -->\n'
                       '<figure class="wp-block-gallery has-nested-images columns-2 is-cropped">')
            for it in g["items"]:
                a = att[it["path"]]
                cap = escape(it["caption"])
                out.append(f'<!-- wp:image {{"lightbox":{{"enabled":true}},"id":{a["id"]},"sizeSlug":"large","linkDestination":"none"}} -->\n'
                           f'<figure class="wp-block-image size-large"><img src="{a["large"]}" alt="{cap}" class="wp-image-{a["id"]}"/>'
                           f'<figcaption class="wp-element-caption">{cap}</figcaption></figure>\n<!-- /wp:image -->')
            out.append("</figure>\n<!-- /wp:gallery -->")
        return "\n\n".join(out)

    def upsert(ptype, slug, title, content, extra):
        with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False) as f:
            f.write(content)
        os.chmod(f.name, 0o644)
        ids = wp("post", "list", f"--post_type={ptype}", f"--name={slug}", "--field=ID", "--post_status=any").split()
        common = [f"--post_title={title}", "--post_status=publish", "--comment_status=closed", "--ping_status=closed"] + extra
        if ids:
            wp("post", "update", ids[0], f.name, *common)
        else:
            ids = [wp("post", "create", f.name, f"--post_type={ptype}", f"--post_name={slug}", "--porcelain", *common)]
        os.remove(f.name)
        return ids[0]

    page_ids = {}
    # Parents first, so a child's parent has an ID; no parent puts it back on
    # the top bar.
    for p in sorted(site["pages"], key=lambda p: bool(p.get("parent"))):
        content = fill(p["content"])
        if p.get("gallery"):
            content += "\n\n" + gallery_blocks(p["gallery"])
        parent = page_ids[p["parent"]] if p.get("parent") else 0
        page_ids[p["slug"]] = upsert("page", p["slug"], p["title"], content,
                                     [f"--menu_order={p['menu_order']}", f"--post_parent={parent}"])
        print("page", p["slug"])

    cats = {}
    for p in site["posts"]:
        c = p["category"]
        if c not in cats:
            found = wp("term", "list", "category", f"--name={c}", "--field=term_id").split()
            cats[c] = found[0] if found else wp("term", "create", "category", c, "--porcelain")
        upsert("post", p["slug"], p["title"], fill(p["content"]),
               [f"--post_date_gmt={p['date_gmt']}", f"--post_category={cats[c]}"])
        print("post", p["slug"])

    # --- the logo: header (site logo beside the title) and browser-tab icon.
    # The header part overrides the theme's (twentytwentyfive's header
    # pattern with a site-logo block in front of the title); it is ours, so
    # it is rewritten on every publish.
    if site.get("logo"):
        logo = att[site["logo"]]["id"]
        wp("option", "update", "site_logo", str(logo))
        wp("option", "update", "site_icon", str(logo))
        header = (
            '<!-- wp:group {"align":"full","layout":{"type":"default"}} -->\n'
            '<div class="wp-block-group alignfull">'
            '<!-- wp:group {"layout":{"type":"constrained"}} -->\n<div class="wp-block-group">'
            '<!-- wp:group {"align":"wide","style":{"spacing":{"padding":{"top":"var:preset|spacing|30","bottom":"var:preset|spacing|30"}}},"layout":{"type":"flex","flexWrap":"nowrap","justifyContent":"space-between"}} -->\n'
            '<div class="wp-block-group alignwide" style="padding-top:var(--wp--preset--spacing--30);padding-bottom:var(--wp--preset--spacing--30)">'
            '<!-- wp:group {"style":{"spacing":{"blockGap":"var:preset|spacing|20"}},"layout":{"type":"flex","flexWrap":"nowrap","verticalAlignment":"center"}} -->\n<div class="wp-block-group">'
            '<!-- wp:site-logo {"width":40,"shouldSyncIcon":true} /-->\n'
            '<!-- wp:site-title {"level":0} /-->'
            '</div>\n<!-- /wp:group -->\n'
            '<!-- wp:group {"style":{"spacing":{"blockGap":"var:preset|spacing|10"}},"layout":{"type":"flex","flexWrap":"nowrap","justifyContent":"right"}} -->\n<div class="wp-block-group">'
            '<!-- wp:navigation {"overlayBackgroundColor":"base","overlayTextColor":"contrast","layout":{"type":"flex","justifyContent":"right","flexWrap":"wrap"}} /-->'
            '</div>\n<!-- /wp:group -->'
            '</div>\n<!-- /wp:group -->'
            '</div>\n<!-- /wp:group -->'
            '</div>\n<!-- /wp:group -->')
        php = ("$c = " + json.dumps(header) + ";"
               "$q = get_posts(['post_type'=>'wp_template_part','name'=>'header','post_status'=>'any','numberposts'=>1,"
               "'tax_query'=>[['taxonomy'=>'wp_theme','field'=>'name','terms'=>get_stylesheet()]]]);"
               "$a = ['post_type'=>'wp_template_part','post_name'=>'header','post_title'=>'Header',"
               "'post_status'=>'publish','post_content'=>$c,"
               "'tax_input'=>['wp_theme'=>[get_stylesheet()],'wp_template_part_area'=>['header']]];"
               "if ($q) { $a['ID'] = $q[0]->ID; }"
               "$id = wp_insert_post(wp_slash($a), true);"
               "if (is_wp_error($id)) { fwrite(STDERR, $id->get_error_message()); exit(1); }"
               "wp_set_object_terms($id, get_stylesheet(), 'wp_theme');"
               "wp_set_object_terms($id, 'header', 'wp_template_part_area');"
               "echo $id;")
        print("logo", logo, "header part", wp("eval", php))

    wp("option", "update", "show_on_front", "page")
    wp("option", "update", "page_on_front", page_ids["home"])
    wp("option", "update", "page_for_posts", page_ids["news"])
    wp("option", "update", "default_comment_status", "closed")
    wp("cache", "flush")
    print("DONE")
finally:
    os.remove(phar.name)
