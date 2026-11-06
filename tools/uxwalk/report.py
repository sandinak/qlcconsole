#!/usr/bin/env python3
"""The UX walker's checks and report (docs/dev/UX_WALKER.md).

Reads the records the walker wrote (OUT/screens/*.json, one per screen
state: a window's visible widgets, an open menu's items, or a walker note),
runs the checks over them -- the rules of docs/dev/ux_rules.json that a
recorded screen can be checked against, and a few of its own -- and writes

    OUT/findings.json   every finding, grouped: one entry per distinct
                        problem, with every screen it was seen on
    OUT/report.html     one page: findings by rule with thumbnails, then
                        every screen

Report-only: always exits 0. Its last line is the summary,
    uxwalk: N screens, M findings (K rules)

    tools/uxwalk/report.py OUT [--rules docs/dev/ux_rules.json]

Adding a check: write a function `check_xxx(rec, ctx)` that yields
Finding(...) and add it to CHECKS (see the bottom of the checks section).
"""

import argparse
import collections
import glob
import html
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

# ---------------------------------------------------------------------------
# Built-in rules (the rule book's ids where one fits, W-* where it has none)
# ---------------------------------------------------------------------------

BUILTIN = {
    "W-DUP": ("Duplicated menu words",
              "The same item twice in one menu, or an item that repeats its menu's own name."),
    "W-MOJIBAKE": ("Garbled characters",
                   "Text decoded with the wrong encoding (\"â–¾\" for \"▾\", \"Ã©\" for \"é\")."),
    "W-AMPERSAND": ("\"&\" eaten as a shortcut mark",
                    "A label, button or group title with \"& \" in it: Qt takes the & as a "
                    "keyboard mark and shows an underlined space (\"Position _Orientation\"); "
                    "it needs \"&&\"."),
    "W-NOMENU": ("Right-click opens nothing",
                 "A representative item whose right-click opened no menu."),
    "W-CRASH": ("The walk crashed",
                "The app crashed on this screen; the walk went on past it."),
    "W-CHANGED": ("A walk step changed the show",
                  "A step that should only show or ask (an item ending in \"…\" then Cancel, "
                  "a command line that was not a command) left the show modified."),
}

# Rules of the book the walker checks with code of its own (the others with
# a forbid_regex for ui_text are applied to every text as they are).
CODED = {"UX-01", "UX-07", "UX-08", "UX-09", "UX-20", "UX-21", "UX-23", "UX-24",
         "UX-27", "UX-34", "UX-36", "UX-38"}


class Finding:
    def __init__(self, rule, text, detail="", where="", rect=None, sig=None):
        self.rule = rule
        self.text = text            # the text at fault (or a short description)
        self.detail = detail        # why
        self.where = where          # widget path / menu item
        self.rect = rect            # [x, y, w, h] on the screen's PNG
        # What makes two findings the same problem on different screens.
        self.sig = sig if sig is not None else "%s|%s|%s" % (rule, where, text)


# ---------------------------------------------------------------------------
# Reading the records
# ---------------------------------------------------------------------------

def load_records(out):
    recs = []
    for f in sorted(glob.glob(os.path.join(out, "screens", "*.json"))):
        try:
            with open(f, encoding="utf-8") as fh:
                recs.append(json.load(fh))
        except (OSError, ValueError) as e:
            print("uxwalk: unreadable record %s: %s" % (f, e), file=sys.stderr)
    return recs


def load_rules(path):
    if not path or not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    rules = data["rules"] if isinstance(data, dict) else data
    return {r["id"]: r for r in rules}


