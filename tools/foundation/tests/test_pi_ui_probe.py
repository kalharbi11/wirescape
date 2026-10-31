# SPDX-License-Identifier: GPL-3.0-only
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from pi_ui_probe import clean_environment, run


class ProbeTests(unittest.TestCase):
    def invoke(self, results):
        out = io.StringIO()
        with patch("pi_ui_probe.subprocess.run", side_effect=results) as call, contextlib.redirect_stdout(out):
            code = run(Path("/probe"))
        return code, json.loads(out.getvalue()), call

    def test_commands_and_environment_are_bounded_and_offscreen(self):
        good = subprocess.CompletedProcess([], 0, "ok", "")
        with patch.dict("os.environ", {"ACCESS_TOKEN":"x", "DB_PASSWORD":"x", "ACTIONS_ID":"x",
                                       "GIT_CONFIG_COUNT":"1", "SAFE":"yes"}, clear=True):
            code, report, call = self.invoke([good, good, good])
        self.assertEqual(code, 0)
        self.assertEqual([c.args[0][0] for c in call.call_args_list], ["cmake", "cmake", "ctest"])
        self.assertEqual(call.call_args_list[0].args[0][1:4], ["-S", str(Path("/probe").resolve() / "native"), "-B"])
        self.assertIn("-DWIRESCAPE_BUILD_UI=ON", report["commands"][0])
        self.assertTrue(all(t in report["commands"][1] for t in
                            ("--target", "wirescape_studio", "studio_ui_tests", "controller_failure_tests", "audio_device_tests", "ws_prepare_failure")))
        self.assertEqual(report["commands"][2][report["commands"][2].index("-R") + 1],
                         "^(studio_ui_tests|controller_failure_tests|audio_device_tests)$")
        self.assertIn("--no-tests=error", report["commands"][2])
        child_env = call.call_args_list[0].kwargs["env"]
        self.assertEqual(child_env, {"SAFE":"yes", "QT_QPA_PLATFORM":"offscreen", "QT_QUICK_BACKEND":"software"})
        self.assertEqual(clean_environment({"TOKEN":"x", "MY_SECRET":"x", "OK":"y"}), {"OK":"y"})

    def test_configure_and_build_fail_closed(self):
        failed = subprocess.CompletedProcess([], 1, "", "Qt not found")
        code, report, call = self.invoke([failed])
        self.assertEqual((code, report["stage"], call.call_count), (1, "configure", 1))
        self.assertIn("Qt not found", report["output_tail"])
        good = subprocess.CompletedProcess([], 0, "", "")
        code, report, call = self.invoke([good, subprocess.CompletedProcess([], 1, "", "target missing")])
        self.assertEqual((code, report["stage"], call.call_count), (1, "build", 2))
        self.assertIn("target missing", report["output_tail"])

    def test_ctest_failure_propagates_and_does_not_launch_devices(self):
        good = subprocess.CompletedProcess([], 0, "", "")
        bad = subprocess.CompletedProcess([], 2, "", "offscreen test failed")
        code, report, call = self.invoke([good, good, bad])
        self.assertEqual((code, report["stage"], report["returncode"]), (1, "ctest", 2))
        self.assertEqual(call.call_args_list[2].args[0][0], "ctest")
