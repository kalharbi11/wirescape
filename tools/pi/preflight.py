#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only Pi source preflight and sanitized stage logging; never opens devices."""
from __future__ import annotations

import argparse
import ctypes.util
import json
import os
import platform
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "foundation"))
from helpers import discover_toolchain, tool_version  # noqa: E402

NODE_LTS = {22, 24, 26}  # Maintained even-numbered LTS releases in Nov 2026.
REQUIRED_LIBS = {
    "portaudio": "libportaudio2", "nss3": "libnss3", "gtk-3": "libgtk-3-0t64",
    "atk-1.0": "libatk1.0-0t64", "atk-bridge-2.0": "libatk-bridge2.0-0t64",
    "X11": "libx11-6", "Xcomposite": "libxcomposite1",
    "Xdamage": "libxdamage1", "Xrandr": "libxrandr2",
    "gbm": "libgbm1", "asound": "libasound2t64",
}
STAGES = ("native", "studio-ci", "studio-build", "desktop-ci", "desktop-build")


def git(root: Path, *args: str) -> tuple[int, str]:
    try:
        proc = subprocess.run(["git", "-C", str(root), *args], text=True,
                              capture_output=True, timeout=10, check=False)
        return proc.returncode, proc.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return 127, ""


def versions() -> dict[str, dict]:
    found = discover_toolchain()
    names = {"git": shutil.which("git"), "c++": shutil.which("c++"),
             "cmake": found["cmake"], "ninja": found["ninja"],
             "node": shutil.which("node"), "npm": shutil.which("npm")}
    result = {name: tool_version(path) for name, path in names.items()}
    result["python"] = {"available": True, "version": platform.python_version()}
    return result


