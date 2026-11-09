#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Bounded, device-off Pi ARM64 Electron/real-sidecar smoke in the existing suite."""
from __future__ import annotations

import argparse
import ctypes.util
import hashlib
import json
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REQUIRED = {
    "engine": ROOT / "build/native/wirescape-engine",
    "electron": ROOT / "apps/desktop/node_modules/electron/dist/electron",
    "main": ROOT / "apps/desktop/build/main.js",
    "ui": ROOT / "apps/studio/dist/index.html",
    "playwright": ROOT / "apps/studio/node_modules/playwright/cli.js",
}


def arm64_elf(file: Path) -> bool:
    """Check the executable itself, not host or npm package metadata."""
    try:
        with file.open("rb") as stream:
            data = stream.read(20)
        return (len(data) >= 20 and data[:4] == b"\x7fELF" and
                data[4] == 2 and data[5] == 1 and
                int.from_bytes(data[18:20], "little") == 183)
    except OSError:
        return False


def git_sha() -> str | None:
    try:
        found = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                               capture_output=True, text=True, timeout=10)
        sha = found.stdout.strip()
        return sha if found.returncode == 0 and re.fullmatch(r"[0-9a-f]{40}", sha) else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def clean_checkout() -> bool:
    try:
        result = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT,
                                capture_output=True, text=True, timeout=10)
        return result.returncode == 0 and not result.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return False


def sanitize(output: str) -> str:
    output = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", output)
    output = re.sub(r"(?i)(bearer\s+)\S+", r"\1<redacted>", output)
    output = re.sub(r"(?i)((?:token|password|secret|_authToken)\s*[=:]\s*)\S+", r"\1<redacted>", output)
    output = re.sub(r"\b(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{12,}\b", "<redacted>", output)
    output = re.sub(r"(https?://)[^\s/@]+:[^\s/@]+@", r"\1<redacted>@", output)
    output = output.replace(str(ROOT), "<checkout>")
    home = str(Path.home())
    return output.replace(home, "<home>") if home != "/" else output


def tool_versions() -> dict[str, str | None]:
    info = {"python": platform.python_version()}
    for tool in ("node", "npm", "cmake", "c++", "git"):
        binary = shutil.which(tool)
        if not binary:
            info[tool] = None
            continue
        try:
            version = subprocess.run([binary, "--version"], capture_output=True,
                                     text=True, timeout=10)
            lines = version.stdout.strip().splitlines()
            info[tool] = lines[0] if version.returncode == 0 and lines else None
        except (OSError, subprocess.TimeoutExpired):
            info[tool] = None
    return info


def check_environment(artifacts: bool = True) -> tuple[list[str], str | None]:
    problems = []
    if platform.system() != "Linux" or platform.machine().lower() not in ("aarch64", "arm64"):
        problems.append("Linux ARM64 host is required; x86/cloud mocks are not Pi evidence")
    if not git_sha() or not clean_checkout():
        problems.append("The source must be a clean immutable Git checkout at the PR head")
    if not ctypes.util.find_library("portaudio"):
        problems.append("libportaudio.so.2 is missing; owner must install the OS runtime")
    if os.environ.get("DISPLAY"):
        backend = "x11"
    elif os.environ.get("WAYLAND_DISPLAY"):
        backend = "wayland"
    else:
        backend = None
        problems.append("No DISPLAY or WAYLAND_DISPLAY; owner must provide an existing display or Xvfb (never auto-install)")
    for name in ("node", "npm"):
        if not shutil.which(name):
            problems.append(f"Missing {name} on PATH; provision it outside the smoke")
    for name, file in REQUIRED.items():
        if artifacts and not file.is_file():
            problems.append(f"Missing {name}: build from existing pinned lockfiles/source in this checkout")
    for name in ("engine", "electron"):
        if artifacts and REQUIRED[name].is_file() and not arm64_elf(REQUIRED[name]):
            problems.append(f"{name} is not a Linux ARM64 ELF64 artifact; rebuild from this checkout, never reuse x86 outputs")
    if artifacts and REQUIRED["electron"].is_file():
        try:
            linked = subprocess.run(["ldd", str(REQUIRED["electron"])], capture_output=True,
                                    text=True, timeout=10)
            if linked.returncode != 0 or "not found" in linked.stdout:
                problems.append("Electron shared libraries missing; inspect ldd and install OS prerequisites manually")
        except (OSError, subprocess.TimeoutExpired):
            problems.append("Cannot inspect Electron linked libraries with ldd")
    return problems, backend


def run_child(command: list[str], cwd: Path, env: dict[str, str], seconds: int) -> tuple[int, str]:
    """Own the entire process group; timeout only kills our launched children."""
    with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors="replace",
                          start_new_session=True) as child:
        try:
            out, _ = child.communicate(timeout=max(1, seconds))
            return child.returncode, out
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            out, _ = child.communicate(timeout=5)
            return 124, out + "\nOwned smoke process group timed out and was terminated.\n"


