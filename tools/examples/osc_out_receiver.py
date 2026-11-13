# SPDX-License-Identifier: GPL-3.0-only
"""Minimal OSC 1.0 receiver for Wirescape's OSC Out module (stdlib only).

Binds 127.0.0.1:9001 by default, decodes single (non-bundle) messages with
`f` (float32) and `i` (int32) arguments, prints `address value...` lines and a
per-second message count. Loopback only: it refuses non-loopback bind hosts.

  python tools/examples/osc_out_receiver.py
  python tools/examples/osc_out_receiver.py --count-seconds 60   # JSON summary, then exit
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time

LOOPBACK = "127.0.0.1"


def _read_string(data: bytes, pos: int) -> tuple[str, int]:
    end = data.find(b"\0", pos)
    if end < 0:
        raise ValueError("unterminated OSC string")
    text = data[pos:end].decode("ascii")
    return text, (end + 4) & ~3  # NUL-terminated, padded to 4 bytes


def decode(data: bytes) -> tuple[str, list]:
    """Decode one OSC 1.0 message into (address, [args]). Raises ValueError."""
    if not data.startswith(b"/"):
        raise ValueError("not an OSC message (bundles are not used in v1)")
    address, pos = _read_string(data, 0)
    if pos >= len(data) or data[pos:pos + 1] != b",":
        return address, []
    tags, pos = _read_string(data, pos)
    args: list = []
    for tag in tags[1:]:
        if tag == "f":
            args.append(struct.unpack_from(">f", data, pos)[0])
        elif tag == "i":
            args.append(struct.unpack_from(">i", data, pos)[0])
        else:
            raise ValueError(f"unsupported type tag {tag!r}")
        pos += 4
    return address, args


def forward(address: str, args: list) -> None:
    """Hook: forward to a microcontroller here (serial, I2C, SPI...).

    Example: `serial_port.write(f"{address} {args[0]}\\n".encode())`.
    Keep it quick; this runs once per received message.
    """


def run(port: int, count_seconds: float | None) -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOOPBACK, port))
    sock.settimeout(0.1)
    start = last = time.monotonic()
    total = bad = 0
    window = 0
    per_address: dict[str, int] = {}
    try:
        while True:
            now = time.monotonic()
            if count_seconds is not None and now - start >= count_seconds:
                break
            try:
                data, _ = sock.recvfrom(2048)
            except socket.timeout:
                data = None
            if data is not None:
                try:
                    address, args = decode(data)
                except (ValueError, struct.error):
                    bad += 1
                else:
                    total += 1
                    window += 1
                    per_address[address] = per_address.get(address, 0) + 1
                    if count_seconds is None:
                        print(address, *args, flush=True)
                    forward(address, args)
            if now - last >= 1.0:
                if count_seconds is None:
                    print(f"# {window} msg/s, {total} total", flush=True)
                window, last = 0, now
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    if count_seconds is not None:
        print(json.dumps({"port": port, "seconds": count_seconds, "messages": total,
                          "malformed": bad, "per_address": per_address}, sort_keys=True))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=9001, help="UDP port (default 9001)")
    parser.add_argument("--count-seconds", type=float, metavar="N",
                        help="count for N seconds, print a JSON summary and exit")
    opts = parser.parse_args(argv)
    if not 1 <= opts.port <= 65535:
        parser.error("--port must be 1..65535")
    return run(opts.port, opts.count_seconds)


if __name__ == "__main__":
    sys.exit(main())
