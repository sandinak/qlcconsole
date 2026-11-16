#!/usr/bin/env python3
"""Writes the synthetic starter traces in traces/ (see README.md).

They copy what the bench taught us until a real recording replaces them
(docs/dev/PAD_TRACES.md). Channels are the Xbox/PlayStation profile's:
axes lx 0, ly 1, lt 2, rx 3, ry 4, rt 5; buttons a 6, b 7, x 8, y 9, lb 10,
rb 11, back/Create 12, start/Options 13. Times are ms.
"""
import json
import os
import random

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "traces")
CH = dict(lx=0, ly=1, lt=2, rx=3, ry=4, rt=5, a=6, b=7, x=8, y=9, lb=10, rb=11,
          create=12, options=13)

DS_PATH = "USB_054c_0ce6_2124000"
DS_NAME = "DualSense Wireless Controller"
XB_PATH = "USB_045e_028e_2124000"
XB_NAME = "Xbox 360 Controller for Windows"
LOC = "USB 2124000"


class Trace:
    def __init__(self, header):
        self.lines = ["# " + h for h in header]

    def add(self, **kw):
        self.lines.append(json.dumps(kw, separators=(",", ":")))

    def arrive(self, t, path, vid, pid, name, serial=""):
        self.add(t=t, ev="arrived", path=path, vid=vid, pid=pid, name=name,
                 serial=serial, location=LOC, identity="port:HID:" + LOC,
                 axes=6, buttons=22)

    def gone(self, t, path, name):
        self.add(t=t, ev="gone", path=path, name=name)

    def inp(self, t, path, role, value):
        self.add(t=t, ev="input", path=path, ch=CH[role], value=value)

    def save(self, name):
        head = [l for l in self.lines if l.startswith("#")]
        body = [l for l in self.lines if not l.startswith("#")]
        body.sort(key=lambda l: json.loads(l)["t"])    # stable: ties keep their order
        self.lines = head + body
        with open(os.path.join(OUT, name), "w") as f:
            f.write("\n".join(self.lines) + "\n")


def settle_sticks(tr, path, t0, t1, step=100):
    rnd = random.Random(7)
    for t in range(t0, t1, step):
        for role in ("lx", "ly", "rx", "ry"):
            tr.inp(t, path, role, 128 + rnd.choice((-1, 0, 0, 1)))


def arm(tr, path, t):
    tr.inp(t, path, "lb", 255)
    tr.inp(t + 60, path, "rb", 255)
    tr.inp(t + 700, path, "rb", 0)
    tr.inp(t + 720, path, "lb", 0)
    return t + 720


def go(tr, path, t):
    tr.inp(t, path, "lt", 255)
    tr.inp(t + 80, path, "a", 255)
    tr.inp(t + 200, path, "a", 0)
    tr.inp(t + 260, path, "lt", 0)
    return t + 260


def clone_flip():
    tr = Trace(["SYNTHETIC (written by make_synthetic_traces.py, not recorded).",
                "A cabled DualSense clone: armed, GO, then Create + Options held",
                "~3 s flips it to an Xbox 360 pad: it leaves USB and comes back",
                "as three 045e:028e arrivals at one port within ~1 s; the first two leave again."])
    tr.arrive(0, DS_PATH, "054c", "0ce6", DS_NAME, "SYN0001")
    t = arm(tr, DS_PATH, 600)
    t = go(tr, DS_PATH, t + 600)
    t += 700
    tr.inp(t, DS_PATH, "create", 255)
    tr.inp(t + 40, DS_PATH, "options", 255)
    t += 3000
    tr.gone(t + 100, DS_PATH, DS_NAME)
    tr.arrive(t + 400, XB_PATH, "045e", "028e", XB_NAME)
    tr.gone(t + 700, XB_PATH, XB_NAME)
    tr.arrive(t + 900, XB_PATH, "045e", "028e", XB_NAME)
    tr.gone(t + 1200, XB_PATH, XB_NAME)
    tr.arrive(t + 1400, XB_PATH, "045e", "028e", XB_NAME)
    # The pad that stayed is pressed on: a GO attempt that the arm must not allow.
    go(tr, XB_PATH, t + 2400)
    tr.save("clone_flip_to_xbox.jsonl")


def twin_listing():
    tr = Trace(["SYNTHETIC. One DualSense that the system lists twice with the",
                "same identity (macOS does). Armed, GO, then the emergency stop",
                "(LB + RB + LT + RT): one pad, one GO, one stop."])
    tr.arrive(0, DS_PATH, "054c", "0ce6", DS_NAME, "SYN0002")
    tr.arrive(60, DS_PATH, "054c", "0ce6", DS_NAME, "SYN0002")
    t = arm(tr, DS_PATH, 500)
    t = go(tr, DS_PATH, t + 600)
    t += 600
    for i, role in enumerate(("lb", "rb", "lt", "rt")):
        tr.inp(t + i * 60, DS_PATH, role, 255)
    for i, role in enumerate(("rt", "lt", "rb", "lb")):
        tr.inp(t + 800 + i * 40, DS_PATH, role, 0)
    tr.save("twin_listing.jsonl")


def trigger_rest(name, quiet, header):
    tr = Trace(header)
    tr.arrive(0, DS_PATH, "054c", "0ce6", DS_NAME, "SYN0003")
    rnd = random.Random(11)
    for t in range(0, 4500, 100):
        if quiet and 760 <= t <= 1540:
            continue    # a pad sends what changed: the triggers hold still while the chord is held
        tr.inp(t + 5, DS_PATH, "lt", 20 + rnd.choice((-1, 0, 0, 1)))
        tr.inp(t + 7, DS_PATH, "rt", 20 + rnd.choice((-1, 0, 0, 1)))
    settle_sticks(tr, DS_PATH, 10, 4500)
    arm(tr, DS_PATH, 800)
    for t in (2200, 3000, 3800):
        tr.inp(t, DS_PATH, "a", 255)
        tr.inp(t + 150, DS_PATH, "a", 0)
    tr.save(name)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    clone_flip()
    twin_listing()
    trigger_rest("trigger_rest.jsonl", True,
                 ["SYNTHETIC. Both triggers rest at ~8 % (20 of 255) and the sticks",
                  "jitter around centre (the triggers hold still for the arm chord). Armed",
                  "by the chord, A pressed with no LT held: the resting trigger is not the",
                  "safety, so no GO; no hit."])
    trigger_rest("trigger_rest_in_chord.jsonl", False,
                 ["SYNTHETIC. As trigger_rest.jsonl, but the resting triggers keep",
                  "reporting while LB + RB are held (the arm chord)."])