# Where a text came from: "chrome" is the app's own words (labels, buttons,
# menus, tooltips, titles, messages); "data" is what the show holds (list
# items, combo entries, a line edit's text), which the wording rules leave
# alone and the units rule still reads.
def texts(rec):
    """Yield (kind, where, text, rect) for every text of a record."""
    if rec.get("kind") == "menu":
        menu = rec.get("name", "")
        for it in rec.get("items", []):
            if it.get("separator"):
                continue
            where = "%s ▸ %s" % (menu, it.get("text", ""))
            yield "chrome", where, it.get("text", ""), it.get("rect")
            for k in ("tooltip", "statusTip"):
                if it.get(k):
                    yield "chrome", where + " (%s)" % k, it[k], it.get("rect")
        return
    if rec.get("windowTitle"):
        yield "chrome", "window title", rec["windowTitle"], None
    for k in ("message",):
        if rec.get(k):
            yield "chrome", k, rec[k], None
    for w in rec.get("widgets", []):
        path = w.get("path", w.get("class", ""))
        rect = w.get("rect")
        cls = w.get("class", "")
        datafield = cls in ("QLineEdit", "QTextEdit", "QPlainTextEdit", "QTextBrowser") \
            or "LineEdit" in cls or "Edit" in cls and "Spin" not in cls
        if w.get("text"):
            yield ("data" if datafield else "chrome"), path, w["text"], rect
        for k in ("tooltip", "placeholder", "statusTip", "whatsThis"):
            if w.get(k):
                yield "chrome", path + " (%s)" % k, w[k], rect
        for h in w.get("headers", []):
            if h:
                yield "chrome", path + " (header)", h, rect
        for it in w.get("items", []):
            if isinstance(it, str):
                yield "data", path + " (item)", it, rect
            else:
                kind = "chrome" if cls == "QTabBar" else "data"
                if it.get("text"):
                    yield kind, path + " (item)", it["text"], it.get("rect", rect)
                if it.get("tooltip"):
                    yield "data", path + " (item tooltip)", it["tooltip"], it.get("rect", rect)


# ---------------------------------------------------------------------------
# The checks
# ---------------------------------------------------------------------------

# units_test.cpp's patterns: a metric length, an imperial one.
METRIC = re.compile(r"\d\s?(?:mm|cm|m)\b|\b[Mm]et(?:res?|ers)\b")
# Tuned from units_test's: a foot mark only after a number that is not a
# possessive ("QLC+ 4's"), an inch mark only after feet or a fraction (a
# quoted name ends in a digit and a quote: "Spot 3", "172.18.2.219").
IMPERIAL = re.compile(r"\d\s?(?:ft|in)\b(?!\s+(?:the|a|an|this|that|its|his|her)\b)"
                      r"|\d['′](?![A-Za-z])|\d+' ?\d+(?: \d+/\d+)?[\"″]|\d/\d+[\"″]"
                      r"|\b(?:[Ff]eet|[Ff]oot|[Ii]nch(?:es)?)\b")
# A length field's help lists every unit it takes, on purpose.
LENGTH_HELP = re.compile(r"or a length with its unit, converted for you")
# Not a length: a time ("5 m" is rare; "500 ms" isn't metres), a size of a
# file, a version, a date.
NOT_LENGTH = re.compile(r"\d\s?ms\b|\d\s?(?:MB|KB|GB)\b|\bm/s\b")


def check_units(rec, ctx):
    units = rec.get("units")
    for kind, where, text, rect in texts(rec):
        if LENGTH_HELP.search(text):
            continue
        t = NOT_LENGTH.sub("", text)
        # A text that shows both kinds is about the units themselves (the
        # Units setting's help, a field's list of what it takes).
        if METRIC.search(t) and IMPERIAL.search(t):
            continue
        if units == "feet":
            m = METRIC.search(t) or (ctx.ux01 and ctx.ux01.search(t))
        else:
            m = IMPERIAL.search(t)
        if m:
            # A name the show gave it (a fixture called "2m bar") is the
            # show's, not the app's: a data text only counts with a number
            # the app formatted ("3.5 m", "12' 6\"").
            if kind == "data" and not re.search(r"\d+(?:\.\d+)?\s?(?:mm|cm|m)\b|\d+' ?\d", t):
                continue
            yield Finding("UX-01", text, "%s shown while Settings ▸ Units is %s"
                          % ("metric length" if units == "feet" else "imperial length",
                             "Feet" if units == "feet" else "Millimetres"),
                          where, rect, sig="UX-01|%s|%s" % (where, m.group(0)))


