#!/usr/bin/env python3
"""Record a control surface or a game pad on the bench, for the surface
simulator to replay (docs/dev/SURFACE_TESTS.md, "Recording a device").

Offline only: it reads the device and writes a .jsonl file; it sends
nothing to the device and needs no qlcconsole running (quit qlcconsole
first, or the system may give the device to it).

    tools/surface-capture.py list
    tools/surface-capture.py midi --port "PMJ Black 1" -o pmj-go.jsonl
    tools/surface-capture.py press --port "OpenDeck | PMJ_BLACK_1" \
        --profile resources/inputprofiles/PMJ-Black-1.qxi -o pmj-every.jsonl
    tools/surface-capture.py hid --vid 054c --pid 0ce6 -o dualsense-r2.jsonl
    tools/surface-capture.py selftest

Each line is what the simulator's SurfaceScript plays:
    {"t": 96, "ev": "raw", "bytes": "88 0b 00"}
t is milliseconds since the first message. Ctrl-C ends the capture.

MIDI needs `pip install mido python-rtmidi`; HID needs `pip install hidapi`.
"""

import argparse
import json
import select
import sys
import time
import xml.etree.ElementTree as ET


def line(t0, data, out):
    t = int((time.monotonic() - t0) * 1000) if t0 is not None else 0
    rec = {"t": t, "ev": "raw", "bytes": " ".join("%02x" % b for b in data)}
    out.write(json.dumps(rec) + "\n")
    out.flush()
    return rec


def header(out, what):
    out.write("# tools/surface-capture.py %s (format: docs/dev/SURFACE_TESTS.md)\n" % what)
    out.write("# captured %s\n" % time.strftime("%Y-%m-%d %H:%M"))


def cmd_list(_args):
    try:
        import mido
        print("MIDI inputs:")
        for name in mido.get_input_names():
            print("  " + name)
    except ImportError:
        print("MIDI: pip install mido python-rtmidi")
    try:
        import hid
        print("HID devices (vid:pid product):")
        seen = set()
        for d in hid.enumerate():
            key = (d["vendor_id"], d["product_id"])
            if key in seen:
                continue
            seen.add(key)
            print("  %04x:%04x %s" % (d["vendor_id"], d["product_id"], d.get("product_string") or ""))
    except ImportError:
        print("HID: pip install hidapi")
    return 0


def cmd_midi(args):
    try:
        import mido
    except ImportError:
        sys.exit("needs mido: pip install mido python-rtmidi")
    out = open(args.output, "w") if args.output else sys.stdout
    header(out, "midi --port %r" % args.port)
    t0 = None
    n = 0
    with mido.open_input(args.port) as port:
        print("recording %s; Ctrl-C to stop" % args.port, file=sys.stderr)
        try:
            for msg in port:
                if msg.type == "clock" and not args.clock:
                    continue
                if t0 is None:
                    t0 = time.monotonic()
                line(t0, msg.bytes(), out)
                n += 1
        except KeyboardInterrupt:
            pass
    print("%d messages" % n, file=sys.stderr)
    return 0


def cmd_hid(args):
    try:
        import hid
    except ImportError:
        sys.exit("needs hidapi: pip install hidapi")
    dev = hid.device()
    dev.open(int(args.vid, 16), int(args.pid, 16))
    dev.set_nonblocking(False)
    out = open(args.output, "w") if args.output else sys.stdout
    header(out, "hid --vid %s --pid %s" % (args.vid, args.pid))
    t0 = None
    last = None
    n = 0
    print("recording %s:%s; Ctrl-C to stop" % (args.vid, args.pid), file=sys.stderr)
    try:
        while True:
            report = dev.read(128, 1000)
            if not report:
                continue
            # A pad sends its state hundreds of times a second: keep a report
            # only when something in it but the counter and the motion
            # sensors changed (DualSense: bytes 1-10).
            key = bytes(report[:11]) if not args.all else bytes(report)
            if key == last:
                continue
            last = key
            if t0 is None:
                t0 = time.monotonic()
            line(t0, report, out)
            n += 1
    except KeyboardInterrupt:
        pass
    print("%d reports" % n, file=sys.stderr)
    return 0


