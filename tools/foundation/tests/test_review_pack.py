# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / "review_pack.py"
SPEC = importlib.util.spec_from_file_location("review_pack", SCRIPT)
pack = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pack)
FIXTURE = Path(__file__).parent / "fixtures/sample.diff"


class ReviewPackTests(unittest.TestCase):
    def test_noise_is_listed_but_not_in_diff_and_last_acceptance_selected(self):
        args = SimpleNamespace(prompt="external", max_bytes=8192, split=1)
        task = "```acceptance\nold: true\n```\ntext\n```acceptance\ntier: T1\n```\n"
        text = pack.make_pack(args, ("Title", "Intent", "a" * 40, FIXTURE.read_text(),
                                    pack.acceptance(task), "CI unavailable", "Accept unavailable"))
        for name in ("noisy.json", "table.h", "wave.wav"):
            self.assertIn(name, text)
        for noise in ("noise-hidden", "generated_noise_hidden", "old: true", "Binary files a/"):
            self.assertNotIn(noise, text)
        self.assertIn("+int sample = 1;", text)
        self.assertIn("tier: T1", text)
        self.assertTrue(text.startswith(pack.PROMPTS["external"]))

    def test_unicode_cap_and_exactly_three_parts(self):
        diff = "diff --git a/native/music.cpp b/native/music.cpp\n--- a/native/music.cpp\n+++ b/native/music.cpp\n"
        diff += "+// café 🎵" * 2000
        args = SimpleNamespace(prompt="claude", max_bytes=8192, split=3)
        text = pack.make_pack(args, ("音楽", "Intent", None, diff, "No block", "No CI", "No accept"))
        with tempfile.TemporaryDirectory() as tmp:
            paths = pack.write_pack(text, Path(tmp), "test", 3, 8192)
            self.assertEqual(len(paths), 3)
            self.assertTrue(all(p.stat().st_size <= 8192 // 3 for p in paths))
            self.assertLessEqual(sum(p.stat().st_size for p in paths), 8192)
            self.assertEqual("".join(p.read_text() for p in paths), text)
            self.assertEqual(b"".join(p.read_bytes() for p in paths), text.encode("utf-8"))
            self.assertIn("Diff truncated by size cap", text)

    def test_many_parts_keep_complete_prompt_in_first_file(self):
        args = SimpleNamespace(prompt="external", max_bytes=61440, split=120)
        text = pack.make_pack(args, ("Title", "Intent", None, FIXTURE.read_text(), "Block", "CI", "Accept"))
        with tempfile.TemporaryDirectory() as tmp:
            paths = pack.write_pack(text, Path(tmp), "many", 120, 61440)
            self.assertEqual(len(paths), 120)
            self.assertTrue(paths[0].read_text(encoding="utf-8").startswith(pack.PROMPTS["external"]))
            self.assertTrue(all(p.stat().st_size <= 512 for p in paths))

    def test_offline_cli_needs_no_token_and_does_not_invent_ci_or_head(self):
        with tempfile.TemporaryDirectory() as tmp:
            task = Path(tmp) / "task.md"
            task.write_text("```acceptance\ntier: T0\nmust_pass:\n  - python test.py\n```\n")
            env = {k: v for k, v in os.environ.items() if k not in ("GITHUB_TOKEN", "GH_TOKEN")}
            result = subprocess.run([sys.executable, str(SCRIPT), "--diff", str(FIXTURE), "--task", str(task),
                                     "--output", str(Path(tmp) / "out"), "--split", "3"],
                                    capture_output=True, text=True, env=env)
            self.assertEqual(result.returncode, 0, result.stderr)
            paths = sorted((Path(tmp) / "out").glob("*.md"))
            text = "".join(p.read_text() for p in paths)
            self.assertEqual(len(paths), 3)
            self.assertIn("offline mode; no CI request made", text)
            self.assertIn("Head: `unavailable`", text)
            self.assertIn("no exact-SHA accept report", text)

    def test_local_accept_requires_exact_commit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "build_reports").mkdir()
            path = root / "build_reports/accept-86.json"
            path.write_text(json.dumps({"commit": "a" * 40, "status": "passed", "commands": []}))
            self.assertIsNone(pack.local_accept(root, "b" * 40))
            self.assertIn("exact SHA", pack.local_accept(root, "a" * 40))

    def test_online_diff_and_checks_use_immutable_head(self):
        args = SimpleNamespace(diff=None, task=None, repo="owner/repo", pr=86)
        head, base = "a" * 40, "b" * 40
        def respond(path, diff=False):
            if path.endswith("/pulls/86"):
                return {"title": "Title", "body": "Intent", "head": {"sha": head}, "base": {"sha": base}}
            if diff:
                self.assertIn(f"{base}...{head}", path)
                return FIXTURE.read_text()
            self.assertIn(head, path)
            return {"check_runs": [], "total_count": 0} if "check-runs" in path else {"statuses": []}
        with tempfile.TemporaryDirectory() as tmp, patch.object(pack, "api", side_effect=respond):
            evidence = pack.collect(args, Path(tmp))
            self.assertEqual(evidence[2], head)
            self.assertIn("no checks returned", evidence[5])
            self.assertIn("no accept check summary", evidence[6])

    def test_redirect_cannot_forward_authentication(self):
        with self.assertRaisesRegex(ValueError, "not forwarded"):
            pack.NoRedirect().redirect_request(None, None, 302, "redirect", {}, "https://other.invalid/")

    def test_online_task_is_loaded_from_exact_pr_head(self):
        args = SimpleNamespace(diff=None, task=None, repo="owner/repo", pr=86)
        def respond(path, diff=False):
            if path.endswith("/pulls/86"):
                return {"title": "Title", "body": "docs/tasks/new.md", "head": {"sha": "a" * 40}, "base": {"sha": "b" * 40}}
            if diff:
                return FIXTURE.read_text()
            self.assertIn("/contents/docs/tasks/new.md?ref=" + "a" * 40, path)
            return {"encoding": "base64", "content": base64.b64encode(b"```acceptance\ntier: T2\n```\n").decode()}
        with tempfile.TemporaryDirectory() as tmp, patch.object(pack, "api", side_effect=respond), \
                patch.object(pack, "ci_summary", return_value=("CI", "Accept")):
            self.assertIn("tier: T2", pack.collect(args, Path(tmp))[4])

    def test_impossible_cap_rejected_without_silently_dropping_acceptance(self):
        args = SimpleNamespace(prompt="chatgpt", max_bytes=512, split=1)
        with self.assertRaisesRegex(ValueError, "increase --max-bytes"):
            pack.make_pack(args, ("Title", "Intent", None, FIXTURE.read_text(), "Acceptance", "CI", "Accept"))


if __name__ == "__main__":
    unittest.main()