# Submenus that list the show's own things (its groups, layers, folders,
# recent files): their items are names the person gave, not the app's words.
DATA_MENUS = re.compile(r"Open Recent|New from Template|Recent|Add to Group|Move to Layer|Move to Folder"
                        r"|▸ Selection$|Used By|Add to Circuit|Put Back")
SKIP_TITLE_MENUS = DATA_MENUS


def check_title_case(rec, ctx):
    if rec.get("kind") == "menu":
        if SKIP_TITLE_MENUS.search(rec.get("name", "")):
            return
        for it in rec.get("items", []):
            if it.get("titleCase"):
                yield Finding("UX-07", it["text"], it["titleCase"],
                              "%s ▸ %s" % (rec.get("name"), it["text"]), it.get("rect"),
                              sig="UX-07|%s" % it["text"])
        return
    for w in rec.get("widgets", []):
        if w.get("titleCase"):
            yield Finding("UX-08", w.get("text", ""), w["titleCase"], w.get("path", ""), w.get("rect"),
                          sig="UX-08|%s" % w.get("text", ""))


def check_ellipsis(rec, ctx):
    if rec.get("kind") == "note" and rec.get("rule") == "walker-no-window":
        yield Finding("UX-09", rec.get("name", ""), "ends in \"…\" but opened no window",
                      rec.get("name", ""), sig="UX-09|nowindow|%s" % rec.get("name"))
    for kind, where, text, rect in texts(rec):
        if kind == "chrome" and "..." in text and "(tooltip)" not in where:
            yield Finding("UX-09", text, "\"...\" instead of \"…\"", where, rect,
                          sig="UX-09|...|%s" % text)


def check_picker(rec, ctx):
    if rec.get("group") != "pickers" or "offered" not in rec:
        return
    if rec.get("targetWled"):
        return
    wled = [o for o in rec["offered"] if o.get("wled")]
    if wled:
        names = sorted({o["name"] for o in wled})
        yield Finding("UX-20", ", ".join(names[:8]) + (" …" if len(names) > 8 else ""),
                      "%d WLED effects offered for the %s group \"%s\", which has no WLED device"
                      % (len(names), rec.get("target"), rec.get("targetGroup")),
                      "effect picker", sig="UX-20|wled|%s" % rec.get("target"))
    unfit = [o for o in rec["offered"] if not o.get("fits", True)]
    if unfit:
        names = sorted({o["name"] for o in unfit})
        yield Finding("UX-20", ", ".join(names[:8]),
                      "%d effects listed that don't fit the %s group (Show All Effects is off)"
                      % (len(names), rec.get("target")),
                      "effect picker", sig="UX-20|unfit|%s" % rec.get("target"))


def check_plain_words(rec, ctx):
    # UX-21: a line in plain words is not answered as a typo of a command.
    if rec.get("group") != "cmdline" or "line" not in rec:
        return
    msg = rec.get("message", "")
    if "plain words" in rec.get("name", "") and re.search(r"Did you mean", msg):
        yield Finding("UX-21", msg, "a line in plain words was read as a typo of a command",
                      "Build command line", sig="UX-21|typo-for-words")
    if len(msg) > 220:
        yield Finding("UX-21", msg[:160] + "…", "the reply is %d characters on one line under the bar"
                      % len(msg), "Build command line", sig="UX-21|long|%s" % rec.get("name"))


def check_empty(rec, ctx):
    for w in rec.get("widgets", []):
        if not w.get("empty") or not w.get("enabled", True):
            continue
        r = w.get("rect") or [0, 0, 0, 0]
        if w.get("class") != "QComboBox" and (r[2] < 120 or r[3] < 60):
            continue
        if w.get("emptyNote") or w.get("placeholder") or w.get("tooltip"):
            continue
        yield Finding("UX-23", w.get("class", ""), "an empty %s with nothing saying why or what to do"
                      % ("list" if w.get("class") != "QComboBox" else "drop-down"),
                      w.get("path", ""), r)


