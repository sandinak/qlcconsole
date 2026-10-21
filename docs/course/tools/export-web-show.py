#!/usr/bin/env python3
"""
Export the teaching show for the browser practice version (docs/course/web/).

Reads a course show (by default shows/ShowChoir-Classroom.qlcc) and writes a
small JSON file with only what the practice page needs to draw the stage and
build looks:

  - fixtures: where each light is, what kind it is, how many heads it has
    and what it can do (colour mixing, a colour wheel, a dimmer, moving);
  - groups and their Library folders (and which heads of a pixel box a
    group holds: its RGB pixels or its white ones);
  - stage targets;
  - palettes (colour, dimmer, position/aim, shutter);
  - the stage: risers, trusses, audience towers, the cyc, the main drape and
    the proscenium.

    python3 -I docs/course/tools/export-web-show.py            # the defaults
    python3 -I docs/course/tools/export-web-show.py SHOW.qlcc OUT_DIR

It writes OUT_DIR/show.json and OUT_DIR/show-data.js (the same data as
`window.SHOW_DATA = {...}`, so the page also works opened as a file, where a
browser won't let a page read a .json file next to it).

Units are metres. x runs from stage right (the singers' right, the
audience's left) to stage left; y from upstage (the cyc) to downstage and
into the house; z is up.

The show file is never changed. Licence: Apache-2.0, as qlcconsole is.
"""

import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
COURSE = os.path.dirname(HERE)
DEFAULT_SHOW = os.path.join(COURSE, 'shows', 'ShowChoir-Classroom.qlcc')
DEFAULT_OUT = os.path.join(COURSE, 'web')

W = '{http://www.qlcplus.org/Workspace}'
D = '{http://www.qlcplus.org/FixtureDefinition}'
NO_ID = '4294967295'

# What a fixture can do, for the models whose definitions aren't embedded in
# the show (the app ships them). Embedded definitions are read instead.
KNOWN_MODELS = {
    'Wall Washer Light Bar': dict(colour='rgbw', dimmer=False, moves=False),
    'Focus Spot Three Z': dict(colour='wheel', dimmer=True, moves=True),
    'Lantern': dict(colour='rgb', dimmer=True, moves=False),
}
# The AT spots' shutter rests closed: "White, Full" lights nothing until the
# Shutter palette Open (COURSE_PLAN.md section 8, item 7).
SHUTTER_CLOSED = {'Focus Spot Three Z'}

# How the page draws each group's lights (one role per light: the first
# group in this list that holds it).
ROLES = [
    ('Steps', 'step'), ('StepTops', 'steptop'),
    ('TrussC-RGB', 'trusscyc'), ('TrussW-RGB', 'trusswash'),
    ('Truss-Spots', 'trussspot'), ('USTs', 'ust'),
    ('AT-Spots', 'atspot'), ('AT-W-ALL', 'atwash'), ('Lanterns', 'lantern'),
]


