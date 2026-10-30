# SPDX-License-Identifier: GPL-3.0-only
import json
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import runner_health as health


class RunnerHealthTests(unittest.TestCase):
    def test_optional_command_missing_timeout_and_nonzero_are_explicit(self):
        with patch.object(health.shutil, "which", return_value=None):
            self.assertEqual(health.command(["missing"]), {"status": "unavailable"})
        with patch.object(health.shutil, "which", return_value="/bin/tool"), \
                patch.object(health.subprocess, "run", side_effect=health.subprocess.TimeoutExpired("tool", 4)):
            self.assertEqual(health.command(["tool"]), {"status": "timeout"})
        with patch.object(health.shutil, "which", return_value="/bin/tool"), \
                patch.object(health.subprocess, "run", return_value=SimpleNamespace(returncode=1, stdout="secret")):
            self.assertEqual(health.command(["tool"]), {"status": "unavailable"})

    def test_optional_binary_failure_and_null_tailscale_fields_are_safe(self):
        with patch.object(health.shutil, "which", return_value="tool"), \
                patch.object(health.subprocess, "run", side_effect=PermissionError("secret")):
            self.assertEqual(health.command(["tool"]), {"status": "unavailable"})
        payload = {"BackendState": "NeedsLogin", "Self": None, "Health": None}
        with patch.object(health, "command", return_value={"status": "ok", "output": json.dumps(payload)}):
            self.assertEqual(health.tailscale_state(), {"status": "ok", "backend": "NeedsLogin",
                                                       "self_online": None, "health_count": 0})

    def test_tailscale_projection_does_not_return_peer_or_address_data(self):
        payload = {"BackendState": "Running", "Self": {"Online": True, "DNSName": "secret"},
                   "Health": ["warn"], "Peer": {"100.64.0.2": {"HostName": "secret"}}}
        with patch.object(health, "command", return_value={"status": "ok", "output": json.dumps(payload)}):
            result = health.tailscale_state()
        self.assertEqual(result, {"status": "ok", "backend": "Running", "self_online": True,
                                  "health_count": 1})
        self.assertNotIn("secret", json.dumps(result))

    def test_throttle_flags_are_parsed_without_returning_command_text(self):
        with patch.object(health, "command", return_value={"status": "ok", "output": "throttled=0x50005\n"}):
            self.assertEqual(health.throttled_state(), {"status": "ok", "flags": 0x50005})

    def test_wifi_and_journal_projection_excludes_sensitive_text(self):
        outputs = iter([
            {"status": "ok", "output": "wifi:connected\nwifi-p2p:disconnected\n"},
            {"status": "ok", "output": "under-voltage detected\nmmc0: timeout\nthermal_sys init\nmmc0 boot\n"},
        ])
        with patch.object(health, "command", side_effect=lambda *_: next(outputs)):
            wifi = health.wifi_state()
            counts = health.kernel_counts()
        self.assertEqual(wifi, {"status": "ok", "devices": [
            {"type": "wifi", "state": "connected"},
            {"type": "wifi-p2p", "state": "disconnected"}]})
        self.assertEqual(counts, {"status": "ok", "oom": 0, "undervoltage": 1,
                                  "thermal": 0, "mmc": 1})
        self.assertNotIn("under-voltage", json.dumps(counts))


if __name__ == "__main__":
    unittest.main()
