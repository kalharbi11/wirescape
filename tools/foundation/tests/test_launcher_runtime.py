# SPDX-License-Identifier: GPL-3.0-only
import sys
import hashlib
import json
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from foundation.launcher_runtime import choose_path, compare_revisions, shortcut_target


class LauncherRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def qt(self, root):
        files = ("lib/cmake/Qt6/Qt6Config.cmake", "bin/Qt6Core.dll",
                 "bin/Qt6Gui.dll", "bin/Qt6Qml.dll", "bin/Qt6Quick.dll",
                 "plugins/platforms/qwindows.dll")
        for name in files:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        return root

    def test_precedence_and_auto_candidate_skip(self):
        remembered = self.qt(self.root / "remembered")
        explicit = self.qt(self.root / "explicit")
        result = choose_path("qt", explicit, remembered, [("environment", self.root)])
        self.assertEqual(result, {"path": str(explicit.resolve()), "source": "explicit"})
        result = choose_path("qt", remembered=remembered,
                             candidates=[("environment", self.root), ("repository", explicit)])
        self.assertEqual(result["source"], "remembered")
        result = choose_path("qt", candidates=[("bad", self.root), ("repo", explicit)])
        self.assertEqual(result["source"], "repo")

    def test_invalid_explicit_and_remembered_fail_without_fallback(self):
        valid = self.qt(self.root / "valid")
        with self.assertRaises(ValueError):
            choose_path("qt", explicit=self.root, candidates=[("repo", valid)])
        with self.assertRaises(ValueError):
            choose_path("qt", remembered=self.root, candidates=[("repo", valid)])

    def test_qt_missing_required_dll_rejected(self):
        partial = self.qt(self.root / "partial")
        (partial / "bin/Qt6Quick.dll").unlink()
        with self.assertRaises(ValueError):
            choose_path("qt", candidates=[("repository", partial)])

    def test_portaudio_validation_and_missing_candidate(self):
        dll = self.root / "libportaudio64bit.dll"
        dll.touch()
        self.assertEqual(choose_path("portaudio", candidates=[("env", dll)])[
                         "path"], str(dll.resolve()))
        with self.assertRaises(ValueError):
            choose_path("portaudio", candidates=[("bad", self.root / "missing.dll")])

    def test_shortcut_target_bounds_and_requires_executable(self):
        checkout = self.root / "checkout"
        package = checkout / "build/packages/wirescape-current"
        package.mkdir(parents=True)
        exe = package / "wirescape_studio.exe"
        exe.write_bytes(b"studio")
        self.manifest(checkout, package, exe)
        self.assertEqual(shortcut_target(checkout, package), str(exe.resolve()))
        outside = self.root / "wirescape-v1"
        outside.mkdir()
        (outside / "wirescape_studio.exe").touch()
        with self.assertRaises(ValueError):
            shortcut_target(checkout, outside)
        with self.assertRaises(ValueError):
            shortcut_target(checkout, checkout / "build/packages/empty")

    def manifest(self, checkout, package, exe, source=None, digest=None):
        data = {"build": {"source_root": str((source or checkout).resolve())},
                "files": {"wirescape_studio.exe": digest or hashlib.sha256(exe.read_bytes()).hexdigest()}}
        (package / "package-manifest.json").write_text(json.dumps(data), encoding="utf-8")

    def test_shortcut_requires_matching_provenance_and_file_hash(self):
        checkout = self.root / "checkout"
        package = checkout / "build/packages/pkg"
        package.mkdir(parents=True)
        exe = package / "wirescape_studio.exe"
        exe.write_bytes(b"studio")
        self.manifest(checkout, package, exe, source=self.root / "other")
        with self.assertRaises(ValueError):
            shortcut_target(checkout, package)
        self.manifest(checkout, package, exe, digest="0" * 64)
        with self.assertRaises(ValueError):
            shortcut_target(checkout, package)

    def test_shortcut_symlink_escape_rejected(self):
        checkout = self.root / "checkout"
        packages = checkout / "build/packages"
        packages.mkdir(parents=True)
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "wirescape_studio.exe").touch()
        link = packages / "escape"
        try:
            link.symlink_to(outside, target_is_directory=True)
        except (OSError, NotImplementedError):
            # Exercise the same resolved escape without Windows symlink privileges.
            original = Path.resolve
            with patch.object(Path, "resolve", lambda path: outside if path == link else original(path)):
                with self.assertRaises(ValueError):
                    shortcut_target(checkout, link)
        else:
            with self.assertRaises(ValueError):
                shortcut_target(checkout, link)

    def test_revision_equality_difference_and_validation(self):
        a, b = "a" * 40, "b" * 40
        self.assertEqual(compare_revisions(a, a.upper()), "current")
        self.assertEqual(compare_revisions(a, b), "different")
        for invalid in ("abc", "g" * 40, "a" * 39):
            with self.assertRaises(ValueError):
                compare_revisions(invalid, a)
        with self.assertRaises(ValueError):
            compare_revisions(a, "bad")


if __name__ == "__main__":
    unittest.main()