def check_help_topic(rec, ctx):
    if rec.get("kind") == "window" and rec.get("isDialog") and not rec.get("helpTopic") \
            and rec.get("class") not in ("QMessageBox", "QFileDialog", "QInputDialog",
                                         "QColorDialog", "QFontDialog", "QProgressDialog"):
        yield Finding("UX-24", rec.get("windowTitle") or rec.get("class", ""),
                      "a dialog with no helpTopic: F1 opens no page for it",
                      rec.get("class", ""), sig="UX-24|%s" % rec.get("class"))


# Self-explaining when greyed: there is nothing to undo, copy, paste.
SELF_EXPLAINED = re.compile(r"^(Undo|Redo|Cut|Copy|Paste|Select All|Delete)\b")


def check_disabled_says_why(rec, ctx):
    if rec.get("kind") == "menu":
        if DATA_MENUS.search(rec.get("name", "")):
            return
        for it in rec.get("items", []):
            if it.get("separator") or it.get("enabled", True) or it.get("submenu"):
                continue
            # A line of information, greyed so it can't be picked.
            if it.get("text", "").startswith("("):
                continue
            t = it.get("text", "")
            if SELF_EXPLAINED.match(t) or it.get("tooltip") or it.get("statusTip"):
                continue
            yield Finding("UX-27", t, "a greyed menu item with no tooltip saying why",
                          "%s ▸ %s" % (rec.get("name"), t), it.get("rect"), sig="UX-27|menu|%s" % t)
        return
    if rec.get("class") == "QFileDialog":
        return                          # Qt's own
    for w in rec.get("widgets", []):
        if w.get("enabled", True) or "Button" not in w.get("class", ""):
            continue
        # A tab strip's scroll arrows.
        if not w.get("text") and re.search(r"(QTabBar|QTabWidget[^/]*)/QToolButton$|ScrollLeftButton|ScrollRightButton",
                                           w.get("path", "")):
            continue
        # Greyed because its whole panel is: the panel says why, not each button.
        if w.get("tooltip") or w.get("statusTip"):
            continue
        t = w.get("text", "") or w.get("path", "")
        if SELF_EXPLAINED.match(w.get("text", "")):
            continue
        yield Finding("UX-27", t, "a greyed button with no tooltip saying why",
                      w.get("path", ""), w.get("rect"), sig="UX-27|button|%s|%s" % (w.get("path"), t))


LAYOUT_KIND = re.compile(r"^(overlap|clipped|off-window|zero-size|text-cut)")


def check_layout(rec, ctx):
    for f in rec.get("layout", []):
        t = f.get("text", "")
        m = LAYOUT_KIND.match(t)
        kind = m.group(1) if m else "layout"
        if kind == "text-cut":
            continue                    # check_elided reads every widget's text
        rule = "UX-36" if kind == "text-cut" else "UX-34"
        # The finding without its numbers, so one problem at two sizes is one.
        sig = re.sub(r"\(\d+,\d+ \d+x\d+\)|\d+", "#", t)
        yield Finding(rule, t, kind, "", f.get("rect"), sig="%s|%s" % (rule, sig))


# The Virtual Console is the person's drawing: its sizes are theirs.
PLACED = re.compile(r"\bVC[A-Z]\w*#")


def check_elided(rec, ctx):
    # A row cut short in a narrow window is a tree doing its job; at the
    # laptop size and up it is a column too narrow for its words.
    wide = (rec.get("size") or [0])[0] >= 1280
    for w in rec.get("widgets", []):
        if PLACED.search(w.get("path", "")):
            continue
        if w.get("elided") and w.get("text"):
            yield Finding("UX-36", w["text"], "text wider than its %s (%s px, needs %s)"
                          % (w.get("class"), (w.get("rect") or [0, 0, "?"])[2], w.get("need", "?")),
                          w.get("path", ""), w.get("rect"),
                          sig="UX-36|%s|%s" % (w.get("path"), w["text"]))
        for it in w.get("items", []):
            if wide and isinstance(it, dict) and it.get("elided") and it.get("text"):
                yield Finding("UX-36", it["text"], "cut short in its %s row" % w.get("class"),
                              w.get("path", ""), it.get("rect", w.get("rect")),
                              sig="UX-36|item|%s|%s" % (w.get("path"), it["text"]))


