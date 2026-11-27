#!/usr/bin/env python3
"""Redraws four built-in surface layouts (FaderPort 2, PMJ Black 1, APC40 mkII,
APC64) from measurements of the boards' top views.  Channels, faces, jobs, lamps,
roles and the lamp tables are read from the layout file as it is and kept; only
the places, shapes and printed sections are written.  Run from the repo root:
    tools/gen-surface-layouts.py
Coordinates below are pixels of the reference photo, divided by one key pitch
(U) to give grid units, so the proportions are the board's."""
import json, os, sys

D = "resources/inputprofiles/layouts/"

def load(name):
    with open(D + name + ".layout.json") as f:
        return json.load(f)

def save(name, d):
    """One control a line, as the files have always been laid out."""
    order = ["ch", "also", "x", "y", "w", "h", "kind", "shape", "face", "job", "lamp"]
    def line(o, keys):
        parts = ["%s: %s" % (json.dumps(k), json.dumps(o[k], ensure_ascii=False)) for k in keys if k in o]
        return "{" + ", ".join(parts) + "}"
    out = ["{"]
    items = []
    for k, v in d.items():
        if k == "controls":
            body = ",\n".join("    " + line(c, order) for c in v)
            items.append('  "controls": [\n%s\n  ]' % body)
        elif k == "sections":
            body = ",\n".join("    " + line(c, ["label", "x", "y", "w", "h"]) for c in v)
            items.append('  "sections": [\n%s\n  ]' % body)
        elif k == "roles":
            body = ",\n".join("    " + line(c, ["role", "ch", "seed"]) for c in v)
            items.append('  "roles": [\n%s\n  ]' % body)
        else:
            items.append("  %s: %s" % (json.dumps(k), json.dumps(v, ensure_ascii=False)))
    out.append(",\n".join(items))
    out.append("}")
    with open(D + name + ".layout.json", "w") as f:
        f.write("\n".join(out) + "\n")

# Millimetres of the real board in one grid unit, from each board's published
# size (key pitch for the PMJ): to be checked with a rule before a sheet is cut.
UNIT_MM = {"PreSonus-FaderPort-2": 17.5, "PMJ-Black-1": 19.05,
           "Akai-APC40-mkII": 36.2, "Akai-APC64": 24.5}

class Board:
    def __init__(self, name, U, W, H):
        self.d = load(name)
        self.name, self.U = name, U
        self.d["width"], self.d["height"] = round(W / U, 2), round(H / U, 2)
        self.by = {c["ch"]: c for c in self.d["controls"]}
        self.done = set()
        self.d["sections"] = []
        self.d["unitMm"] = UNIT_MM[name]

    def r(self, x, y, w, h):
        u = self.U
        return dict(x=round(x / u, 3), y=round(y / u, 3), w=round(w / u, 3), h=round(h / u, 3))

    def put(self, ch, x, y, w, h, kind=None, shape=None, face=None, also=None):
        c = self.by[ch]
        c.update(self.r(x, y, w, h))
        if kind: c["kind"] = kind
        if face: c["face"] = face
        if shape: c["shape"] = shape
        else: c.pop("shape", None)
        if also is not None: c["also"] = sorted(set(c.get("also", []) + also))
        self.done.add(ch)

    def center(self, ch, cx, cy, w, h=None, **kw):
        h = h or w
        self.put(ch, cx - w / 2, cy - h / 2, w, h, **kw)

    def section(self, label, x, y, w, h):
        s = {"label": label}
        s.update(self.r(x, y, w, h))
        self.d["sections"].append(s)

    def finish(self):
        order = list(self.done)
        missing = [c for c in self.by if c not in self.done]
        # a channel that is another control's "also" is placed with it
        alsos = {a for c in self.d["controls"] for a in c.get("also", [])}
        missing = [c for c in missing if c not in alsos]
        if missing:
            sys.exit("%s: not placed: %s" % (self.name, missing))
        self.d["controls"] = [c for c in self.d["controls"] if c["ch"] in self.done]
        save(self.name, self.d)

