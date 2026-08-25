#!/usr/bin/env python3
#
#  Q Light Controller Plus - qlcconsole
#  artnet-probe.py
#
#  Standalone Art-Net node probe: ArtPoll (who is out there?) and the RDM
#  discovery handshake (ArtTodControl/AtcFlush + ArtTodRequest -> ArtTodData).
#
#  Written to answer "does this node actually do RDM?" without building
#  anything into the app first, and to be re-runnable when node hardware
#  changes. See the RDM item in TODO.md.
#
#  Replies arrive on UDP 6454, which a running qlcconsole already holds, so
#  the receive side uses SO_REUSEPORT where available and otherwise tells you
#  to quit the app. Nodes commonly BROADCAST their replies rather than
#  unicasting them back to the requester, so we must listen on 6454, not on
#  the ephemeral source port.
#
#  Usage:  ./artnet-probe.py [target-ip] [--universes N] [--wait SEC]
#          ./artnet-probe.py 172.18.2.10
#          ./artnet-probe.py 2.255.255.255      # broadcast discovery
#
import argparse
import socket
import struct
import sys
import time

ARTNET_PORT = 6454
HDR = b"Art-Net\x00"

# Opcodes -- these MUST match plugins/artnet/src/artnetpacketizer.h
OP_POLL = 0x2000
OP_POLLREPLY = 0x2100
OP_TODREQUEST = 0x8000
OP_TODDATA = 0x8100
OP_TODCONTROL = 0x8200
OP_RDM = 0x8300

OPNAMES = {
    OP_POLL: "ArtPoll", OP_POLLREPLY: "ArtPollReply",
    OP_TODREQUEST: "ArtTodRequest", OP_TODDATA: "ArtTodData",
    OP_TODCONTROL: "ArtTodControl", OP_RDM: "ArtRdm",
}


def cstr(b):
    return b.split(b"\x00")[0].decode("ascii", "replace")


def artnet_hdr(opcode):
    """8-byte ID + little-endian opcode + ProtVer 14 (big-endian)."""
    return HDR + struct.pack("<H", opcode) + bytes([0, 14])


def tod_request(net, universes):
    """ArtTodRequest, 56 bytes. Asks a node for its current Table Of Devices."""
    addr = bytes(universes[:32]) + bytes(32 - len(universes[:32]))
    return (artnet_hdr(OP_TODREQUEST)
            + bytes(2)                    # Filler1-2
            + bytes(7)                    # Spare1-7
            + bytes([net])                # Net
            + bytes([0])                  # Command: 0 = TodFull
            + bytes([len(universes[:32])])  # AddCount
            + addr)


def tod_control(net, universe, command=1):
    """ArtTodControl, 24 bytes. Command 1 = AtcFlush: force a re-discovery.

    This is the step that matters: ArtTodRequest only asks for the TOD the
    node already has. A node that has never run discovery can legitimately
    answer with an empty TOD (or, less politely, not at all) until flushed.
    """
    return (artnet_hdr(OP_TODCONTROL)
            + bytes(2)                    # Filler1-2
            + bytes(7)                    # Spare1-7
            + bytes([net])                # Net
            + bytes([command])            # Command: 1 = AtcFlush
            + bytes([universe]))          # Address (low byte of port address)


def decode_pollreply(a, src):
    print(f"\n<-- ArtPollReply from {src}  ({len(a)} bytes)")
    print(f"    node IP    : {'.'.join(str(b) for b in a[10:14])}")
    print(f"    shortName  : {cstr(a[26:44])!r}")
    print(f"    longName   : {cstr(a[44:108])!r}")
    print(f"    nodeReport : {cstr(a[108:172])!r}")
    print(f"    versInfo   : {struct.unpack('>H', a[16:18])[0]}"
          f"  OEM=0x{struct.unpack('>H', a[20:22])[0]:04X}"
          f"  ESTA=0x{struct.unpack('<H', a[24:26])[0]:04X}")
    nports = struct.unpack(">H", a[172:174])[0]
    status2 = a[212] if len(a) > 212 else 0
    print(f"    net/sub    : {a[18]}/{a[19]}   numPorts={nports}")
    print(f"    status1    : 0x{a[23]:02X}   status2: 0x{status2:02X}")
    for i in range(min(nports, 4)):
        pt, go = a[174 + i], a[182 + i]
        # GoodOutput bit 3 set == "RDM is DISABLED" on this port.
        print(f"      port{i}: type=0x{pt:02X} out={bool(pt & 0x80)} "
              f"in={bool(pt & 0x40)} proto={pt & 0x3f} | goodOut=0x{go:02X} "
              f"rdm_disabled={bool(go & 0x08)} | swOut={a[190 + i]}")


