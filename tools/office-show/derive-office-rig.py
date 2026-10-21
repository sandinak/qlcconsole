#!/usr/bin/env python3
"""
Step 1 of building Office.qlcc, the show for testing qlcconsole on
Branson's office rig.

Reads the office universe of a real show (CampEvo-2.1, "U02-Office": the
source of truth for what is hung there, and where it is patched), checks it
against DanceHysteria-0.6's "U01-Office", and writes a small rig with only
those fixtures in one universe, "U01-Office", with NO output (the office
Art-Net node isn't up yet) and the show network 172.18.0.0/20:

    python3 -I derive-office-rig.py CampEvo-2.1.qxw DanceHysteria-0.6.qlcc rig.qxw

Neither show is changed. Step 2 is the officeshow test, which opens rig.qxw
in the real app, builds the stage, groups, palettes and the Show Tree
production, and saves Office.qlcc (the app embeds the definitions):

    OFFICE_RIG=rig.qxw OFFICE_OUT=Office.qlcc \\
        build/ui/test/officeshow/... officeshow_test buildTheShow

The ring mover is loaded as "LED Movinghead+Circle (2 heads)": the same
channels, the same modes, with its colour ring as a head of its own, so a
Show Tree layer can set the ring (engine/test/common/fixtures).
"""

import sys
import xml.etree.ElementTree as ET

NS = 'http://www.qlcplus.org/Workspace'
N = '{%s}' % NS
ET.register_namespace('', NS)

SOURCE_UNIVERSE = 'U02-Office'
UNIVERSE_NAME = 'U01-Office'
RIG_NETWORK = '172.18.0.0/20'

# CampEvo name -> (new name, (manufacturer, model) to use, or None to keep).
RENAME = {
    'OT1-SC': ('Office Ring Mover', ('Branson', 'LED Movinghead+Circle (2 heads)')),
    'OT-P #1': ('Office Par 1', None),
    'OT-P #2': ('Office Par 2', None),
    'OT-S': ('Office Spot', None),
    'OT-3z #1': ('Office 3Z', None),
}
# What the rig must be (1-based DMX address, model, mode), as Branson gave it.
EXPECT = {
    'OT1-SC': (1, 'LED Movinghead+Circle', '12 Channel'),
    'OT-P #1': (25, 'LM70', '9 Channel'),
    'OT-P #2': (34, 'LM70', '9 Channel'),
    'OT-S': (13, 'LED SPOT', '10 Channel'),
    'OT-3z #1': (43, 'Focus Spot Three Z', '18 Channel'),
}


def text(el, tag):
    c = el.find(N + tag)
    return c.text if c is not None else None


def universe_fixtures(path, uname):
    root = ET.parse(path).getroot()
    eng = root.find(N + 'Engine')
    uid = None
    for u in eng.find(N + 'InputOutputMap').findall(N + 'Universe'):
        if u.get('Name') == uname:
            uid = u.get('ID')
    if uid is None:
        sys.exit('%s: no universe "%s"' % (path, uname))
    return [f for f in eng.findall(N + 'Fixture') if text(f, 'Universe') == uid]


def main(campevo, dance, dst):
    office = universe_fixtures(campevo, SOURCE_UNIVERSE)
    names = sorted(text(f, 'Name') for f in office)
    if names != sorted(EXPECT):
        sys.exit('CampEvo %s has %s, expected %s' % (SOURCE_UNIVERSE, names, sorted(EXPECT)))
    for f in office:
        addr, model, mode = EXPECT[text(f, 'Name')]
        got = (int(text(f, 'Address')) + 1, text(f, 'Model'), text(f, 'Mode'))
        if got != (addr, model, mode):
            sys.exit('%s: %s, expected %s' % (text(f, 'Name'), got, (addr, model, mode)))

    # The cross-check: DanceHysteria's office universe has the two LM70s and
    # the LED SPOT at the same addresses and modes (its stage curtain, which
    # isn't in the office, is left out).
    dh = {}
    for f in universe_fixtures(dance, 'U01-Office'):
        if text(f, 'Model') == 'Stage Curtain':
            continue
        dh.setdefault((text(f, 'Model'), text(f, 'Mode')), []).append(int(text(f, 'Address')) + 1)
    ce = {}
    for f in office:
        ce.setdefault((text(f, 'Model'), text(f, 'Mode')), []).append(int(text(f, 'Address')) + 1)
    for key, addrs in dh.items():
        if sorted(addrs) != sorted(ce.get(key, [])):
            sys.exit('DanceHysteria has %s at %s; CampEvo at %s' % (key, addrs, ce.get(key)))
    print('cross-check: DanceHysteria U01-Office agrees on %s' % sorted(dh))

    # ---- the rig: these five, in universe 0, IDs in address order
    root = ET.Element(N + 'Workspace', {'CurrentWindow': 'ShowTree'})
    creator = ET.SubElement(root, N + 'Creator')
    ET.SubElement(creator, N + 'Name').text = 'qlcconsole'
    ET.SubElement(creator, N + 'Version').text = 'office-show'
    ET.SubElement(creator, N + 'Author').text = 'Branson Matheson'
    eng = ET.SubElement(root, N + 'Engine')
    io = ET.SubElement(eng, N + 'InputOutputMap')
    ET.SubElement(io, N + 'RigNetwork').text = RIG_NETWORK
    ET.SubElement(io, N + 'BeatGenerator', {'BeatType': 'Disabled', 'BPM': '0'})
    # No Output: unpatched until the office node's address is known.
    ET.SubElement(io, N + 'Universe', {'Name': UNIVERSE_NAME, 'ID': '0'})

    office.sort(key=lambda f: int(text(f, 'Address')))
    for i, f in enumerate(office):
        new_name, model = RENAME[text(f, 'Name')]
        fx = ET.SubElement(eng, N + 'Fixture')
        ET.SubElement(fx, N + 'Manufacturer').text = model[0] if model else text(f, 'Manufacturer')
        ET.SubElement(fx, N + 'Model').text = model[1] if model else text(f, 'Model')
        ET.SubElement(fx, N + 'Mode').text = text(f, 'Mode')
        ET.SubElement(fx, N + 'ID').text = str(i)
        ET.SubElement(fx, N + 'Name').text = new_name
        ET.SubElement(fx, N + 'Universe').text = '0'
        ET.SubElement(fx, N + 'Address').text = text(f, 'Address')
        ET.SubElement(fx, N + 'Channels').text = text(f, 'Channels')
        print('%-18s <- %-9s %s %s, %s, DMX %d' % (new_name, text(f, 'Name'),
                                                  text(fx, 'Manufacturer'), text(fx, 'Model'),
                                                  text(fx, 'Mode'), int(text(f, 'Address')) + 1))

    ET.indent(root, ' ')
    with open(dst, 'w', encoding='utf-8') as out:
        out.write('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE Workspace>\n')
        out.write(ET.tostring(root, encoding='unicode'))
        out.write('\n')
    print('%d fixtures -> %s' % (len(office), dst))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
