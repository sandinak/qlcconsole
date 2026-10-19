#!/usr/bin/env python3
"""
Step 1 of building the course's teaching show (docs/course/shows/).

Reads a real show choir show (DanceHysteria) and writes a smaller rig for the
classroom: the same stage (risers, towers, front towers, cyc, proscenium),
fewer fixtures with plain names, groups that read the way a designer talks,
three stage targets -- and NO outputs: no universe is patched to anything,
so the show can never send DMX on a school network.

    python3 -I derive-classroom-rig.py DanceHysteria-0.6.qlcc rig.qxw

The show it reads is never changed. Step 2 is the courseshow test, which
opens rig.qxw in the real app, adds the palettes with the Show Tree's own
command line and saves the .qlcc files (the app embeds the fixture
definitions as it saves):

    COURSE_RIG=rig.qxw build/ui/test/courseshow/... courseshow_test buildTheShows

See docs/course/shows/README.md.
"""

import sys
import xml.etree.ElementTree as ET

NS = 'http://www.qlcplus.org/Workspace'
N = '{%s}' % NS
ET.register_namespace('', NS)

# ---------------------------------------------------------------- the rig --
# Old fixture ID -> new name. Numbering: SR (stage right, the singers' right,
# the audience's left) before SL; on towers, 1 is the OUTSIDE tower (nearest
# the wings) on each side; riser units 1-4 run from SR to SL.
#
# Towers in the real show: T-1 and T-2 are SR (outside, inside), T-3 and T-4
# SL (inside, outside). AT1 / AT2 are the front towers in the house.

KEEP = {}
# Tower spots (movers with a colour ring), one per tower.
KEEP.update({'0': 'SR Tower Spot 1', '190': 'SR Tower Spot 2',
             '192': 'SL Tower Spot 2', '194': 'SL Tower Spot 1'})
# Pixel boxes (Warmdance XL-450), two per tower: one faces out over the
# stage to the audience (eye candy), one lights the cyc behind.
KEEP.update({'7': 'SR Tower Box 1', '196': 'SR Tower Box 2',
             '197': 'SL Tower Box 2', '198': 'SL Tower Box 1'})
KEEP.update({'199': 'SR Tower Cyc 1', '200': 'SR Tower Cyc 2',
             '201': 'SL Tower Cyc 2', '202': 'SL Tower Cyc 1'})
# Front towers in the house: a spot each and two washes each.
KEEP.update({'4': 'SR Front Spot', '142': 'SL Front Spot',
             '179': 'SR Front Wash 1', '180': 'SR Front Wash 2',
             '176': 'SL Front Wash 1', '177': 'SL Front Wash 2'})
# The beams standing on the top riser (UST-1..8), SR to SL.
for i, fid in enumerate(['9', '18', '19', '20', '21', '22', '23', '24']):
    KEEP[fid] = 'Back Beam %d' % (i + 1)

# Risers: rows US (top, at the back), CS, DS (front); units 1-4 SR to SL.
# The real show has eight LED strips on each riser front; the classroom keeps
# the top one ("#1"), and both bars on each riser top ("t1" SR, "t2" SL).
ROWS = ['US', 'CS', 'DS']
RISER_FACE = {}     # old name -> new name
RISER_TOP = {}
for row in ROWS:
    for unit in range(1, 5):
        RISER_FACE['%s%d #1' % (row, unit)] = '%s Riser %d' % (row, unit)
        RISER_TOP['%s%dt1' % (row, unit)] = '%s Riser %d Top SR' % (row, unit)
        RISER_TOP['%s%dt2' % (row, unit)] = '%s Riser %d Top SL' % (row, unit)

# Structures renamed the same way.
STRUCTURE_NAMES = {
    'T-1': 'SR Tower 1', 'T-2': 'SR Tower 2', 'T-3': 'SL Tower 2', 'T-4': 'SL Tower 1',
    'Bar 1': 'SR Tower 1 Top Bar', 'Bar 6': 'SR Tower 2 Top Bar',
    'Bar 7': 'SL Tower 2 Top Bar', 'Bar 8': 'SL Tower 1 Top Bar',
    'AT1': 'SR Front Tower', 'AT2': 'SL Front Tower',
    'Cyclorama 1': 'Cyc', 'Proscenium 3': 'Proscenium',
}
for row in ROWS:
    for unit in range(1, 5):
        STRUCTURE_NAMES['%sP%d' % (row, unit)] = '%s Riser %d' % (row, unit)
        STRUCTURE_NAMES['%s-%d' % (row, unit)] = '%s Riser %d Front' % (row, unit)

