#!/usr/bin/env python3
#
#  Q Light Controller Plus - qlcconsole
#  artnet-config.py
#
#  Read and WRITE Art-Net node configuration (ArtAddress, OpCode 0x6000) --
#  the DMX-Workshop "configure the node" capability, prototyped standalone so
#  the packet format can be proven against real hardware before any of it goes
#  into the plugin.
#
#  ArtAddress writes to hardware. Port-address changes silently break output
#  on a live rig, so every field defaults to "no change" and you must ask for
#  each change explicitly.
#
#  Per Art-Net 4, NetSwitch / SubSwitch / SwIn / SwOut are only acted on when
#  bit 7 is high: to program value 7 you send 0x87. 0x00 means "reset to zero",
#  and 0x7f is the conventional "leave alone" -- which is why 0x7f, not 0x00,
#  is the default here.
#
#  Usage:
#    ./artnet-config.py 172.18.2.10 --show
#    ./artnet-config.py 172.18.2.10 --locate      # blink the node's LEDs
#    ./artnet-config.py 172.18.2.10 --led-normal
#    ./artnet-config.py 172.18.2.10 --short NAME --long "Longer Name"
#    ./artnet-config.py 172.18.2.10 --swout 0,1,2,3
#
import argparse, socket, struct, sys, time

PORT = 6454
HDR = b"Art-Net\x00"
OP_POLL, OP_POLLREPLY, OP_ADDRESS = 0x2000, 0x2100, 0x6000

NOCHANGE = 0x7F          # bit 7 low and non-zero => field ignored by the node
PROGRAM  = 0x80          # OR this in to actually set a value

CMD = {
    "none": 0x00, "cancel-merge": 0x01, "led-normal": 0x02,
    "led-mute": 0x03, "locate": 0x04, "reset-rx-flags": 0x05,
}


def cstr(b):
    return b.split(b"\x00")[0].decode("ascii", "replace")


def sock():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        try: s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except OSError: pass
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("0.0.0.0", PORT))
    s.settimeout(1.0)
    return s


def poll(s, target, wait=3.0):
    """Return the node's current ArtPollReply, or None."""
    s.sendto(HDR + struct.pack("<H", OP_POLL) + bytes([0, 14, 0x02, 0x00]), (target, PORT))
    t = time.time()
    while time.time() - t < wait:
        try: d, a = s.recvfrom(2048)
        except socket.timeout: continue
        if (d.startswith(HDR) and len(d) >= 214
                and struct.unpack("<H", d[8:10])[0] == OP_POLLREPLY
                and a[0] == target):
            return d
    return None


def describe(d):
    n = struct.unpack(">H", d[172:174])[0]
    print(f"  short/long : {cstr(d[26:44])!r} / {cstr(d[44:108])!r}")
    print(f"  report     : {cstr(d[108:172])!r}")
    print(f"  firmware   : {struct.unpack('>H', d[16:18])[0]}"
          f"   OEM 0x{struct.unpack('>H', d[20:22])[0]:04X}"
          f"   ESTA 0x{struct.unpack('<H', d[24:26])[0]:04X}")
    print(f"  net/sub    : {d[18]}/{d[19]}    ports: {n}")
    print(f"  status1    : 0x{d[23]:02X} (rdm_capable={bool(d[23] & 0x02)})"
          f"   status2: 0x{d[212]:02X}")
    for i in range(min(n, 4)):
        print(f"    port{i}: type=0x{d[174+i]:02X} swIn={d[186+i]} swOut={d[190+i]}"
              f" goodOut=0x{d[182+i]:02X}")


def art_address(short=None, long_=None, swout=None, swin=None,
                net=None, sub=None, command=0x00, bindindex=0):
    p = bytearray(107)
    p[0:8] = HDR
    p[8:10] = struct.pack("<H", OP_ADDRESS)
    p[10], p[11] = 0, 14
    p[12] = (PROGRAM | (net & 0x7F)) if net is not None else 0x00   # NetSwitch
    p[13] = bindindex
    # An all-zero name field means "no change"; a non-empty one is programmed.
    if short is not None:
        p[14:32] = short.encode()[:17].ljust(18, b"\x00")
    if long_ is not None:
        p[32:96] = long_.encode()[:63].ljust(64, b"\x00")
    for i in range(4):
        p[96 + i] = (PROGRAM | (swin[i] & 0x0F)) if swin else NOCHANGE
        p[100 + i] = (PROGRAM | (swout[i] & 0x0F)) if swout else NOCHANGE
    p[104] = (PROGRAM | (sub & 0x0F)) if sub is not None else 0x00  # SubSwitch
    p[105] = 0                                                     # AcnPriority
    p[106] = command
    return bytes(p)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target")
    ap.add_argument("--show", action="store_true")
    ap.add_argument("--short"); ap.add_argument("--long")
    ap.add_argument("--swout", help="comma list of 4 universes, e.g. 0,1,2,3")
    ap.add_argument("--net", type=int); ap.add_argument("--sub", type=int)
    for c in CMD:
        ap.add_argument(f"--{c}", action="store_true")
    a = ap.parse_args()

    s = sock()
    before = poll(s, a.target)
    if before is None:
        print(f"no ArtPollReply from {a.target}", file=sys.stderr); return 2
    print(f"=== {a.target} BEFORE ==="); describe(before)

    cmd = 0x00
    for c, v in CMD.items():
        if getattr(a, c.replace("-", "_")):
            cmd = v
    swout = [int(x) for x in a.swout.split(",")] if a.swout else None
    changing = any([a.short, a.long, swout, a.net is not None,
                    a.sub is not None, cmd != 0x00])
    if a.show or not changing:
        return 0

    pkt = art_address(a.short, a.long, swout, None, a.net, a.sub, cmd)
    print(f"\n--> ArtAddress ({len(pkt)} bytes) cmd=0x{cmd:02X}")
    s.sendto(pkt, (a.target, PORT))
    time.sleep(2.0)

    after = poll(s, a.target)
    if after is None:
        print("no reply after the write (node may be rebooting)"); return 1
    print(f"\n=== {a.target} AFTER ==="); describe(after)
    # NodeReport carries a rolling counter that ticks on every reply, so a raw
    # byte compare always says "changed". Compare only the configurable fields.
    def cfg(d):
        return (d[26:44], d[44:108], d[18], d[19], d[186:190], d[190:194])
    ca, cb = cfg(after), cfg(before)
    names = ["shortName", "longName", "net", "sub", "swIn", "swOut"]
    diffs = [f"{n}: {b!r} -> {a!r}" for n, b, a in zip(names, cb, ca) if a != b]
    if diffs:
        print("\nCONFIG CHANGED:")
        for d in diffs: print("   ", d)
    else:
        print("\nCONFIG UNCHANGED (node acknowledged but did not apply)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
