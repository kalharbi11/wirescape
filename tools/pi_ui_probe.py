# SPDX-License-Identifier: GPL-3.0-only
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

TARGETS = ("wirescape_studio", "studio_ui_tests", "controller_failure_tests", "audio_device_tests",
           "ws_prepare_failure")
TESTS = "^(studio_ui_tests|controller_failure_tests|audio_device_tests)$"
TIMEOUT = 900


def clean_environment(source=None):
    source = os.environ if source is None else source
    blocked = ("TOKEN", "SECRET", "PASSWORD", "ACTIONS_", "GIT_CONFIG")
    return {key: value for key, value in source.items() if not any(word in key.upper() for word in blocked)}


def tail(value):
    if isinstance(value, bytes):
        value = value.decode(errors="replace")
    return (value or "")[-4000:]


def run(root):
    root = Path(root).resolve()
    build = root / "build" / "pi-ui"
    env = clean_environment()
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
    record = {"status": "failed", "build_dir": str(build), "stage": "configure"}
    steps = [
        ["cmake", "-S", str(root / "native"), "-B", str(build), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DWIRESCAPE_BUILD_UI=ON"],
        ["cmake", "--build", str(build), "--config", "Release", "--parallel", "2", "--target", *TARGETS],
        ["ctest", "--test-dir", str(build), "-R", TESTS, "--no-tests=error", "--output-on-failure"],
    ]
    record["commands"] = steps
    for index, command in enumerate(steps):
        record["stage"] = ("configure", "build", "ctest")[index]
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                    text=True, timeout=TIMEOUT, check=False)
            record["returncode"] = result.returncode
            if result.returncode:
                record["output_tail"] = tail(result.stdout + "\n" + result.stderr)
                break
        except (OSError, subprocess.TimeoutExpired) as error:
            record["returncode"] = 124 if isinstance(error, subprocess.TimeoutExpired) else 127
            record["error"] = str(error)
            if isinstance(error, subprocess.TimeoutExpired):
                record["output_tail"] = tail(error.stdout) + tail(error.stderr)
            break
    else:
        record.update(status="passed", stage="complete", returncode=0)
    print(json.dumps(record, sort_keys=True))
    return int(record["returncode"] != 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    sys.exit(run(parser.parse_args().root))