def check_parent(rec, ctx):
    if rec.get("group") == "actions" and rec.get("kind") == "window" \
            and not rec.get("parentIsMain") and rec.get("class") != "QFileDialog":
        yield Finding("UX-38", rec.get("windowTitle") or rec.get("class", ""),
                      "a window opened from a menu that is not a child of the main window "
                      "(it can fall behind it)", rec.get("class", ""), sig="UX-38|%s" % rec.get("class"))


def check_dup_words(rec, ctx):
    if rec.get("kind") != "menu":
        return
    seen = collections.Counter(it.get("text", "").rstrip("…").strip()
                               for it in rec.get("items", []) if not it.get("separator"))
    for t, n in seen.items():
        if n > 1 and t:
            yield Finding("W-DUP", t, "%d items called \"%s\" in one menu" % (n, t),
                          rec.get("name", ""), sig="W-DUP|%s|%s" % (rec.get("name"), t))


MOJIBAKE = re.compile("[ÂÃâ][\u0080-¿]|â\u0080|â\u0096")


def check_mojibake(rec, ctx):
    for kind, where, text, rect in texts(rec):
        if MOJIBAKE.search(text):
            yield Finding("W-MOJIBAKE", text, "UTF-8 read as Latin-1", where, rect,
                          sig="W-MOJIBAKE|%s|%s" % (where, text))


def check_ampersand(rec, ctx):
    for w in rec.get("widgets", []):
        if w.get("mnemonicSpace"):
            yield Finding("W-AMPERSAND", w.get("text", ""), "shows an underlined space where the & is",
                          w.get("path", ""), w.get("rect"), sig="W-AMPERSAND|%s" % w.get("text", ""))


def check_notes(rec, ctx):
    if rec.get("kind") != "note":
        return
    r = rec.get("rule")
    if r == "walker-no-menu":
        yield Finding("W-NOMENU", rec.get("name", ""), rec.get("text", ""), rec.get("name", ""))
    elif r == "walker-crash":
        yield Finding("W-CRASH", rec.get("name", ""), rec.get("text", ""), rec.get("name", ""))
    elif r == "walker-modified":
        yield Finding("W-CHANGED", rec.get("name", ""), rec.get("text", ""), rec.get("name", ""))


TOOLTIP_OK = {"UX-10"}


def check_book_patterns(rec, ctx):
    """Every rule of the book with a forbid_regex over the UI's text, as it
    is written there (UX-01's is part of check_units)."""
    for rid, rx in ctx.patterns:
        for kind, where, text, rect in texts(rec):
            if kind != "chrome" or LENGTH_HELP.search(text):
                continue
            # A tooltip may name the shortcut; the label may not (UX-10).
            if rid in TOOLTIP_OK and re.search(r"\((tooltip|statusTip|whatsThis)\)$", where):
                continue
            m = rx.search(text)
            if m:
                yield Finding(rid, text, "matches the rule's pattern: \"%s\"" % m.group(0), where, rect,
                              sig="%s|%s|%s" % (rid, where, m.group(0)))


CHECKS = [check_units, check_title_case, check_ellipsis, check_picker, check_plain_words,
          check_empty, check_help_topic, check_disabled_says_why, check_layout, check_elided,
          check_parent, check_dup_words, check_mojibake, check_ampersand, check_notes,
          check_book_patterns]