# What goes: the pixel walls between the towers and their trusses (FART-*),
# the pixel bars inside the towers, the second spot on each tower, the drape's
# motor (the drape stays, open), the pars and bars that aren't on the rig.
DROP_TRUSSES = {'FART-1', 'FART-2'}
DROP_MONITOR_GROUPS = {'FART-1', 'FART-2'}

# The patch: universes with names and NO output. Channels per fixture come
# from the show; fixtures are packed in this order.
UNIVERSES = [
    ('Spots, washes, beams, lanterns', lambda n: not ('Riser' in n or 'Tower Box' in n or 'Tower Cyc' in n)),
    ('Tower pixel boxes', lambda n: 'Tower Box' in n or 'Tower Cyc' in n),
    ('Riser fronts', lambda n: 'Riser' in n and 'Top' not in n),
    ('Riser tops', lambda n: 'Riser' in n and 'Top' in n),
]


def text(el, tag):
    c = el.find(N + tag)
    return c.text if c is not None else None


def main(src, dst):
    tree = ET.parse(src)
    root = tree.getroot()
    eng = root.find(N + 'Engine')
    mon = eng.find(N + 'Monitor')

    # ---- fixtures
    fixtures = eng.findall(N + 'Fixture')
    byname = {text(f, 'Name'): f for f in fixtures}
    keep = dict(KEEP)
    for old, new in list(RISER_FACE.items()) + list(RISER_TOP.items()):
        keep[text(byname[old], 'ID')] = new
    lanterns = [f for f in fixtures if text(f, 'Name').startswith('Lantern')]
    # Lanterns numbered in the order of the hangers they hang from.
    hanger_of = {r.get('FID'): r.get('Hanger') for r in mon.findall(N + 'FixtureRig')}
    lanterns.sort(key=lambda f: int(hanger_of.get(text(f, 'ID')) or 99))
    for i, f in enumerate(lanterns):
        keep[text(f, 'ID')] = 'Lantern %d' % (i + 1)

    kept = []
    for f in fixtures:
        fid = text(f, 'ID')
        if fid in keep:
            f.find(N + 'Name').text = keep[fid]
            # No fixture talks to the network by itself (the lanterns were
            # WLED devices at fixed addresses): DMX only, and no output.
            for tag in ['Connection', 'IPAddress', 'Transport']:
                for e in f.findall(N + tag):
                    f.remove(e)
            kept.append(f)
        else:
            eng.remove(f)

    # ---- the patch: universes with no outputs, fixtures packed per universe
    iomap = eng.find(N + 'InputOutputMap')
    # Nothing of the real show's network comes along: its universes, their
    # outputs, and its names for the nodes' addresses.
    for u in list(iomap):
        if u.tag != N + 'BeatGenerator':
            iomap.remove(u)
    uni_id = 0
    for label, wants in UNIVERSES:
        members = [f for f in kept if wants(text(f, 'Name'))]
        members.sort(key=lambda f: text(f, 'Name'))
        addr = 0
        first = uni_id
        for f in members:
            ch = int(text(f, 'Channels'))
            if addr + ch > 512:
                uni_id += 1
                addr = 0
            f.find(N + 'Universe').text = str(uni_id)
            f.find(N + 'Address').text = str(addr)
            addr += ch
        for u in range(first, uni_id + 1):
            name = '%s' % label if first == uni_id else '%s %d' % (label, u - first + 1)
            ET.SubElement(iomap, N + 'Universe', {'Name': name, 'ID': str(u)})
        uni_id += 1

    # ---- everything that was the real show's programming, power and network
    for tag in ['FixtureGroup', 'Palette', 'Function', 'PowerDistribution', 'ShowControl',
                'ShowTree', 'NetworkTopology']:
        for e in eng.findall(N + tag):
            eng.remove(e)
    for tag in ['VirtualConsole', 'SimpleDesk']:
        for e in root.findall(N + tag):
            root.remove(e)
    sm = eng.find(N + 'SpeedMasters')
    if sm is not None:
        for m in sm.findall(N + 'Master'):
            if m.get('Index') != '0':
                sm.remove(m)
            else:
                m.set('BPM', '120')
                m.attrib.pop('Rate', None)

    # ---- the rig: drop what went, rename what stays
    kept_ids = {text(f, 'ID') for f in kept}
    for tag, key in [('FxItem', 'ID'), ('FixtureRig', 'FID')]:
        for e in mon.findall(N + tag):
            if e.get(key) not in kept_ids:
                mon.remove(e)
            else:
                e.attrib.pop('Hidden', None)
    for e in mon.findall(N + 'Truss'):
        if e.get('Name') in DROP_TRUSSES:
            mon.remove(e)
    for e in mon.findall(N + 'MonitorGroup'):
        if e.get('Name') in DROP_MONITOR_GROUPS:
            mon.remove(e)
    for e in mon:
        name = e.get('Name')
        if name in STRUCTURE_NAMES:
            e.set('Name', STRUCTURE_NAMES[name])
        # Pictures from the designer's own computer aren't in the course.
        for a in ['SockPattern', 'SockPatternOpacity', 'FacesPattern']:
            e.attrib.pop(a, None)
        # Risers in plain black, as a school's are: the light shows on them.
        if e.tag == N + 'StagePlatform':
            e.set('Color', '#26262b' if e.get('Stackable') != 'true' else '#1c1c20')
        if e.tag == N + 'Scenery' and e.get('Fixture') is not None:
            e.attrib.pop('Fixture')         # the drape stays, open, with no motor
            e.set('State', 'Open')
    # Hangers whose lantern went (none today) would stay as bare arms; fine.

    # ---- stage targets: three a designer uses for a show choir
    for e in mon.findall(N + 'StageTarget'):
        mon.remove(e)
    targets_layer = None
    for l in mon.findall(N + 'MonitorLayer'):
        if l.get('Name', '').lower() == 'targets':
            targets_layer = l.get('ID')
            l.set('Name', 'Targets')
    # The risers run x 3.04-12.81 m; centre x 7.92. Rows (front edge, height):
    # US y 4.84 h 0.61, CS y 6.06 h 0.40, DS y 7.28 h 0.18. The curtain line
    # is at y 7.89.
    def target(tid, name, kind, x, y, z, colour, extra=None):
        a = {'ID': str(tid), 'Name': name, 'X': '%.3f' % x, 'Y': '%.3f' % y, 'Z': '%.3f' % z,
             'Color': colour, 'Kind': kind}
        if targets_layer is not None:
            a['LayerId'] = targets_layer
        if kind == 'person':
            a['PersonHeight'] = '1.650'
        if extra:
            a.update(extra)
        return ET.Element(N + 'StageTarget', a)

    insert_at = list(mon).index(mon.findall(N + 'HeadLayout')[0]) if mon.findall(N + 'HeadLayout') else len(mon)
    tgs = [target(0, 'Soloist DS', 'person', 7.92, 7.60, 0.0, '#ffb400'),
           target(1, 'Center', 'person', 7.92, 5.45, 0.396, '#ffb400')]
    line = target(2, 'Riser Line', 'line', 7.92, 5.45, 0.396, '#3cb4ff')
    # A line's points are relative to its position (its middle).
    for dx in (-4.50, 4.50):
        ET.SubElement(line, N + 'LinePoint', {'X': '%.3f' % dx, 'Y': '0.000', 'Z': '0.000'})
    tgs.append(line)
    for i, t in enumerate(tgs):
        mon.insert(insert_at + i, t)

    # ---- groups, the way a designer talks about the rig
    defs = embedded_head_layouts(root)
    groups = make_groups(kept, mon, defs)
    # Groups go right after the fixtures, where the app writes them.
    after = max(list(eng).index(f) for f in kept) + 1
    for i, g in enumerate(groups):
        eng.insert(after + i, g)

    # ---- the embedded definitions: only the ones still used, and not the
    # ones the app ships (a student's install has those; a show copy that
    # differs from it would only put "differs from this computer" in the
    # footer).
    used = {(text(f, 'Manufacturer'), text(f, 'Model')) for f in kept}
    shipped = shipped_definitions()
    emb = root.find(N + 'Embedded')
    if emb is not None:
        for e in list(emb):
            key = (e.get('Manufacturer'), e.get('Model'))
            if e.tag != N + 'FixtureDefinition' or key not in used or key in shipped:
                emb.remove(e)

    root.set('CurrentWindow', 'ShowTree')
    creator = root.find(N + 'Creator')
    if creator is not None and creator.find(N + 'Author') is not None:
        creator.find(N + 'Author').text = 'qlcconsole course'
    ET.indent(tree, ' ')
    # The app wants its doctype line, which ElementTree doesn't write.
    body = ET.tostring(root, encoding='unicode')
    with open(dst, 'w', encoding='utf-8') as out:
        out.write('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE Workspace>\n')
        out.write(body)
        out.write('\n')
    print('%d fixtures, %d groups, %d universes -> %s' % (len(kept), len(groups), uni_id, dst))