def decode_toddata(a, src):
    rdm_ver = a[12]
    uid_total = struct.unpack(">H", a[24:26])[0]
    uid_count = a[27]
    print(f"\n<-- ArtTodData from {src}: rdmVer=0x{rdm_ver:02X} port={a[13]} "
          f"net={a[21]} cmdRes={a[22]} address={a[23]} "
          f"uidTotal={uid_total} uidCount={uid_count}")
    if uid_count == 0:
        print("      TOD is EMPTY -- node implements RDM but found no devices "
              "on this port.")
    for i in range(uid_count):
        u = a[28 + i * 6:34 + i * 6]
        if len(u) == 6:
            print(f"      UID {u[0]:02X}{u[1]:02X}:"
                  f"{u[2]:02X}{u[3]:02X}{u[4]:02X}{u[5]:02X}"
                  f"   (ESTA 0x{u[0] << 8 | u[1]:04X})")


def main():
    ap = argparse.ArgumentParser(description="Art-Net node / RDM probe")
    ap.add_argument("target", nargs="?", default="172.18.2.10")
    ap.add_argument("--net", type=int, default=0, help="Art-Net Net (default 0)")
    ap.add_argument("--universes", type=int, default=4,
                    help="how many universes/ports to ask about (default 4)")
    ap.add_argument("--port", type=int, default=ARTNET_PORT,
                    help="destination UDP port (default 6454)")
    ap.add_argument("--listen-port", type=int, default=None,
                    help="local port to receive on. Defaults to 6454 because "
                         "real nodes reply there regardless of source port. "
                         "Use 0 (ephemeral) when probing a simulator that "
                         "replies to the sender.")
    ap.add_argument("--wait", type=float, default=6.0,
                    help="seconds to listen after the TOD requests")
    args = ap.parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        try:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except OSError:
            pass
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    try:
        lp = args.listen_port if args.listen_port is not None else ARTNET_PORT
        s.bind(("0.0.0.0", lp))
    except OSError as e:
        print(f"cannot bind UDP {ARTNET_PORT}: {e}\n"
              f"A running qlcconsole holds this port. Quit it and retry "
              f"(replies come back to 6454, so an ephemeral port will miss "
              f"them).", file=sys.stderr)
        return 2
    s.settimeout(1.0)

    unis = list(range(args.universes))
    target = (args.target, args.port)

    print(f"--> ArtPoll to {args.target}")
    s.sendto(artnet_hdr(OP_POLL) + bytes([0x02, 0x00]), target)
    time.sleep(1.5)

    print(f"--> ArtTodControl/AtcFlush, universes {unis} (forces re-discovery)")
    for u in unis:
        s.sendto(tod_control(args.net, u), target)
    time.sleep(2.0)

    print(f"--> ArtTodRequest, universes {unis}")
    s.sendto(tod_request(args.net, unis), target)

    seen_poll, n_tod = set(), 0
    deadline = time.time() + args.wait
    while time.time() < deadline:
        try:
            data, addr = s.recvfrom(2048)
        except socket.timeout:
            continue
        if not data.startswith(HDR) or len(data) < 12:
            continue
        op = struct.unpack("<H", data[8:10])[0]
        if op == OP_POLLREPLY and len(data) >= 200:
            if addr[0] in seen_poll:
                continue
            seen_poll.add(addr[0])
            decode_pollreply(data, addr[0])
        elif op == OP_TODDATA and len(data) >= 28:
            n_tod += 1
            decode_toddata(data, addr[0])
        elif op == OP_RDM:
            print(f"\n<-- ArtRdm from {addr[0]} ({len(data)} bytes)")
        elif op not in (OP_POLL, OP_TODREQUEST, OP_TODCONTROL, 0x5000):
            print(f"\n<-- {OPNAMES.get(op, hex(op))} from {addr[0]}")

    print()
    if not seen_poll:
        print("RESULT: no ArtPollReply -- node not reachable, or not Art-Net.")
    elif n_tod == 0:
        print("RESULT: node answers ArtPoll but NOT ArtTodRequest/ArtTodControl.")
        print("        => this node does not implement Art-Net RDM.")
    else:
        print(f"RESULT: node implements Art-Net RDM ({n_tod} ArtTodData reply/ies).")
    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