# ---- press every control: the profile against the board ----------------

def describe(channel):
    """A QLC+ MIDI channel number as words: 'note 0 ch 9', 'CC 3 ch 9'.
    Bits 12-15 are the MIDI channel when the profile was learned with the
    input on any channel (plugins/midi/src/common/midiprotocol.h)."""
    ch = (channel >> 12) & 0x0F
    c = channel & 0x0FFF
    if c <= 127:
        what = "CC %d" % c
    elif c <= 255:
        what = "note %d" % (c - 128)
    elif c <= 383:
        what = "aftertouch %d" % (c - 256)
    elif c <= 511:
        what = "program %d" % (c - 384)
    else:
        what = "channel %d" % c
    return "%s ch %d" % (what, ch + 1)


def qlc_channel(data):
    """The QLC+ channel a MIDI message is heard on, the input on any
    channel (QLCMIDIProtocol::midiToInput in OMNI mode); None: not one."""
    if not data:
        return None
    status = data[0]
    kind, ch = status & 0xF0, status & 0x0F
    if kind in (0x80, 0x90) and len(data) >= 2:
        c = 128 + data[1]
    elif kind == 0xB0 and len(data) >= 2:
        c = data[1]
    elif kind == 0xA0 and len(data) >= 2:
        c = 256 + data[1]
    elif kind == 0xC0 and len(data) >= 2:
        c = 384 + data[1]
    else:
        return None
    return (ch << 12) | c


def profile_controls(path):
    """[(channel, name, type)] of a .qxi input profile, in its order."""
    root = ET.parse(path).getroot()
    out = []
    for el in root.iter():
        if el.tag.split("}")[-1] != "Channel" or "Number" not in el.attrib:
            continue
        name, typ = "", ""
        for sub in el:
            tag = sub.tag.split("}")[-1]
            if tag == "Name":
                name = sub.text or ""
            elif tag == "Type":
                typ = sub.text or ""
        out.append((int(el.attrib["Number"]), name, typ))
    return out


def compare(controls, heard):
    """Rows (name, profile, received, ok) for each control; heard maps a
    control's name to the QLC+ channel its press arrived on (None: skipped)."""
    rows = []
    for channel, name, _typ in controls:
        got = heard.get(name)
        if got is None:
            rows.append((name, describe(channel), "(skipped)", None))
        else:
            rows.append((name, describe(channel), describe(got), got == channel))
    return rows


def print_table(rows, out=sys.stdout):
    width = max([len(r[0]) for r in rows] + [7])
    out.write("%-*s  %-16s  %-16s  %s\n" % (width, "Control", "Profile", "Received", ""))
    bad = 0
    for name, prof, got, ok in rows:
        mark = "" if ok is None else ("ok" if ok else "<<< MISMATCH")
        if ok is False:
            bad += 1
        out.write("%-*s  %-16s  %-16s  %s\n" % (width, name, prof, got, mark))
    out.write("%d controls, %d mismatched, %d skipped\n"
              % (len(rows), bad, sum(1 for r in rows if r[3] is None)))
    return bad