def shipped_definitions():
    """(manufacturer, model) of every definition in the repo's
    resources/fixtures -- what the app ships."""
    import os
    import re
    here = os.path.dirname(os.path.abspath(__file__))
    top = os.path.join(here, '..', '..', '..', 'resources', 'fixtures')
    out = set()
    for d, _, files in os.walk(top):
        for f in files:
            if not f.endswith('.qxf'):
                continue
            with open(os.path.join(d, f), encoding='utf-8', errors='replace') as fh:
                head = fh.read(4096)
            ma = re.search(r'<Manufacturer>([^<]*)</Manufacturer>', head)
            mo = re.search(r'<Model>([^<]*)</Model>', head)
            if ma and mo:
                out.add((ma.group(1), mo.group(1)))
    return out


def embedded_head_layouts(root):
    """Head count per (manufacturer, model, mode), from the embedded
    definitions (every fixture kept has one), and the show's head layouts."""
    fd = '{http://www.qlcplus.org/FixtureDefinition}'
    heads = {}
    emb = root.find(N + 'Embedded')
    for e in emb.findall(N + 'FixtureDefinition'):
        d = ET.fromstring(e.text.encode('utf-8'))
        for m in d.findall(fd + 'Mode'):
            heads[(e.get('Manufacturer'), e.get('Model'), m.get('Name'))] = max(1, len(m.findall(fd + 'Head')))
    layouts = {}
    for hl in root.find(N + 'Engine').find(N + 'Monitor').findall(N + 'HeadLayout'):
        cells = [tuple(int(v) for v in p.split(',')) for p in hl.text.split()]
        layouts[hl.get('Key')] = (int(hl.get('Cols')), int(hl.get('Rows')), cells)
    return heads, layouts


