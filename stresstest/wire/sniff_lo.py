#!/usr/bin/env python3
"""Capture UDP on Linux loopback to a pcap file (stdlib only, AF_PACKET; run
as root, e.g. inside `unshare -n`). Only UDP to or from the given ports is
kept. Stops on SIGTERM/SIGINT.

  sniff_lo.py OUT.pcap [--iface lo] [--ports 6454,7700-7799]
"""
import argparse
import signal
import socket
import struct
import sys
import time

ap = argparse.ArgumentParser()
ap.add_argument("out")
ap.add_argument("--iface", default="lo")
ap.add_argument("--ports", default="6454")
a = ap.parse_args()
ports = set()
for part in a.ports.split(","):
    if "-" in part:
        lo, hi = part.split("-")
        ports.update(range(int(lo), int(hi) + 1))
    else:
        ports.add(int(part))

ETH_P_ALL = 0x0003
s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 64 * 1024 * 1024)
s.bind((a.iface, 0))
s.settimeout(0.5)
f = open(a.out, "wb", buffering=1024 * 1024)
f.write(struct.pack("<IHHiIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))   # LINKTYPE_ETHERNET
stop = [False]


def handler(*_):
    stop[0] = True


signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGINT, handler)
n = 0
while not stop[0]:
    try:
        pkt, addr = s.recvfrom(65535)
    except socket.timeout:
        continue
    except InterruptedError:
        continue
    # on lo every packet is seen twice (outgoing + incoming): keep incoming only
    if addr[2] == socket.PACKET_OUTGOING:
        continue
    if len(pkt) < 14 + 20 + 8 or pkt[12:14] != b"\x08\x00" or pkt[23] != 17:
        continue
    ihl = (pkt[14] & 0x0F) * 4
    sport, dport = struct.unpack("!HH", pkt[14 + ihl:14 + ihl + 4])
    if dport not in ports and sport not in ports:
        continue
    t = time.time()
    sec = int(t)
    f.write(struct.pack("<IIII", sec, int((t - sec) * 1e6), len(pkt), len(pkt)))
    f.write(pkt)
    n += 1
f.close()
print("sniff_lo: %d packets -> %s" % (n, a.out))