def cmd_press(args):
    try:
        import mido
    except ImportError:
        sys.exit("needs mido: pip install mido python-rtmidi")
    controls = profile_controls(args.profile)
    if not controls:
        sys.exit("no controls in %s" % args.profile)
    out = open(args.output, "w") if args.output else None
    if out:
        header(out, "press --port %r --profile %r" % (args.port, args.profile))
    heard = {}
    t0 = None
    with mido.open_input(args.port) as port:
        for i, (channel, name, typ) in enumerate(controls):
            verb = "move" if typ in ("Slider", "Knob", "Encoder") else "press"
            sys.stderr.write("[%d/%d] %s %s (profile: %s) -- Enter skips, q quits: "
                             % (i + 1, len(controls), verb, name, describe(channel)))
            sys.stderr.flush()
            for _ in port.iter_pending():
                pass                    # what came before the prompt is not this control
            got, until = None, None
            while True:
                msg = port.poll()
                if msg is not None and msg.type != "clock":
                    if t0 is None:
                        t0 = time.monotonic()
                    if out:
                        if got is None:
                            out.write("# %s\n" % name)
                        line(t0, msg.bytes(), out)
                    if got is None:
                        got = qlc_channel(msg.bytes())
                        until = time.monotonic() + 0.4     # its release, then the next
                if until is not None and time.monotonic() > until:
                    break
                r, _, _ = select.select([sys.stdin], [], [], 0.01)
                if r:
                    answer = sys.stdin.readline().strip().lower()
                    if answer == "q":
                        sys.stderr.write("\n")
                        bad = print_table(compare(controls, heard))
                        return 1 if bad else 0
                    if got is None:
                        break
            heard[name] = got
            sys.stderr.write("%s\n" % ("skipped" if got is None else describe(got)))
    bad = print_table(compare(controls, heard))
    if out:
        print("wrote %s: replay it with SurfaceScript (docs/dev/SURFACE_TESTS.md)" % args.output, file=sys.stderr)
    return 1 if bad else 0


def cmd_selftest(_args):
    """The format, without a device: what the simulator's parser reads."""
    import io
    buf = io.StringIO()
    header(buf, "selftest")
    first = line(None, [0x98, 0x00, 0x7F], buf)
    assert first == {"t": 0, "ev": "raw", "bytes": "98 00 7f"}, first
    for text in buf.getvalue().splitlines():
        if text.startswith("#"):
            continue
        rec = json.loads(text)
        assert set(rec) == {"t", "ev", "bytes"}
        assert bytes.fromhex(rec["bytes"].replace(" ", "")) == b"\x98\x00\x7f"
    # press: the PMJ's Go is note 0 on MIDI channel 9, QLC+ channel 32896.
    assert qlc_channel([0x98, 0x00, 0x7F]) == 32896
    assert describe(32896) == "note 0 ch 9", describe(32896)
    assert describe(32768) == "CC 0 ch 9"
    rows = compare([(32896, "Go", "Button"), (32907, "Back", "Button"), (32946, "Effects", "Button")],
                   {"Go": 32896, "Back": 128 + 11, "Effects": None})
    assert [r[3] for r in rows] == [True, False, None], rows
    out = io.StringIO()
    assert print_table(rows, out) == 1 and "MISMATCH" in out.getvalue()
    print("ok")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list", help="the MIDI inputs and HID devices this machine sees")
    m = sub.add_parser("midi", help="record a MIDI input")
    m.add_argument("--port", required=True, help="the input's name, as `list` shows it")
    m.add_argument("--clock", action="store_true", help="keep MIDI clock messages")
    m.add_argument("-o", "--output", help="the .jsonl to write (default: stdout)")
    h = sub.add_parser("hid", help="record a HID device's input reports")
    h.add_argument("--vid", required=True, help="vendor id, hex (054c: Sony, 045e: Microsoft)")
    h.add_argument("--pid", required=True, help="product id, hex (0ce6: DualSense)")
    h.add_argument("--all", action="store_true", help="keep every report, not only the changed ones")
    h.add_argument("-o", "--output", help="the .jsonl to write (default: stdout)")
    pr = sub.add_parser("press", help="prompt for every control of a profile in turn and compare "
                                      "what the board sends with what the profile says")
    pr.add_argument("--port", required=True, help="the input's name, as `list` shows it")
    pr.add_argument("--profile", required=True, help="the .qxi input profile")
    pr.add_argument("-o", "--output", help="also write every message as a .jsonl the simulator replays")
    sub.add_parser("selftest", help="check the format without a device")
    args = ap.parse_args()
    return {"list": cmd_list, "midi": cmd_midi, "hid": cmd_hid, "press": cmd_press,
            "selftest": cmd_selftest}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