def inspect(root: Path) -> tuple[list[tuple[str, bool, str]], dict]:
    checks: list[tuple[str, bool, str]] = []

    def check(name: str, ok: bool, remedy: str) -> None:
        checks.append((name, bool(ok), remedy))

    arch = platform.machine().lower()
    check("Linux ARM64", platform.system() == "Linux" and arch in ("aarch64", "arm64"),
          "Use Raspberry Pi OS 64-bit on a Pi 5; x86 mocks are not Pi evidence.")
    tools = versions()
    for name in ("git", "c++", "cmake", "ninja", "node", "npm"):
        check(name, tools[name]["available"], f"Install {name} for this user and make it available on PATH (owner-managed).")
    check("Python >= 3.10", sys.version_info >= (3, 10), "Install a maintained Python 3 release (3.12 recommended).")
    node_version = tools["node"].get("version") or ""
    parsed = re.search(r"\bv?(\d+)\.\d+\.\d+", node_version)
    check("Node maintained LTS", bool(parsed and int(parsed.group(1)) in NODE_LTS),
          "Install a maintained Node.js LTS (22, 24, or 26) with matching npm; do not use an odd or EOL major.")
    for lock in ("apps/studio/package-lock.json", "apps/desktop/package-lock.json"):
        check(lock, (root / lock).is_file(), f"Restore tracked {lock} from the exact source revision; run npm ci, never npm install.")
    for lib, package in REQUIRED_LIBS.items():
        check(f"lib:{lib}", ctypes.util.find_library(lib) is not None,
              f"Owner: install the distro package providing {lib} (often {package}); rerun preflight.")
    electron = root / "apps/desktop/node_modules/electron/dist/electron"
    if electron.is_file():
        try:
            result = subprocess.run(["ldd", str(electron)], capture_output=True, text=True,
                                    timeout=10, check=False)
            missing = re.findall(r"^\s*([^\s]+) => not found", result.stdout, re.M)
            check("Electron ELF dependencies", result.returncode == 0 and not missing,
                  "Install missing Electron shared libraries via the OS package manager: " +
                  (", ".join(missing) if missing else "inspect ldd on the Electron binary"))
        except (OSError, subprocess.TimeoutExpired):
            check("Electron ELF dependencies", False, "Install ldd and verify the ARM64 Electron binary dependencies.")
    try:
        free = shutil.disk_usage(root).free
    except OSError:
        free = 0
    check("disk >= 2 GiB free", free >= 2 * 1024**3,
          "Free at least 2 GiB in the checkout filesystem without deleting owner data.")
    check("checkout writable", os.access(root, os.W_OK | os.X_OK),
          "Owner: correct checkout ownership/permissions; do not sudo the build.")
    rc, sha = git(root, "rev-parse", "HEAD")
    check("source HEAD", rc == 0 and bool(re.fullmatch(r"[0-9a-f]{40}", sha)),
          "Run inside a full Git checkout; resolve an immutable commit first.")
    rc, dirty = git(root, "status", "--porcelain", "--untracked-files=normal")
    check("clean tree", rc == 0 and not dirty,
          "Commit or separately save owner changes before updating; never reset or clean automatically.")
    inherited = ("CMAKE_PREFIX_PATH", "Qt6_DIR", "QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "WIRESCAPE_QT_DIR")
    present = [key for key in inherited if os.environ.get(key)]
    check("no inherited Qt overrides", not present,
          "Unset process-local Qt/CMake overrides: " + ", ".join(present) + ". Do not change global user settings.")
    for cache in (root / "build/CMakeCache.txt", root / "build/native/CMakeCache.txt"):
        if not cache.is_file():
            continue
        try:
            contents = cache.read_text(encoding="utf-8", errors="replace")
        except OSError:
            contents = ""
        match = re.search(r"^CMAKE_HOME_DIRECTORY:[^=]*=(.*)$", contents, re.M)
        expected = (root / "native").resolve()
        match_arch = re.search(r"^CMAKE_SYSTEM_PROCESSOR:[^=]*=(.*)$", contents, re.M)
        same_source = bool(match and Path(match.group(1)).resolve() == expected)
        same_arch = not match_arch or match_arch.group(1).lower() in ("aarch64", "arm64")
        qt_cache = bool(re.search(r"^(Qt6_DIR|CMAKE_PREFIX_PATH):[^=]*=\S+", contents, re.M))
        check(f"cache:{cache.relative_to(root)}", same_source and same_arch and not qt_cache,
              "Stale/Qt-tainted CMake cache from another source or architecture. Owner: move the build directory aside; do not delete it automatically.")
    return checks, {"sha": sha if rc == 0 else None, "architecture": arch, "tools": tools}


def preflight(root: Path) -> int:
    checks, info = inspect(root)
    print(f"Pi source preflight (read-only): checkout={root} sha={info['sha']}")
    for name, ok, remedy in checks:
        print(f"{'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f" — {remedy}"))
    failed = sum(not ok for _, ok, _ in checks)
    print(f"Preflight: {len(checks) - failed} passed, {failed} blocked; no changes made.")
    return int(failed != 0)


def sanitize(text: str, root: Path) -> str:
    text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text)
    text = re.sub(r"(?i)(bearer\s+)\S+", r"\1<redacted>", text)
    text = re.sub(r"(?i)((?:_authToken|token|password|secret)\s*[=:]\s*)\S+", r"\1<redacted>", text)
    text = re.sub(r"\b(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{12,}\b", "<redacted>", text)
    text = re.sub(r"(https?://)[^\s/@]+:[^\s/@]+@", r"\1<redacted>@", text)
    text = text.replace(str(root), "<checkout>")
    home = str(Path.home())
    if home != str(root) and home != "/":
        text = text.replace(home, "<home>")
    return text


def stage_run(root: Path, report_dir: Path, stage: str, command: list[str], timeout: int) -> int:
    if stage not in STAGES or not command or any("--confirm-" in part for part in command):
        print("Invalid or device-confirming stage invocation", file=sys.stderr)
        return 2
    # No symlink or arbitrary external output: reports are generated, checkout-local only.
    expected = (root / "build_reports").resolve()
    if not expected.is_relative_to(root) or not report_dir.resolve().is_relative_to(expected):
        print("Report must live under checkout/build_reports", file=sys.stderr)
        return 2
    report_dir.mkdir(parents=True, exist_ok=True)
    log_path = report_dir / f"{stage}.log"
    start = time.monotonic()
    code = 127
    cwd = root / ("apps/studio" if stage.startswith("studio-") else "apps/desktop" if stage.startswith("desktop-") else ".")
    with log_path.open("w", encoding="utf-8") as log:
        try:
            with subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, start_new_session=True) as proc:
                deadline = start + timeout
                total = 0
                pending = ""
                with selectors.DefaultSelector() as selector:
                    selector.register(proc.stdout, selectors.EVENT_READ)
                    while selector.get_map():
                        remaining = deadline - time.monotonic()
                        if remaining <= 0:
                            os.killpg(proc.pid, signal.SIGKILL)
                            proc.wait()
                            code = 124
                            break
                        for key, _ in selector.select(timeout=min(remaining, 1.0)):
                            chunk = os.read(key.fileobj.fileno(), 4096)
                            if not chunk:
                                selector.unregister(key.fileobj)
                                continue
                            pending += chunk.decode("utf-8", errors="replace")
                            lines = pending.splitlines(keepends=True)
                            pending = "" if lines and lines[-1].endswith("\n") else lines.pop() if lines else ""
                            for line in lines:
                                if total < 3 * 1024 * 1024:
                                    safe = sanitize(line[:16384], root)
                                    log.write(safe)
                                    total += len(safe)
                            if len(pending) > 16384:
                                if total < 3 * 1024 * 1024:
                                    safe = sanitize(pending[:16384], root)
                                    log.write(safe)
                                    total += len(safe)
                                pending = pending[16384:]
                if pending and total < 3 * 1024 * 1024:
                    log.write(sanitize(pending, root))
                if code != 124:
                    code = proc.wait(timeout=3)
                else:
                    log.write("Stage timed out; owned process group terminated.\n")
        except OSError as exc:
            log.write(sanitize(f"Cannot start stage: {exc}\n", root))
    rc, sha = git(root, "rev-parse", "HEAD")
    if re.fullmatch(r"[0-9a-f]{40}", report_dir.name) and (rc != 0 or sha != report_dir.name):
        code = 1
        with log_path.open("a", encoding="utf-8") as log:
            log.write("Source HEAD changed during this stage; refusing provenance.\n")
    stage_file = report_dir / f"{stage}.json"
    stage_file.write_text(json.dumps({"stage": stage, "exit_code": code,
                                      "seconds": round(time.monotonic() - start, 2),
                                      "log": log_path.name}) + "\n", encoding="utf-8")
    results = {name: json.loads((report_dir / f"{name}.json").read_text())
               if (report_dir / f"{name}.json").is_file() else {"stage": name, "status": "not-run"}
               for name in STAGES}
    summary = {"source_sha": sha if rc == 0 else None, "architecture": platform.machine(),
               "tools": {k: v["version"] for k, v in versions().items()}, "stages": results}
    (report_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if code:
        print(f"FAILED {stage} (exit {code}); log: {log_path}; next: inspect log, fix prerequisite and rerun update-and-build.sh at exact SHA {sha}", file=sys.stderr)
    else:
        print(f"PASS {stage}; log: {log_path}")
    return code if code >= 0 else 128 - code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check-only", action="store_true")
    mode.add_argument("--run-stage", choices=STAGES)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    root = args.root.resolve()
    if args.check_only:
        if args.report_dir or args.command:
            parser.error("check-only takes no report or command")
        return preflight(root)
    if not args.report_dir or not (30 <= args.timeout <= 1200):
        parser.error("stage requires report-dir and timeout 30..1200")
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    return stage_run(root, args.report_dir, args.run_stage, command, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
