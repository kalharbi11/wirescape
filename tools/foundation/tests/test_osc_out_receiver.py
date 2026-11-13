# SPDX-License-Identifier: GPL-3.0-only
from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "examples"))
import osc_out_receiver as rx


def pad(text: str) -> bytes:
    raw = text.encode("ascii") + b"\0"
    return raw + b"\0" * (-len(raw) % 4)


class DecodeTests(unittest.TestCase):
    def test_float_message(self):
        data = pad("/ws/row/1") + pad(",f") + struct.pack(">f", 0.5)
        self.assertEqual(rx.decode(data), ("/ws/row/1", [0.5]))

    def test_note_message_ifi(self):
        data = pad("/ws/note") + pad(",ifi") + struct.pack(">ifi", 60, 0.75, 2)
        self.assertEqual(rx.decode(data), ("/ws/note", [60, 0.75, 2]))

    def test_no_arguments(self):
        self.assertEqual(rx.decode(pad("/ping")), ("/ping", []))

    def test_rejects_bundle_garbage_and_unknown_tags(self):
        with self.assertRaises(ValueError):
            rx.decode(b"#bundle\0" + b"\0" * 8)
        with self.assertRaises(ValueError):
            rx.decode(pad("/a") + pad(",s") + pad("x"))
        with self.assertRaises(ValueError):
            rx.decode(b"/unterminated")


if __name__ == "__main__":
    unittest.main()