# ---------------------------------------------------------------- FaderPort 2
def faderport():
    b = Board("PreSonus-FaderPort-2", 63, 475, 770)
    b.put(513, 62, 150, 66, 387, face="Fader")
    b.center(232, 95, 132, 22, kind="button", shape="round", face="Fader touch")
    cols = [189.5, 253, 317.5, 381]
    for ch, cx in zip((136, 144, 128, 198), cols):      # Solo Mute Arm Shift
        b.center(ch, cx, 128, 42, 28)
    for ch, cx in zip((204, 205, 203, 202), cols):      # (Bypass) Touch Write Read
        b.center(ch, cx, 186, 42, 28)
    b.put(174, 189, 242, 42, 28)                         # Prev
    b.put(16, 253, 218, 64, 64, kind="encoder", shape="round", face="Knob (push)", also=[160])
    for ch, cx in zip((153, 217, 213, 212), cols):      # Master Click Section Marker
        b.center(ch, cx, 374, 42, 28)
    b.center(214, 222, 468, 40, kind="button", shape="round")
    b.center(219, 285, 468, 38, shape="round")
    b.center(220, 350, 468, 38, shape="round")
    b.center(221, 190, 523, 40)
    b.center(223, 382, 523, 40, shape="round")
    b.center(222, 286, 553, 66, shape="round")
    b.section("", 57, 118, 76, 419)
    b.section("", 165, 218, 245, 192)
    b.d["sections"][-1]["label"] = ""
    b.d["controls"] = [c for c in b.d["controls"] if c["ch"] != 160]
    b.finish()

# ------------------------------------------------------------------ PMJ Black 1
def pmj():
    b = Board("PMJ-Black-1", 1.0, 14.0, 10.4)
    b.U = 1.0
    sx = 3.8
    for i in range(10):
        n = i + 1
        x = sx + i
        b.put(32934 + i if i < 10 else 0, x + 0.05, 2.0, 0.9, 0.8, face=str(n))
        b.put(32921 + i, x + 0.05, 2.9, 0.9, 0.8)
        b.put(32769 + i, x + 0.05, 3.95, 0.9, 4.15)
        b.put(32908 + i, x + 0.05, 8.3, 0.9, 0.9)
        b.put(32897 + i, x + 0.05, 9.3, 0.9, 0.9)
    for i, ch in enumerate((32779, 32780, 32781, 32782)):
        cx = sx + 1.45 + 2.45 * i
        b.put(ch, cx - 0.85, 0.1, 1.7, 1.7, kind="encoder", shape="round")
    grid = [(32946, 32947, 32948), (32949, 32931, 32944), (32945, 32919, 32920), (32918, 32932, 32933)]
    for j, row in enumerate(grid):
        for i, ch in enumerate(row):
            b.put(ch, 0.2 + 1.1 * i, 1.9 + 1.1 * j, 1.0, 1.0)
    b.put(32768, 1.2, 6.5, 1.0, 1.7, face="Master")
    b.put(32896, 0.8, 8.3, 1.8, 0.9, face="Go")
    b.put(32907, 0.8, 9.3, 1.8, 0.9, face="Back")
    b.section("", 0.1, 1.8, 3.5, 4.7)
    b.section("Master", 0.1, 6.4, 3.5, 3.9)
    b.section("", sx - 0.05, 0.05, 10.1, 1.85)
    b.section("Strips", sx - 0.05, 1.9, 10.1, 8.4)
    b.finish()

# ------------------------------------------------------------------ APC40 mkII
def apc40():
    b = Board("Akai-APC40-mkII", 114, 1500, 900)
    cx = [75 + 114 * i for i in range(8)]
    for i in range(8):
        b.center(48 + i, cx[i], 70, 78)
    for c in range(8):
        for r in range(5):
            ch = {0: 160, 1: 152, 2: 144, 3: 136, 4: 128}[r] + c
            b.center(ch, cx[c], [166, 221, 276, 330, 384][r], 102, 38)
    for r, ch in enumerate((210, 211, 212, 213, 214)):
        b.center(ch, 970, [166, 221, 276, 330, 384][r], 62, 38)
    b.center(209, 970, 444, 62, 42)
    b.center(208, 970, 506, 62, 36)
    byface = {c["face"]: c["ch"] for c in b.d["controls"]}
    for i in range(8):
        n = i + 1
        b.center(byface["Clip stop %d" % n], cx[i], 444, 97, 44)
        b.center(byface["Track %d" % n], cx[i], 506, 102, 36)
        b.center(byface["%d" % n], 45 + 114 * i, 563, 40, 38)
        b.center(byface["A|B %d" % n], 105 + 114 * i, 563, 44, 38)
        b.center(byface["S %d" % n], 45 + 114 * i, 614, 40, 36)
        b.center(byface["Arm %d" % n], 105 + 114 * i, 614, 44, 36)
        b.put(byface["Fader %d" % n], 38 + 114 * i, 648, 77, 232)
    b.put(14, 933, 648, 72, 232)
    b.center(47, 971, 585, 66)
    for i, ch in enumerate((215, 216, 217)):
        b.center(ch, 1081, [137, 204, 267][i], 47, 26)
    b.center(219, 1195, 127, 46, 40)
    b.center(221, 1308, 127, 46, 40)
    b.center(230, 1421, 127, 46, 40)
    b.center(218, 1195, 204, 46, 22)
    b.center(227, 1308, 206, 46, 22)
    b.center(228, 1195, 267, 46, 20)
    b.center(229, 1308, 267, 46, 20)
    b.center(13, 1420, 235, 66, kind="knob")
    kx = [1081, 1195, 1309, 1423]
    for i in range(4):
        b.center(16 + i, kx[i], 360, 80)
        b.center(20 + i, kx[i], 477, 80)
    bx = [1081, 1195, 1309, 1423]
    for i, ch in enumerate((186, 187, 188, 189)):
        b.center(ch, bx[i], 563, 46, 20)
    for i, ch in enumerate((190, 191, 192, 193)):
        b.center(ch, bx[i], 628, 46, 20)
    b.center(226, 1308, 691, 46, 18)
    b.center(231, 1422, 691, 46, 18)
    b.put(222, 1085, 683, 65, 27)
    b.put(223, 1085, 745, 65, 27)
    b.put(225, 1060, 712, 24, 32)
    b.put(224, 1151, 712, 25, 32)
    b.put(15, 1216, 790, 196, 80)
    # knobs of the rings are round, the pads are not
    for c in b.d["controls"]:
        if c["kind"] in ("knob", "encoder"): c["shape"] = "round"
    b.section("", 18, 20, 914, 106)
    b.section("Scene launch", 934, 100, 74, 304)
    b.section("Device control", 1033, 296, 437, 372)
    b.section("", 1040, 100, 90, 195)
    b.section("", 1148, 170, 322, 124)
    b.section("", 1050, 676, 132, 108)
    b.section("Crossfader", 1210, 770, 210, 120)
    b.finish()

