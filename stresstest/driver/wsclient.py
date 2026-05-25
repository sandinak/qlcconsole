"""
Minimal dependency-free WebSocket client (RFC 6455) for driving the QLC+
web access API. Only what we need: text frames, client masking, ping/pong.
"""

import base64
import os
import socket
import struct
import time


class WSError(Exception):
    pass


class WSClient:
    def __init__(self, host, port, path="/qlcplusWS", timeout=5.0):
        self.host = host
        self.port = port
        self.path = path
        self.timeout = timeout
        self.sock = None
        self._buf = b""

    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.settimeout(self.timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        req = (
            f"GET {self.path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(req.encode())
        resp = self._read_until(b"\r\n\r\n")
        if b"101" not in resp.split(b"\r\n", 1)[0]:
            raise WSError(f"handshake failed: {resp[:120]!r}")

    def _read_until(self, marker):
        while marker not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise WSError("connection closed during handshake")
            self._buf += chunk
        head, self._buf = self._buf.split(marker, 1)
        return head + marker

    def _recv_exact(self, n):
        while len(self._buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise WSError("connection closed")
            self._buf += chunk
        out, self._buf = self._buf[:n], self._buf[n:]
        return out

    def send(self, text):
        payload = text.encode("utf-8")
        header = bytearray()
        header.append(0x81)  # FIN + text opcode
        mask = os.urandom(4)
        n = len(payload)
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", n)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", n)
        header += mask
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        self.sock.sendall(bytes(header) + masked)

    def recv(self):
        """Return next text message, or None on timeout/control frame."""
        try:
            b0, b1 = self._recv_exact(2)
        except (socket.timeout, WSError):
            return None
        opcode = b0 & 0x0F
        masked = b1 & 0x80
        length = b1 & 0x7F
        if length == 126:
            length = struct.unpack(">H", self._recv_exact(2))[0]
        elif length == 127:
            length = struct.unpack(">Q", self._recv_exact(8))[0]
        if masked:
            mkey = self._recv_exact(4)
        data = self._recv_exact(length) if length else b""
        if masked:
            data = bytes(b ^ mkey[i % 4] for i, b in enumerate(data))
        if opcode == 0x8:   # close
            raise WSError("server closed connection")
        if opcode == 0x9:   # ping -> pong
            return None
        if opcode in (0x1, 0x2):
            return data.decode("utf-8", "replace")
        return None

    def request(self, text, expect_prefix=None, wait=2.0):
        """Send a command and read replies until one matches expect_prefix."""
        self.send(text)
        if expect_prefix is None:
            return None
        deadline = time.time() + wait
        while time.time() < deadline:
            msg = self.recv()
            if msg and msg.startswith(expect_prefix):
                return msg
        return None

    def close(self):
        try:
            if self.sock:
                self.sock.close()
        except OSError:
            pass