def run(report: Path, timeout: int) -> tuple[int, dict]:
    started = time.monotonic()
    result = {"status": "blocked", "source_sha": git_sha(),
              "architecture": platform.machine(), "platform": platform.system(),
              "versions": tool_versions(),
              "display_backend": None, "stages": [], "reason": None}
    problems, backend = check_environment(artifacts=False)
    result["display_backend"] = backend
    if problems:
        result["reason"] = problems
        return 2, result
    deadline = started + timeout
    # Reuse the exact checkout's existing drivers and lockfiles; no updater,
    # fetch, checkout, host package installation, or device launch occurs.
    prepare = (
        ("native-build", [sys.executable, "tools/dev.py", "build", "--headless", "--config", "Release"], ROOT),
        ("studio-ci", ["npm", "ci"], ROOT / "apps/studio"),
        ("desktop-ci", ["npm", "ci"], ROOT / "apps/desktop"),
        ("desktop-build", ["npm", "run", "build"], ROOT / "apps/desktop"),
    )
    for name, command, cwd in prepare:
        try:
            code, output = run_child(command, cwd, os.environ.copy(), max(1, int(deadline - time.monotonic())))
        except OSError as error:
            code, output = 127, f"Cannot start {name}: {error}"
        (report / f"{name}.log").write_text(sanitize(output[:3 * 1024 * 1024]), encoding="utf-8")
        result["stages"].append({"stage": name, "exit_code": code, "log": name + ".log"})
        if code:
            result["status"] = "failed" if code != 127 else "blocked"
            result["reason"] = f"{name} failed (exit {code}); inspect the stage log and fix the owner-managed prerequisite"
            return 1 if code != 127 else 2, result
    problems, _ = check_environment()
    if problems:
        result["reason"] = problems
        return 2, result
    if git_sha() != result["source_sha"]:
        result["status"] = "failed"
        result["reason"] = "Git HEAD changed during the build; exact-source smoke refused"
        return 1, result
    result["binary_sha256"] = {
        key: hashlib.sha256(REQUIRED[key].read_bytes()).hexdigest() for key in ("engine", "electron")
    }
    command = [shutil.which("node"), str(REQUIRED["playwright"]), "test",
               "--config=playwright.config.cjs", "--grep=Pi ARM64:",
               "--reporter=json", "--output=" + str(report / "playwright")]
    with tempfile.TemporaryDirectory(prefix="wirescape pi isolated profile ") as profile:
        env = {**os.environ, "WIRESCAPE_PI_APP_SMOKE": "1",
               "WIRESCAPE_TEST_STUB": "0", "WIRESCAPE_PI_PROFILE_DIR": profile,
               "WIRESCAPE_PI_DISPLAY_BACKEND": backend,
               "WIRESCAPE_REAL_ENGINE": str(REQUIRED["engine"])}
        try:
            code, output = run_child(command, ROOT / "apps/desktop", env,
                                     max(1, int(deadline - time.monotonic())))
        except OSError as error:
            code, output = 127, f"Cannot start Playwright: {error}"
    (report / "playwright.log").write_text(sanitize(output[:3 * 1024 * 1024]), encoding="utf-8")
    stage = {"stage": "real-electron", "exit_code": code, "log": "playwright.log"}
    result["stages"].append(stage)
    if code:
        result["status"] = "failed"
        result["reason"] = "Real Electron/sidecar test failed or timed out; see playwright.log and failure traces"
        return 1, result
    try:
        data = json.loads(output)
        stats = data["stats"]
        result["test_counts"] = {k: int(stats.get(k, 0)) for k in ("expected", "unexpected", "skipped", "flaky")}
        if stats.get("expected") != 1 or stats.get("unexpected") or stats.get("skipped") or stats.get("flaky"):
            raise ValueError("Zero/skipped/flaky/failed real tests are never Pi evidence")
    except (ValueError, KeyError, TypeError, json.JSONDecodeError):
        result["status"] = "failed"
        result["reason"] = "Missing or invalid Playwright JSON evidence; zero/skipped tests cannot pass"
        return 1, result
    result["status"] = "passed"
    result["reason"] = "Actual ARM64 Electron plus native device-off smoke passed (functional display only)"
    return 0, result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", default="build_reports/pi-app-smoke", type=Path)
    parser.add_argument("--timeout", type=int, default=1500)
    args = parser.parse_args()
    report = (ROOT / args.report).resolve()
    if not (ROOT / "build_reports").resolve().is_relative_to(ROOT) or not report.is_relative_to((ROOT / "build_reports").resolve()) or not (60 <= args.timeout <= 1500):
        parser.error("report must be under checkout/build_reports; timeout 60..1500 seconds")
    report.mkdir(parents=True, exist_ok=True)
    code, result = run(report, args.timeout)
    (report / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Pi app smoke: {result['status']} (source {result['source_sha']}); report: {report / 'summary.json'}")
    if result["reason"]:
        print(result["reason"])
    return code


if __name__ == "__main__":
    sys.exit(main())