class Context:
    def __init__(self, rules):
        self.rules = rules
        self.patterns = []
        self.ux01 = None
        for rid, r in rules.items():
            p = r.get("patterns") or {}
            rx = p.get("forbid_regex")
            if not rx or p.get("scope") not in ("ui_text", "docs_and_ui_text"):
                continue
            try:
                c = re.compile(rx)
            except re.error as e:
                print("uxwalk: rule %s: bad pattern: %s" % (rid, e), file=sys.stderr)
                continue
            if rid == "UX-01":
                self.ux01 = c           # feet only: check_units
            elif rid == "UX-09":
                continue                # check_ellipsis (chrome only, not tooltips)
            else:
                self.patterns.append((rid, c))


def rule_title(ctx, rid):
    if rid in ctx.rules:
        return ctx.rules[rid].get("title", rid)
    return BUILTIN.get(rid, (rid, ""))[0]


def rule_text(ctx, rid):
    if rid in ctx.rules:
        return ctx.rules[rid].get("rule", "")
    return BUILTIN.get(rid, ("", ""))[1]


# ---------------------------------------------------------------------------
# The report
# ---------------------------------------------------------------------------

def run(out, rules_path):
    recs = load_records(out)
    rules = load_rules(rules_path)
    ctx = Context(rules)
    groups = collections.OrderedDict()      # sig -> {finding, occurrences}
    for rec in recs:
        for check in CHECKS:
            for f in check(rec, ctx):
                g = groups.get(f.sig)
                if g is None:
                    g = groups[f.sig] = {"f": f, "occ": []}
                g["occ"].append({"id": rec.get("id"), "png": rec.get("png"), "show": rec.get("show"),
                                 "units": rec.get("units"), "screen": rec.get("name"),
                                 "group": rec.get("group"), "size": rec.get("size"), "rect": f.rect})
    screens = [r for r in recs if r.get("kind") != "note"]
    by_rule = collections.OrderedDict()
    for sig, g in groups.items():
        by_rule.setdefault(g["f"].rule, []).append(g)
    order = sorted(by_rule, key=lambda r: (not r.startswith("UX"), r))

    findings = []
    for rid in order:
        for g in by_rule[rid]:
            f = g["f"]
            findings.append({"rule": rid, "title": rule_title(ctx, rid), "text": f.text,
                             "detail": f.detail, "where": f.where, "signature": f.sig,
                             "count": len(g["occ"]), "occurrences": g["occ"]})
    with open(os.path.join(out, "findings.json"), "w", encoding="utf-8") as fh:
        json.dump({"screens": len(screens), "findings": findings,
                   "rulesChecked": sorted(CODED | {r for r, _ in ctx.patterns} | set(BUILTIN)),
                   "rulesNotChecked": sorted(r for r, x in rules.items()
                                             if "walker" in json.dumps(x.get("checks", [])) + x.get("check", "")
                                             and r not in CODED and r not in {p for p, _ in ctx.patterns})},
                  fh, indent=1, ensure_ascii=False)
    write_html(out, ctx, recs, screens, by_rule, order)
    notes = [r for r in recs if r.get("kind") == "note" and r.get("rule") == "walker-missing"]
    print("uxwalk: %d screens, %d findings (%d rules)%s"
          % (len(screens), len(findings), len(order),
             ", %d steps not in these shows" % len(notes) if notes else ""))


def thumb(occ, maxw=360):
    if not occ.get("png"):
        return ""
    size = occ.get("size") or [1280, 800]
    w, h = max(1, size[0]), max(1, size[1])
    box = ""
    r = occ.get("rect")
    if r and len(r) == 4 and r[2] > 0 and r[3] > 0:
        box = ('<div class="box" style="left:%.2f%%;top:%.2f%%;width:%.2f%%;height:%.2f%%"></div>'
               % (100.0 * r[0] / w, 100.0 * r[1] / h, 100.0 * r[2] / w, 100.0 * r[3] / h))
    return ('<a class="thumb" href="screens/%s" style="max-width:%dpx"><img loading="lazy" src="screens/%s">%s</a>'
            % (html.escape(occ["png"]), maxw, html.escape(occ["png"]), box))


