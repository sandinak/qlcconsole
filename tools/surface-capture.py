#!/usr/bin/env python3
"""Record a control surface or a game pad on the bench, for the surface
simulator to replay (docs/dev/SURFACE_TESTS.md, "Recording a device").

Offline only: it reads the device and writes a .jsonl file; it sends
nothing to the device and needs no qlcconsole running (quit qlcconsole
first, or the system may give the device to it).

    tools/surface-capture.py list
    tools/surface-capture.py midi --port "PMJ Black 1" -o pmj-go.jsonl
    tools/surface-capture.py hid --vid 054c --pid 0ce6 -o dualsense-r2.jsonl
    tools/surface-capture.py selftest

Each line is what the simulator's SurfaceScript plays:
    {"t": 96, "ev": "raw", "bytes": "88 0b 00"}
t is milliseconds since the first message. Ctrl-C ends the capture.

MIDI needs `pip install mido python-rtmidi`; HID needs `pip install hidapi`.
"""

import argparse
import json
import sys
import time


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
    sub.add_parser("selftest", help="check the format without a device")
    args = ap.parse_args()
    return {"list": cmd_list, "midi": cmd_midi, "hid": cmd_hid, "selftest": cmd_selftest}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
