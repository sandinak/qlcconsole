#!/usr/bin/env python3
"""
Scale a real show up with a synthetic block: merge the fixtures, fixture
groups, functions and universes of a `qlcstress emit` workspace into a real
.qlcc/.qxw, every id moved past the show's own, the block's universes after
the show's (and given no output: they are load, not light).

  merge_block.py <show.qlcc> <block.qxw> <out.qlcc> [--name-prefix STRESS]

Text-level on purpose: the show is written back byte for byte except where
the block goes in (an XML round trip would reformat it and drop nothing we
could see, but it is the show people will open).
"""
import argparse
import re
import sys

INVALID = 4294967295


def ids(pattern, text):
    return [int(x) for x in re.findall(pattern, text) if int(x) != INVALID]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("show")
    ap.add_argument("block")
    ap.add_argument("out")
    ap.add_argument("--name-prefix", default="STRESS")
    a = ap.parse_args()

    show = open(a.show, encoding="utf-8").read()
    block = open(a.block, encoding="utf-8").read()

    fx_off = max(ids(r"<Fixture>\s*<Manufacturer>.*?<ID>(\d+)</ID>", show) or [0]) + 1000
    grp_off = max(ids(r'<FixtureGroup ID="(\d+)"', show) or [0]) + 100
    fn_off = max(ids(r'<Function ID="(\d+)"', show) or [0]) + 100
    uni_off = max(ids(r'<Universe [^>]*ID="(\d+)"', show) or [0]) + 1

    def section(tag_open, text):
        """Every top-level element of one kind, as text."""
        return re.findall(tag_open, text, re.S)

    # Fixtures: id and universe moved, names marked.
    fixtures = section(r"<Fixture>\s*<Manufacturer>.*?</Fixture>", block)
    def fix(m):
        t = m.group(0)
        t = re.sub(r"<ID>(\d+)</ID>", lambda k: "<ID>%d</ID>" % (int(k.group(1)) + fx_off), t)
        t = re.sub(r"<Universe>(\d+)</Universe>",
                   lambda k: "<Universe>%d</Universe>" % (int(k.group(1)) + uni_off), t)
        t = re.sub(r"<Name>([^<]*)</Name>", lambda k: "<Name>%s %s</Name>" % (a.name_prefix, k.group(1)), t)
        return t
    fixtures = [fix(re.match(r"(?s).*", f)) for f in fixtures]

    groups = section(r'<FixtureGroup ID="\d+">.*?</FixtureGroup>', block)
    def grp(t):
        t = re.sub(r'<FixtureGroup ID="(\d+)"', lambda k: '<FixtureGroup ID="%d"' % (int(k.group(1)) + grp_off), t)
        t = re.sub(r'Fixture="(\d+)"', lambda k: 'Fixture="%d"' % (int(k.group(1)) + fx_off), t)
        t = re.sub(r"<Name>([^<]*)</Name>", lambda k: "<Name>%s %s</Name>" % (a.name_prefix, k.group(1)), t, count=1)
        return t
    groups = [grp(g) for g in groups]

    functions = section(r'<Function ID="\d+" Type="[^"]*".*?</Function>', block)
    def fn(t):
        t = re.sub(r'<Function ID="(\d+)"', lambda k: '<Function ID="%d"' % (int(k.group(1)) + fn_off), t, count=1)
        t = re.sub(r'(Name=")([^"]*)"', lambda k: '%s%s %s"' % (k.group(1), a.name_prefix, k.group(2)), t, count=1)
        t = re.sub(r'<FixtureVal ID="(\d+)"', lambda k: '<FixtureVal ID="%d"' % (int(k.group(1)) + fx_off), t)
        t = re.sub(r"(<Step [^>]*>)(\d+)(</Step>)",
                   lambda k: "%s%d%s" % (k.group(1), int(k.group(2)) + fn_off, k.group(3)), t)
        t = re.sub(r"<FixtureGroup>(\d+)</FixtureGroup>",
                   lambda k: "<FixtureGroup>%d</FixtureGroup>" % (int(k.group(1)) + grp_off), t)
        return t
    functions = [fn(f) for f in functions]

    n_uni = len(set(ids(r'<Universe [^>]*ID="(\d+)"', block)))
    universes = ['<Universe Name="%s %d" ID="%d"/>' % (a.name_prefix, u + 1, u + uni_off)
                 for u in range(n_uni)]

    def insert_after_last(text, pattern, items, what):
        hits = list(re.finditer(pattern, text, re.S))
        if not hits:
            sys.exit("no %s in the show to insert after" % what)
        at = hits[-1].end()
        return text[:at] + "\n" + "\n".join(items) + text[at:]

    out = show
    # Last in each run first would shift the others; go from the end back.
    out = insert_after_last(out, r'<Function ID="\d+" Type="[^"]*".*?</Function>', functions, "functions")
    out = insert_after_last(out, r'<FixtureGroup ID="\d+">.*?</FixtureGroup>', groups, "groups")
    out = insert_after_last(out, r"<Fixture>\s*<Manufacturer>.*?</Fixture>", fixtures, "fixtures")
    io_end = out.index("</InputOutputMap>")
    out = out[:io_end] + "\n".join(universes) + "\n" + out[io_end:]

    open(a.out, "w", encoding="utf-8").write(out)
    print("merged: +%d universes (from %d), +%d fixtures (ids from %d), +%d groups, +%d functions (ids from %d)"
          % (n_uni, uni_off, len(fixtures), fx_off, len(groups), len(functions), fn_off))


if __name__ == "__main__":
    main()