def num(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def r3(v):
    return round(v, 3)


def text(e, tag, default=''):
    c = e.find(W + tag)
    return c.text if c is not None and c.text is not None else default


def parse_definition(xml):
    """Return {mode name: {'heads': [kinds], 'caps': {...}}} for one embedded
    fixture definition. A head's kind is 'rgb', 'w' or 'rgbw'."""
    root = ET.fromstring(re.sub(r'^<\?xml[^>]*>\s*(<!DOCTYPE[^>]*>)?', '', xml.strip()))
    chans = {}
    for c in root.findall(D + 'Channel'):
        pre = c.get('Preset') or ''
        grp = c.find(D + 'Group')
        col = c.find(D + 'Colour')
        g = grp.text if grp is not None else ''
        colour = col.text if col is not None else ''
        what = set()
        if pre in ('IntensityRed', 'IntensityGreen', 'IntensityBlue') or \
                (g == 'Intensity' and colour in ('Red', 'Green', 'Blue')):
            what.add('rgb')
        if pre == 'IntensityWhite' or (g == 'Intensity' and colour == 'White'):
            what.add('w')
        if pre in ('IntensityDimmer', 'IntensityMasterDimmer') or (g == 'Intensity' and not colour):
            what.add('dimmer')
        if g == 'Colour' or pre.startswith('ColorMacro') or pre.startswith('ColorWheel'):
            what.add('wheel')
        if pre.startswith('PositionPan') or pre.startswith('PositionTilt') or g in ('Pan', 'Tilt'):
            what.add('moves')
        chans[c.get('Name')] = what
    modes = {}
    for m in root.findall(D + 'Mode'):
        names = {ch.get('Number'): ch.text for ch in m.findall(D + 'Channel')}
        every = set()
        for n in names.values():
            every |= chans.get(n, set())
        heads = []
        for h in m.findall(D + 'Head'):
            k = set()
            for ch in h.findall(D + 'Channel'):
                k |= chans.get(names.get(ch.text), set())
            heads.append('rgbw' if {'rgb', 'w'} <= k else 'rgb' if 'rgb' in k else 'w' if 'w' in k else 'other')
        colour = 'rgbw' if {'rgb', 'w'} <= every else 'rgb' if 'rgb' in every else \
            'wheel' if 'wheel' in every else 'white'
        modes[m.get('Name')] = dict(heads=heads, caps=dict(
            colour=colour, dimmer='dimmer' in every, moves='moves' in every))
    return root.findtext(D + 'Model'), modes


def main():
    show = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SHOW
    out_dir = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    raw = open(show, encoding='utf-8').read()

    # Embedded fixture definitions (CDATA inside the workspace).
    defs = {}
    for xml in re.findall(r'<FixtureDefinition [^>]*><!\[CDATA\[(.*?)\]\]>', raw, re.S):
        model, modes = parse_definition(xml)
        defs[model] = modes
    body = re.sub(r'<!\[CDATA\[.*?\]\]>', '', raw, flags=re.S)
    body = re.sub(r'^<\?xml[^>]*>\s*<!DOCTYPE[^>]*>', '', body.strip())
    root = ET.fromstring(body)
    eng = root.find(W + 'Engine')
    mon = eng.find(W + 'Monitor')

    items = {e.get('ID'): e.attrib for e in mon.findall(W + 'FxItem')}
    rigs = {e.get('FID'): e.attrib for e in mon.findall(W + 'FixtureRig')}
    trusses = {e.get('ID'): e.attrib for e in mon.findall(W + 'Truss')}
    towers = {}
    for t in mon.findall(W + 'Tower'):
        towers[t.get('ID')] = dict(t.attrib, shelves=[num(s.get('Z')) for s in t.findall(W + 'Shelf')])
    platforms = {e.get('ID'): e.attrib for e in mon.findall(W + 'StagePlatform')}
    hangers = {e.get('ID'): e.attrib for e in mon.findall(W + 'Hanger')}
    # A platform anchored by a group sits at that group's OZ (the step
    # fronts stand on the riser below them).
    plat_z = {}
    for g in mon.findall(W + 'MonitorGroup'):
        if g.get('AnchorKind') == 'platform':
            plat_z[g.get('AnchorId')] = num(g.get('OZ'))

    def truss_point(tid, offset):
        t = trusses[tid]
        if t.get('Type') == 'Vertical':
            return num(t['OriginX']), num(t['OriginY']), offset
        return (num(t['OriginX']) + num(t.get('DirX')) * offset,
                num(t['OriginY']) + num(t.get('DirY')) * offset, num(t.get('OriginZ')))

    def place(fid):
        """World position (and length, for bars) of one fixture."""
        rg = rigs.get(fid, {})
        it = items.get(fid, {})
        length = 0.0
        if rg.get('Riser'):
            p = platforms[rg['Riser']]
            x = num(p['OriginX']) + num(rg.get('RiserU'))
            base = plat_z.get(rg['Riser'], 0.0)
            if rg.get('RiserFace') == '1':        # on top of the step
                y = num(p['OriginY']) + num(rg.get('RiserV'))
                z = base + num(p['Height'])
                length = num(p['Width']) / 2
            else:                                 # along its front
                y = num(p['OriginY']) + num(p['Depth'])
                z = base + num(rg.get('RiserV'))
                length = num(p['Width'])
            return x, y, z, length
        if rg.get('Truss') and rg['Truss'] != NO_ID:
            x, y, z = truss_point(rg['Truss'], num(rg.get('Offset')))
            return x, y + num(rg.get('TrussCrossY')), z, length
        if rg.get('Tower'):
            t = towers[rg['Tower']]
            shelves = t['shelves']
            i = int(rg.get('TShelf', 0))
            z = shelves[i] if i < len(shelves) else num(t['Height'])
            return (num(t['OriginX']) + num(rg.get('TU'), num(t['Width']) / 2),
                    num(t['OriginY']) + num(rg.get('TV'), num(t['Depth']) / 2), z, length)
        if rg.get('Hanger'):
            h = hangers[rg['Hanger']]
            if h.get('Parent') == 'tower':
                t = towers[h['ParentId']]
                x, y = num(t['OriginX']) + num(t['Width']) / 2, num(t['OriginY']) + num(t['Depth']) / 2
                z = num(h.get('Along'))
            else:
                x, y, z = truss_point(h['ParentId'], num(h.get('Along')))
            a = math.radians(num(h.get('Angle')))
            arm = num(h.get('Arm'))
            return x - arm * math.sin(a), y + arm * math.cos(a), z - num(h.get('Drop')) - 0.25, length
        # Standing on the stage (the upstage beams on the top riser).
        x, y = num(it.get('XPos')) / 1000, num(it.get('YPos')) / 1000
        z = num(it.get('ZPos'))
        for p in platforms.values():
            px, py = num(p['OriginX']), num(p['OriginY'])
            if px <= x <= px + num(p['Width']) and py - 0.15 <= y <= py + num(p['Depth']):
                z = max(z, plat_z.get(p['ID'], 0.0) + num(p['Height']))
        return x, y, z, length

    fixtures = {}
    order = []
    for f in eng.findall(W + 'Fixture'):
        fid = text(f, 'ID')
        model, mode = text(f, 'Model'), text(f, 'Mode')
        mdef = defs.get(model, {}).get(mode)
        if mdef:
            caps, heads = dict(mdef['caps']), mdef['heads'] or ['rgb']
        else:
            caps, heads = dict(KNOWN_MODELS.get(model, dict(colour='rgb', dimmer=True, moves=False))), None
        x, y, z, length = place(fid)
        fixtures[fid] = dict(id=int(fid), name=text(f, 'Name'), model=model,
                             x=r3(x), y=r3(y), z=r3(z), **caps)
        if length:
            fixtures[fid]['len'] = r3(length)
        if model in SHUTTER_CLOSED:
            fixtures[fid]['shutterClosed'] = True
        fixtures[fid]['_heads'] = heads
        order.append(fid)

    # Groups: which fixtures, and which of their heads.
    groups = []
    for g in eng.findall(W + 'FixtureGroup'):
        per = {}
        for h in g.findall(W + 'Head'):
            per.setdefault(h.get('Fixture'), set()).add(int(h.text))
        members = []
        for fid in sorted(per, key=lambda k: order.index(k) if k in order else 1e9):
            if fid not in fixtures:
                continue
            heads = fixtures[fid]['_heads']
            used = per[fid]
            part = 'all'
            if heads and len(used) < len(heads):
                kinds = {heads[i] for i in used if i < len(heads)}
                part = 'rgb' if kinds <= {'rgb'} else 'w' if kinds <= {'w'} else 'all'
            members.append([int(fid), part])
        groups.append(dict(id=int(g.get('ID')), name=text(g, 'Name'),
                           folder=text(g, 'Path'), fixtures=members))

    for fid, fx in fixtures.items():
        heads = fx.pop('_heads')
        if heads:
            fx['heads'] = len(heads)
            if 'rgb' in heads and 'w' in heads:
                fx['parts'] = {'rgb': heads.count('rgb'), 'w': heads.count('w')}
        else:
            fx['heads'] = 1
    # Head counts for fixtures without an embedded definition: the most heads
    # any group uses of them.
    for g in eng.findall(W + 'FixtureGroup'):
        per = {}
        for h in g.findall(W + 'Head'):
            per.setdefault(h.get('Fixture'), set()).add(int(h.text))
        for fid, used in per.items():
            if fid in fixtures and fixtures[fid]['model'] in KNOWN_MODELS:
                fixtures[fid]['heads'] = max(fixtures[fid]['heads'], max(used) + 1)

    roles = {}
    by_name = {g['name']: g for g in groups}
    for gname, role in ROLES:
        for fid, _ in by_name.get(gname, {}).get('fixtures', []):
            roles.setdefault(fid, role)
    for fx in fixtures.values():
        fx['role'] = roles.get(fx['id'], 'other')

    # Folders in the order the Library shows them.
    folders = []
    for g in groups:
        if g['folder'] not in folders:
            folders.append(g['folder'])
    folders.sort()

    targets = []
    for t in mon.findall(W + 'StageTarget'):
        tg = dict(id=int(t.get('ID')), name=t.get('Name'), kind=t.get('Kind'),
                  x=r3(num(t.get('X'))), y=r3(num(t.get('Y'))), z=r3(num(t.get('Z'))))
        if t.get('PersonHeight'):
            tg['height'] = r3(num(t.get('PersonHeight')))
        pts = t.findall(W + 'LinePoint')
        if pts:
            tg['line'] = [[r3(tg['x'] + num(p.get('X'))), r3(tg['y'] + num(p.get('Y'))),
                           r3(tg['z'] + num(p.get('Z')))] for p in pts]
        targets.append(tg)

    palettes = []
    for p in eng.findall(W + 'Palette'):
        kind, name = p.get('Type'), p.get('Name')
        if kind == 'Color':
            palettes.append(dict(type='Colour', name=name, color=p.get('Value', '#ffffff')[:7]))
        elif kind == 'Dimmer':
            palettes.append(dict(type='Dimmer', name=name, level=round(num(p.get('Value')) / 255, 3)))
        elif kind == 'Aim':
            palettes.append(dict(type='Position', name=name, target=int(p.get('StageTarget', 0))))
        elif kind == 'Shutter':
            palettes.append(dict(type='Shutter', name=name, open=True))

    stage = dict(risers=[], trusses=[], towers=[])
    for p in platforms.values():
        stage['risers'].append(dict(name=p['Name'], x=r3(num(p['OriginX'])), y=r3(num(p['OriginY'])),
                                    z=r3(plat_z.get(p['ID'], 0.0)), w=r3(num(p['Width'])),
                                    d=r3(num(p['Depth'])), h=r3(num(p['Height']))))
    for t in trusses.values():
        if t.get('Type') == 'Vertical':
            stage['trusses'].append(dict(name=t['Name'], x=r3(num(t['OriginX'])), y=r3(num(t['OriginY'])),
                                         h=r3(num(t['Length'])), w=r3(num(t['Width']))))
    for t in towers.values():
        stage['towers'].append(dict(name=t['Name'], x=r3(num(t['OriginX'])), y=r3(num(t['OriginY'])),
                                    w=r3(num(t['Width'])), d=r3(num(t['Depth'])), h=r3(num(t['Height']))))
    for s in mon.findall(W + 'Scenery'):
        a = s.attrib
        thing = dict(x=r3(num(a.get('OriginX'))), y=r3(num(a.get('OriginY'))),
                     w=r3(num(a.get('Width'))), h=r3(num(a.get('Height'))))
        if a.get('OpeningWidth'):
            thing['openW'] = r3(num(a.get('OpeningWidth')))
        if a.get('OpeningHeight'):
            thing['openH'] = r3(num(a.get('OpeningHeight')))
        if a.get('State'):
            thing['state'] = a['State']
        key = {'0': 'cyc', '2': 'drape', '8': 'proscenium'}.get(a.get('Kind'))
        if key:
            stage[key] = thing

    data = dict(
        show=os.path.splitext(os.path.basename(show))[0],
        about='Exported by docs/course/tools/export-web-show.py for the browser practice version. '
              'Metres; x: stage right to stage left, y: upstage to the house, z: up.',
        fixtures=[fixtures[f] for f in order],
        folders=folders,
        groups=groups,
        targets=targets,
        palettes=palettes,
        stage=stage,
    )
    os.makedirs(out_dir, exist_ok=True)
    text_json = json.dumps(data, separators=(',', ':'), ensure_ascii=False)
    with open(os.path.join(out_dir, 'show.json'), 'w', encoding='utf-8') as fh:
        fh.write(text_json + '\n')
    with open(os.path.join(out_dir, 'show-data.js'), 'w', encoding='utf-8') as fh:
        fh.write('// Made by docs/course/tools/export-web-show.py from %s -- do not edit.\n'
                 'window.SHOW_DATA = %s;\n' % (os.path.basename(show), text_json))
    print('%d fixtures, %d groups, %d targets, %d palettes -> %s (%d bytes)' % (
        len(fixtures), len(groups), len(targets), len(palettes), out_dir, len(text_json)))


if __name__ == '__main__':
    main()
