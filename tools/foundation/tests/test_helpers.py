from __future__ import annotations

import json
import os
import sys
import tempfile
import time
import unittest
import zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

TEMP_ROOT = Path(__file__).resolve().parents[3] / "build_reports" / "foundation"
TEMP_ROOT.mkdir(parents=True, exist_ok=True)

from foundation import helpers
from foundation.helpers import create_snapshot, run_logged, wait_for_readiness


class SnapshotTests(unittest.TestCase):
    def test_snapshot_contains_only_private_data_and_hashes_every_file(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            root = Path(temporary) / "repo"
            backups = root / "local_backups"
            (root / "user_data" / "presets").mkdir(parents=True)
            (root / "modules" / "chaos" / "surgepy_bin").mkdir(parents=True)
            (root / "build" / "native").mkdir(parents=True)
            (root / ".git").mkdir()
            (root / ".venv").mkdir()
            (root / "user_data" / "presets" / "my.json").write_text("{}", encoding="utf-8")
            (root / "modules" / "chaos" / "surgepy_bin" / "surgepy.pyd").write_bytes(b"binary")
            (root / "build" / "native" / "build.tmp").write_text("skip", encoding="utf-8")
            (root / ".git" / "index").write_text("skip", encoding="utf-8")
            (root / ".venv" / "python.exe").write_text("skip", encoding="utf-8")

            result = create_snapshot(root, backups, "unit test", "abc123", "fixed")
            archive = Path(result["archive"])
            manifest = json.loads(Path(result["manifest"]).read_text(encoding="utf-8"))
            with zipfile.ZipFile(archive) as bundle:
                self.assertEqual(
                    set(bundle.namelist()),
                    {"user_data/presets/my.json", "modules/chaos/surgepy_bin/surgepy.pyd"},
                )
            self.assertEqual(manifest["commit"], "abc123")
            self.assertEqual(result["file_count"], 2)
            self.assertTrue(all(len(item["sha256"]) == 64 for item in manifest["files"]))
            with self.assertRaises(FileExistsError):
                create_snapshot(root, backups, "unit test", "abc123", "fixed")

    def test_snapshot_handles_absent_private_directories(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            root = Path(temporary) / "empty"
            root.mkdir()
            result = create_snapshot(root, root / "local_backups", "empty", stamp="fixed")
            self.assertEqual(result["file_count"], 0)


class CommandLogTests(unittest.TestCase):
    def test_run_logged_preserves_output_and_exit_status(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            folder = Path(temporary)
            log = folder / "raw.log"
            result = run_logged(
                [sys.executable, "-c", "import sys; print('trace'); sys.exit(7)"],
                folder,
                log,
            )
            self.assertEqual(result["exit_code"], 7)
            self.assertIn("trace", log.read_text(encoding="utf-8"))
            self.assertEqual(result["log"], str(log))

    def test_timeout_kills_owned_child_tree_but_leaves_unrelated_process(self) -> None:
        import os
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            folder = Path(temporary)
            child_code = "import time; time.sleep(30)"
            parent_code = ("import subprocess,sys,time; "
                           "p=subprocess.Popen([sys.executable,'-c'," + repr(child_code) + "]); "
                           "print(p.pid,flush=True); time.sleep(30)")
            unrelated = __import__("subprocess").Popen([sys.executable, "-c", "import time; time.sleep(30)"])
            try:
                result = run_logged([sys.executable, "-c", parent_code], folder, folder/"tree.log", timeout=.5)
                self.assertTrue(result["timed_out"])
                child_pid = int((folder/"tree.log").read_text().splitlines()[0])
                for pid in (child_pid,):
                    for _ in range(30):
                        try: os.kill(pid, 0)
                        except OSError: break
                        # Minimal containers may retain a dead orphan as a zombie.
                        stat = Path(f"/proc/{pid}/stat")
                        if sys.platform == "linux" and stat.exists() and stat.read_text().split(") ", 1)[1].startswith("Z"):
                            break
                        time.sleep(.05)
                    else: self.fail(f"owned descendant {pid} survived timeout")
                self.assertIsNone(unrelated.poll())
            finally:
                unrelated.terminate(); unrelated.wait(timeout=3)

    def test_readiness_report_and_early_exit(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            folder = Path(temporary); report = folder/"ready.json"
            good = "import json,sys,time; p=sys.argv[2]; json.dump({'status':'ready','audio_started':False,'normal_window':True},open(p,'w')); time.sleep(.2)"
            result = wait_for_readiness([sys.executable, "-c", good], folder, folder/"good.log", report, os.environ.copy(), .5)
            self.assertTrue(result["ready"])
            helpers._READINESS_PROCESSES.pop(result["process_id"]).wait(timeout=2)
            helpers._close_windows_job(result["process_id"])
            bad = wait_for_readiness([sys.executable, "-c", "raise SystemExit(9)"], folder,
                                     folder/"bad.log", report, os.environ.copy(), 1)
            self.assertFalse(bad["ready"])
            self.assertIn("exited before readiness", bad["error"])


if __name__ == "__main__":
    unittest.main()