def make_groups(kept, mon, defs):
    heads, layouts = defs
    by = {text(f, 'Name'): f for f in kept}
    gid = [0]

    def fixture_heads(f):
        key = (text(f, 'Manufacturer'), text(f, 'Model'), text(f, 'Mode'))
        n = heads.get(key, 1)
        lay = layouts.get('|'.join(key))
        if lay and len(lay[2]) == n:
            return lay[0], lay[1], lay[2]
        return n, 1, [(i, 0) for i in range(n)]

    def group(name, path, rows):
        """rows: list of rows, each a list of fixture names, laid out left
        to right (SR to SL) -- each fixture its own head layout."""
        g = ET.Element(N + 'FixtureGroup', {'ID': str(gid[0])})
        gid[0] += 1
        ET.SubElement(g, N + 'Name').text = name
        if path:
            ET.SubElement(g, N + 'Path').text = path
        cells = []
        width = 0
        y0 = 0
        for row in rows:
            x0 = 0
            row_h = 1
            for fname in row:
                f = by[fname]
                cols, nrows, layout = fixture_heads(f)
                for h, (cx, cy) in enumerate(layout):
                    cells.append((x0 + cx, y0 + cy, text(f, 'ID'), h))
                x0 += cols
                row_h = max(row_h, nrows)
            width = max(width, x0)
            y0 += row_h
        ET.SubElement(g, N + 'Size', {'X': str(width), 'Y': str(y0)})
        for x, y, fid, h in cells:
            ET.SubElement(g, N + 'Head', {'X': str(x), 'Y': str(y), 'Fixture': fid}).text = str(h)
        return g

    risers = [['%s Riser %d' % (r, u) for u in range(1, 5)] for r in ROWS]
    tops = [['%s Riser %d Top %s' % (r, u, s) for u in range(1, 5) for s in ('SR', 'SL')] for r in ROWS]
    sr_l = lambda kind: ['SR %s 1' % kind, 'SR %s 2' % kind]
    sl_l = lambda kind: ['SL %s 2' % kind, 'SL %s 1' % kind]
    across = lambda kind: sr_l(kind) + sl_l(kind)

    out = [
        group('Risers', 'Risers', risers),
        group('US Risers', 'Risers', [risers[0]]),
        group('CS Risers', 'Risers', [risers[1]]),
        group('DS Risers', 'Risers', [risers[2]]),
        group('Riser Tops', 'Risers', tops),
        group('Front Wash', 'Front', [['SR Front Wash 1', 'SR Front Wash 2', 'SL Front Wash 2', 'SL Front Wash 1']]),
        group('Front Spots', 'Front', [['SR Front Spot', 'SL Front Spot']]),
        group('Movers', 'Towers', [across('Tower Spot')]),
        group('Pixel Boxes', 'Towers', [across('Tower Box')]),
        group('Cyc Washes', 'Towers', [across('Tower Cyc')]),
        group('Towers SR', 'Towers', [sr_l('Tower Spot'), sr_l('Tower Box'), sr_l('Tower Cyc')]),
        group('Towers SL', 'Towers', [sl_l('Tower Spot'), sl_l('Tower Box'), sl_l('Tower Cyc')]),
        group('Back Beams', 'Extras', [['Back Beam %d' % i for i in range(1, 9)]]),
        group('Lanterns', 'Extras', [['Lantern %d' % i for i in range(1, 7)]]),
    ]
    return out


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