def write_html(out, ctx, recs, screens, by_rule, order):
    e = html.escape
    parts = ["""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UX Walk</title><style>
:root{--bg:#fff;--fg:#1d1d1f;--mut:#6e6e73;--line:#d2d2d7;--card:#f5f5f7;--hit:#d70015}
@media (prefers-color-scheme:dark){:root{--bg:#1d1d1f;--fg:#f5f5f7;--mut:#a1a1a6;--line:#424245;--card:#2c2c2e;--hit:#ff453a}}
body{background:var(--bg);color:var(--fg);font:14px/1.45 -apple-system,system-ui,sans-serif;margin:0 auto;max-width:1400px;padding:16px}
h1{font-size:22px}h2{font-size:18px;margin-top:32px;border-bottom:1px solid var(--line);padding-bottom:4px}
.mut{color:var(--mut)}.f{background:var(--card);border-radius:8px;padding:10px 12px;margin:10px 0;display:flex;gap:14px;flex-wrap:wrap}
.f .t{flex:1 1 380px;min-width:0;overflow-wrap:anywhere}.f code{font-size:12px}
.thumb{position:relative;display:inline-block;width:100%;border:1px solid var(--line)}.thumb img{width:100%;display:block}
.box{position:absolute;border:2px solid var(--hit);box-sizing:border-box}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:10px}
.grid figure{margin:0}.grid figcaption{font-size:12px;color:var(--mut);overflow-wrap:anywhere}
nav a{margin-right:10px}details summary{cursor:pointer;color:var(--mut)}
</style></head><body>"""]
    shows = sorted({(r.get("show"), r.get("units")) for r in screens})
    nfind = sum(len(v) for v in by_rule.values())
    parts.append("<h1>UX Walk</h1><p>%d screens of %s &middot; %d findings under %d rules</p>"
                 % (len(screens), ", ".join("%s (%s)" % (e(s or ""), e(u or "")) for s, u in shows),
                    nfind, len(order)))
    parts.append("<nav>" + "".join('<a href="#%s">%s (%d)</a>' % (e(r), e(r), len(by_rule[r])) for r in order)
                 + '<a href="#screens">All screens</a></nav>')
    for rid in order:
        parts.append('<h2 id="%s">%s &mdash; %s</h2><p class="mut">%s</p>'
                     % (e(rid), e(rid), e(rule_title(ctx, rid)), e(rule_text(ctx, rid))))
        for g in sorted(by_rule[rid], key=lambda g: -len(g["occ"])):
            f = g["f"]
            occ = g["occ"]
            first = next((o for o in occ if o.get("png")), occ[0])
            where = sorted({"%s · %s · %s" % (o.get("show"), o.get("units"), o.get("screen")) for o in occ})
            parts.append('<div class="f"><div class="t"><b>%s</b><br>%s<br><code class="mut">%s</code>'
                         '<details><summary>%d screen%s</summary>%s</details></div><div style="flex:0 1 360px">%s</div></div>'
                         % (e(f.text[:400]), e(f.detail), e(f.where[:300]), len(occ), "" if len(occ) == 1 else "s",
                            "<br>".join(e(w) for w in where[:60]), thumb(first)))
    parts.append('<h2 id="screens">All screens</h2><div class="grid">')
    for r in sorted(screens, key=lambda r: (r.get("tag", ""), r.get("group", ""), r.get("name", ""))):
        parts.append("<figure>%s<figcaption>%s &middot; %s &middot; %s</figcaption></figure>"
                     % (thumb({"png": r.get("png"), "size": r.get("size")}, 400),
                        e(r.get("tag", "")), e(r.get("group", "")), e(r.get("name", ""))))
    parts.append("</div></body></html>")
    with open(os.path.join(out, "report.html"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(parts))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--rules", default=os.path.join(REPO, "docs", "dev", "ux_rules.json"))
    a = ap.parse_args()
    run(a.out, a.rules)
    return 0


if __name__ == "__main__":
    sys.exit(main())
