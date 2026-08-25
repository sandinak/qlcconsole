#!/usr/bin/env python3
"""Minimal Art-Net node that speaks RDM, for developing RDM tooling with no rig.

Answers ArtPoll, ArtTodRequest/ArtTodControl (returns a fake Table Of Devices)
and ArtRdm GET for the PIDs the device table needs. Enough to build and verify
discovery + a device table against, entirely offline.

  ./rdm-sim.py [--bind 0.0.0.0] [--devices 3]
"""
import argparse, socket, struct, sys, time

PORT = 6454
HDR = b"Art-Net\x00"
OP_POLL, OP_POLLREPLY = 0x2000, 0x2100
OP_TODREQUEST, OP_TODDATA, OP_TODCONTROL, OP_RDM = 0x8000, 0x8100, 0x8200, 0x8300

PID_DEVICE_INFO        = 0x0060
PID_DEVICE_MODEL_DESC  = 0x0080
PID_MANUFACTURER_LABEL = 0x0081
PID_DEVICE_LABEL       = 0x0082
PID_DMX_START_ADDRESS  = 0x00F0
PID_IDENTIFY_DEVICE    = 0x1000

class Dev:
    def __init__(self, i):
        self.esta = 0x4444
        self.dev  = 0x1000 + i
        self.address = 1 + i * 16
        self.footprint = 16
        self.model = f"SimSpot {i+1}"
        self.maker = "qlcconsole sim"
        self.label = f"Sim Fixture {i+1}"
        self.identify = 0
    @property
    def uid(self):
        return struct.pack(">HI", self.esta, self.dev)

def hdr(op):
    return HDR + struct.pack("<H", op) + bytes([0, 14])

def pollreply(ip):
    a = bytearray(239)
    a[0:8] = HDR
    a[8:10] = struct.pack("<H", OP_POLLREPLY)
    a[10:14] = socket.inet_aton(ip)
    a[14:16] = struct.pack("<H", PORT)
    a[16:18] = struct.pack(">H", 1)
    a[20:22] = struct.pack(">H", 0x0000)
    a[23] = 0xD0
    a[24:26] = struct.pack("<H", 0x4444)
    a[26:44] = b"RDM-SIM".ljust(18, b"\x00")
    a[44:108] = b"qlcconsole RDM simulator".ljust(64, b"\x00")
    a[108:172] = b"#0001 [0] OK".ljust(64, b"\x00")
    a[172:174] = struct.pack(">H", 1)
    a[174] = 0x80          # output port
    a[182] = 0x80          # goodOutput: transmitting; bit3 clear => RDM enabled
    a[212] = 0x08
    return bytes(a)

def toddata(devs, net, addr, port=1):
    a = bytearray(28 + 6 * len(devs))
    a[0:8] = HDR
    a[8:10] = struct.pack("<H", OP_TODDATA)
    a[10:12] = bytes([0, 14])
    a[12] = 0x01           # RdmVer
    a[13] = port
    a[21] = net
    a[22] = 0              # CmdRes
    a[23] = addr
    a[24:26] = struct.pack(">H", len(devs))
    a[26] = 1
    a[27] = len(devs)
    for i, d in enumerate(devs):
        a[28 + i*6:34 + i*6] = d.uid
    return bytes(a)

def rdm_response(dev, pid, payload):
    """Build an RDM GET_COMMAND_RESPONSE message (no ArtRdm wrapper)."""
    m = bytearray()
    m += bytes([0xCC, 0x01])
    m += bytes([24 + len(payload)])
    m += dev.uid                      # destination = controller? (sim: echo)
    m += dev.uid                      # source
    m += bytes([0, 0, 0])             # TN, port, msgcount... simplified
    m += struct.pack(">H", 0)         # sub-device
    m += bytes([0x21])                # GET_COMMAND_RESPONSE
    m += struct.pack(">H", pid)
    m += bytes([len(payload)]) + payload
    cs = sum(m) & 0xFFFF
    m += struct.pack(">H", cs)
    return bytes(m)

def get_payload(dev, pid):
    if pid == PID_DEVICE_INFO:
        return (struct.pack(">BB", 1, 0) + struct.pack(">H", 0x0101)
                + struct.pack(">H", 0x0201) + struct.pack(">H", dev.footprint)
                + bytes([1, 1]) + struct.pack(">H", dev.address)
                + struct.pack(">H", 0) + bytes([0]))
    if pid == PID_DMX_START_ADDRESS:
        return struct.pack(">H", dev.address)
    if pid == PID_DEVICE_MODEL_DESC:
        return dev.model.encode()
    if pid == PID_MANUFACTURER_LABEL:
        return dev.maker.encode()
    if pid == PID_DEVICE_LABEL:
        return dev.label.encode()
    if pid == PID_IDENTIFY_DEVICE:
        return bytes([dev.identify])
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--devices", type=int, default=3)
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--ip", default=None, help="IP to advertise in ArtPollReply")
    a = ap.parse_args()

    devs = [Dev(i) for i in range(a.devices)]
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        try: s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except OSError: pass
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind((a.bind, a.port))
    myip = a.ip or socket.gethostbyname(socket.gethostname())
    print(f"RDM simulator on {a.bind}:{a.port}, advertising {myip}, "
          f"{len(devs)} device(s):", flush=True)
    for d in devs:
        print(f"  UID {d.esta:04X}:{d.dev:08X}  addr={d.address} "
              f"fp={d.footprint} {d.model!r}", flush=True)

    while True:
        data, src = s.recvfrom(2048)
        if not data.startswith(HDR) or len(data) < 12:
            continue
        op = struct.unpack("<H", data[8:10])[0]
        if op == OP_POLL:
            s.sendto(pollreply(myip), src)
            print(f"  <- ArtPoll from {src[0]}, sent ArtPollReply", flush=True)
        elif op in (OP_TODREQUEST, OP_TODCONTROL):
            net = data[21] if len(data) > 21 else 0
            addr = data[24] if op == OP_TODREQUEST and len(data) > 24 else (
                   data[23] if len(data) > 23 else 0)
            s.sendto(toddata(devs, net, addr), src)
            name = "ArtTodRequest" if op == OP_TODREQUEST else "ArtTodControl"
            print(f"  <- {name} from {src[0]} (net={net} addr={addr}), "
                  f"sent TOD with {len(devs)} UID(s)", flush=True)
        elif op == OP_RDM:
            # ArtRdm: 8 id +2 op +2 ver +1 rdmVer +1 filler +7 spare
            # +1 net +1 cmd +1 addr, then the RDM message
            body = data[24:]
            if len(body) < 21 or body[0] != 0xCC:
                continue
            pid = struct.unpack(">H", body[21:23])[0]
            dst = body[3:9]
            dev = next((d for d in devs if d.uid == dst), devs[0])
            pl = get_payload(dev, pid)
            if pl is None:
                print(f"  <- ArtRdm GET pid=0x{pid:04X} (unsupported)", flush=True)
                continue
            out = bytearray(hdr(OP_RDM)) + bytes([0x01]) + bytes(1) + bytes(7)
            out += bytes([0, 0, 0]) + rdm_response(dev, pid, pl)
            s.sendto(bytes(out), src)
            print(f"  <- ArtRdm GET pid=0x{pid:04X} -> {len(pl)}B", flush=True)

if __name__ == "__main__":
    sys.exit(main())