# ---------------------------------------------------------------------- APC64
def apc64():
    b = Board("Akai-APC64", 128, 2000, 1459)
    byface = {c["face"]: c["ch"] for c in b.d["controls"]}
    # pad faces say their row-column: place by face
    for r in range(1, 9):
        for c in range(1, 9):
            b.put(byface["Pad %d-%d" % (r, c)], 493 + 128 * (c - 1), 166 + 126.5 * (r - 1), 110, 108)
    left = ["Device", "Volume", "Pan", "Send", "Ch. Strip", "Off"]
    for i, f in enumerate(left):
        b.center(byface[f], 410, [187, 249, 312, 375, 437, 500][i], 90, 42)
    for f, y in (("Tempo", 596), ("Clear", 691), ("Duplicate", 760), ("Fixed Length", 840),
                 ("Quantize", 903), ("Undo", 992), ("SHIFT", 1073), ("Rec Arm", 1155),
                 ("Mute", 1218), ("Solo", 1281), ("Clip Stop", 1345)):
        b.center(byface[f], 410, y, 90, 42)
    for i, y in enumerate((218, 345, 472, 600, 727, 855, 982, 1110)):
        f = "Scene %d" % (i + 1) if i < 7 else "STOP ALL CLIPS"
        b.center(byface[f], 1583, y, 90, 52)
    sx = [98, 262, 1732, 1897]
    pos = {1: (98, 190), 2: (262, 190), 3: (98, 715), 4: (262, 715),
           5: (1732, 190), 6: (1897, 190), 7: (1732, 715), 8: (1897, 715)}
    for n, (x, y) in pos.items():
        b.put(byface["Strip %d" % n], x - 50, y, 100, 420)
    for i in range(8):
        n = i + 1
        b.center(byface["State %d" % n], 548 + 128 * i, 1234, 110, 48)
        b.center(byface["Track %d" % n], 548 + 128 * i, 1330, 110, 75)
    b.center(byface["PLAY"], 1242, 92, 88, 64)
    b.center(byface["STOP"], 1347, 92, 88, 64)
    b.center(byface["RECORD"], 1454, 92, 88, 64)
    b.center(byface["Encoder (push)"], 1846, 1290, 96, shape="round", kind="encoder")
    b.put(byface["▲"], 130, 1208, 98, 76)
    b.put(byface["▼"], 130, 1292, 98, 76)
    b.put(byface["◀"], 50, 1208, 76, 158)
    b.put(byface["▶"], 232, 1208, 76, 158)
    for c in b.d["controls"]:
        if c["kind"] in ("encoder", "knob"):
            c["shape"] = "round"
    b.section("", 40, 160, 285, 470)
    b.section("", 40, 685, 285, 470)
    b.section("", 1675, 160, 285, 470)
    b.section("", 1675, 685, 285, 470)
    b.section("", 480, 155, 1030, 1020)
    b.section("Track select", 480, 1180, 1030, 205)
    b.section("Screen", 1538, 1215, 418, 158)
    b.finish()

if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    faderport(); pmj(); apc40(); apc64()
