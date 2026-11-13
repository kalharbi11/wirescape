#!/usr/bin/env python3
"""Native build/evidence tools and the Electron development launcher."""
# SPDX-License-Identifier: GPL-3.0-only

from __future__ import annotations

import argparse
import json
import os
import sys
import shutil
import subprocess
import io
import unittest
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
from typing import Any

from foundation.helpers import (
    create_snapshot, discover_toolchain, git_revision, tool_version,
    utc_iso, utc_stamp, write_json, run_logged, source_provenance, sha256_file,
)

ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "build_reports" / "foundation"
BUILD_DIR = ROOT / "build" / "native"


def _build_metadata() -> dict[str, Any] | None:
    path = BUILD_DIR / "build-metadata.json"
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None

def _cache_values() -> dict[str, str]:
    values = {}
    try:
        for line in (BUILD_DIR / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace").splitlines():
            if line and not line.startswith("#") and not line.startswith("//") and "=" in line:
                left, value = line.split("=", 1)
                key = left.split(":", 1)[0]
                if key in {"CMAKE_BUILD_TYPE", "CMAKE_CONFIGURATION_TYPES", "CMAKE_GENERATOR", "CMAKE_HOME_DIRECTORY", "CMAKE_CXX_COMPILER", "CMAKE_GENERATOR_PLATFORM", "CMAKE_SYSTEM_VERSION", "CMAKE_TOOLCHAIN_FILE"} or key.startswith("CMAKE_CXX_FLAGS"):
                    values[key] = value
    except OSError:
        pass
    return values

def _artifact_inventory(config: str) -> dict[str, str]:
    """Hash native artifacts in multi-config and single-config CMake layouts."""
    names = {"wirescape_cli", "wirescape_engine", "wirescape-engine", "native_engine_tests", "native_module_tests", "audio_device_tests", "controller_failure_tests", "native_scale_offset_tests", "native_provider_bridge_tests", "native_lfo_tests", "returned_filter_stress", "essentials_noise_tests", "essentials_slew_tests", "essentials_function_envelope_tests", "essentials_clock_tests", "essentials_vcf_tests", "essentials_ladder_tests"}
    executable_suffixes = {".exe", ".dll", ".so", ".dylib"}
    result = {}
    for path in BUILD_DIR.rglob("*"):
        if not path.is_file() or path.name in {"build-metadata.json", "CMakeCache.txt"}:
            continue
        known_executable = path.stem in names and (path.suffix.lower() in executable_suffixes or not path.suffix)
        library = path.suffix.lower() in executable_suffixes and (path.name.startswith(("ws_", "libws_", "ws2_", "libws2_")) or "wirescape" in path.name)
        if known_executable or library:
            # Ignore binaries from another configuration where the path states one.
            if any(part in {"Debug", "Release"} for part in path.parts) and config not in path.parts and path.parent.name in {"Debug", "Release"}:
                continue
            result[path.relative_to(BUILD_DIR).as_posix()] = sha256_file(path)
    return result

def _refresh_build_metadata(config: str, expected_source: dict[str, Any] | None = None,
                            ui_built: bool = False, desktop_source: dict | None = None) -> tuple[dict[str, Any], bool]:
    current = source_provenance(ROOT)
    stable = expected_source is None or current["sha256"] == expected_source["sha256"]
    meta = _build_metadata() or {}
    engine = _sidecar_executable(config)
    keep_desktop = bool(engine and not ui_built and _launch_freshness_error(engine, config) is None)
    if ui_built:
        sources = _desktop_inventory()
        stable = stable and (desktop_source is None or sources == desktop_source)
        electron = _electron_executable()
        meta["desktop"] = {"sources": sources, "artifacts": _desktop_inventory(True),
                           "electron": sha256_file(electron) if electron else None}
        required = ("apps/desktop/build/main.js", "apps/desktop/build/preload.js", "apps/studio/dist/index.html")
        keep_desktop = bool(electron and all(name in meta["desktop"]["artifacts"] for name in required))
    for key in ("qt_prefix", "ui_verified"):
        meta.pop(key, None)
    meta.update(configuration=config, source=current, source_stable=stable,
                cmake_cache=_cache_values(), desktop_verified=bool(keep_desktop and stable),
                artifact_root=str(BUILD_DIR), binaries=_artifact_inventory(config),
                build_commit=git_revision(ROOT), source_root=str(ROOT.resolve()))
    write_json(BUILD_DIR / "build-metadata.json", meta)
    return meta, stable

def _base_record(action: str, config: str | None = None) -> dict[str, Any]:
    return {"schema_version": 1, "action": action, "created_at": utc_iso(),
            "repository": str(ROOT), "commit": git_revision(ROOT), "source": source_provenance(ROOT),
            "build_metadata": _build_metadata(), "configuration": config,
            "python": {"version": sys.version.split()[0], "executable": str(Path(sys.executable).resolve()),
                       "implementation": sys.implementation.name},
            "platform": {"system": sys.platform, "architecture": __import__("platform").machine()}}


def _save_record(record: dict[str, Any], log: str | None = None) -> Path:
    path = _unique_report_path(record["action"], ".json")
    if log and record.get("command_result"):
        result_log = Path(record["command_result"]["log"])
        if result_log.exists():
            record["command_result"]["log"] = str(result_log.resolve())
    write_json(path, record)
    return path

def _unique_report_path(stem: str, suffix: str) -> Path:
    stamp = utc_stamp()
    path = REPORT_DIR / f"{stem}-{stamp}{suffix}"
    serial = 1
    while path.exists():
        path = REPORT_DIR / f"{stem}-{stamp}-{serial}{suffix}"
        serial += 1
    return path

def _print_record(record: dict[str, Any], path: Path, as_json: bool = False) -> None:
    if as_json:
        print(json.dumps(record, indent=2, sort_keys=True))
        return
    status = record.get("status", "unknown").upper()
    print(f"{record['action']}: {status}")
    if record.get("summary"):
        print(record["summary"])
    print(f"Report: {path}")
    if record.get("command_result", {}).get("log"):
        print(f"Full log: {record['command_result']['log']}")

def command_doctor(args: argparse.Namespace) -> int:
    record = _base_record("doctor")
    discovered = discover_toolchain()
    record["tools"] = {
        "cmake": tool_version(discovered["cmake"]),
        "generator": discovered["generator"],
        "architecture": discovered["architecture"],
        "ninja": tool_version(discovered["ninja"]) if discovered["ninja"] else None,
        "make": tool_version(discovered["make"], ("--version",)) if discovered["make"] else None,
        "visual_studio": discovered["visual_studio"],
    }
    record["tools"]["node"] = tool_version(shutil.which("node"))
    record["tools"]["npm_cli"] = _node_npm()[1]
    record["tools"]["electron"] = str(_electron_executable() or "not installed")
    record["native_project"] = {
        "cmake_file": str(ROOT / "native" / "CMakeLists.txt"),
        "present": (ROOT / "native" / "CMakeLists.txt").is_file(),
    }
    issues = []
    if not discovered["cmake"]:
        issues.append("CMake was not found")
    if not discovered["generator"]:
        issues.append("No supported CMake generator was detected")
    if not record["native_project"]["present"]:
        issues.append("Native CMake project has not been added yet")
    record["status"] = "ready" if not issues else "pending"
    record["issues"] = issues
    record["summary"] = "; ".join(issues) if issues else f"CMake and {discovered['generator']} are available"
    return _finish(record, args, 0 if not issues else 1)

def _cmake_and_generator() -> tuple[str | None, str | None, str | None]:
    toolchain = discover_toolchain()
    return toolchain["cmake"], toolchain["generator"], toolchain["architecture"]

def command_build(args: argparse.Namespace) -> int:
    record = _base_record("build", args.config)
    source_before = record["source"]
    desktop_before = _desktop_inventory() if not args.headless else None
    cmake, generator, architecture = _cmake_and_generator()
    if not cmake or not generator or not (ROOT / "native/CMakeLists.txt").is_file():
        record.update(status="skipped", summary="Build skipped: native project or CMake toolchain unavailable")
        return _finish(record, args, 2)
    previous = _build_metadata()
    if isinstance(previous, dict):
        previous["desktop_verified"] = False
        write_json(BUILD_DIR / "build-metadata.json", previous)
    configure = [cmake, "-S", str(ROOT / "native"), "-B", str(BUILD_DIR), "-G", generator]
    configure += ["-A", "x64"] if generator.startswith("Visual Studio") else [f"-DCMAKE_BUILD_TYPE={args.config}"]
    # Cloud fallback: tools/cloud/git_deps.py writes this when codeload is blocked.
    git_deps_cache = ROOT / "build" / "deps-cache" / "fetchcontent.cmake"
    if git_deps_cache.is_file():
        configure[1:1] = ["-C", str(git_deps_cache)]
    steps = [{"name": "configure", **run_logged(configure, ROOT, _unique_report_path("build-configure", ".log"))}]
    if steps[-1]["exit_code"] == 0:
        command = [cmake, "--build", str(BUILD_DIR), "--config", args.config, "--parallel"]
        if os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL"):
            command.append(os.environ["CMAKE_BUILD_PARALLEL_LEVEL"])
        steps.append({"name": "build", **run_logged(command, ROOT, _unique_report_path("build-compile", ".log"))})
    if steps[-1]["exit_code"] == 0 and not args.headless:
        node, npm = _node_npm()
        result = run_logged([node, npm, "run", "build"], ROOT / "apps/desktop", _unique_report_path("build-electron", ".log")) if node and npm else {"exit_code": 2, "error": "Install Node/npm and run npm ci in apps/studio and apps/desktop"}
        steps.append({"name": "electron_build", **result})
    failed = next((stage for stage in steps if stage["exit_code"]), None)
    if not failed:
        metadata, stable = _refresh_build_metadata(args.config, source_before, not args.headless, desktop_before)
        record["build_metadata"] = metadata
        if not stable or (not args.headless and not metadata["desktop_verified"]):
            failed = {"name": "source_or_artifacts", "exit_code": 1, "error": "Build inputs changed or Electron artifacts are missing"}
            steps.append(failed)
        metadata.update(configure_command=configure)
        write_json(BUILD_DIR / "build-metadata.json", metadata)
    record.update(steps=steps, command_result=steps[-1], tools={"cmake": cmake, "generator": generator, "architecture": architecture},
                  status="failed" if failed else "passed", summary=f"{failed['name']} failed" if failed else f"Built {args.config}" )
    return _finish(record, args, failed["exit_code"] if failed else 0)

def command_test(args: argparse.Namespace) -> int:
    record = _base_record("test", args.config)
    source_before = record["source"]
    cmake = discover_toolchain()["cmake"]
    cache = BUILD_DIR / "CMakeCache.txt"
    if not cmake or not cache.is_file():
        record.update(status="skipped", summary="Tests skipped: configure and build the native project first")
        return _finish(record, args, 2)
    cmd = [cmake, "--build", str(BUILD_DIR), "--config", args.config]
    build_result = run_logged(cmd, ROOT, _unique_report_path("test-build", ".log"))
    steps = [{"name": "ensure_build", **build_result}]
    if build_result["exit_code"] == 0:
        metadata, stable = _refresh_build_metadata(args.config, source_before)
        record["build_metadata"] = metadata
        if not stable:
            steps.append({"name": "source_stability", "exit_code": 1,
                          "error": "source changed during ensure-build; test snapshot is unstable"})
        else:
            ctest_exe = discover_toolchain()["ctest"]
            if ctest_exe:
                ctest = [ctest_exe, "--test-dir", str(BUILD_DIR), "-C", args.config, "--output-on-failure", "--timeout", "60"]
                test_result = run_logged(ctest, ROOT, _unique_report_path("test-ctest", ".log"))
                steps.append({"name": "ctest", **test_result})
                import re
                text = Path(test_result["log"]).read_text(encoding="utf-8", errors="replace")
                failed_output = False
                for line in text.splitlines():
                    if re.search(r"\bTest\s+#\d+:", line):
                        print(line.strip())
                        failed_output = "***" in line
                    elif failed_output and line.strip() and not line.startswith(("Total Test time", "The following tests")):
                        # --output-on-failure text of a failed test, so CI logs show the real cause.
                        print("    " + line.rstrip()[:500])

            else:
                record.update(status="skipped", summary="Tests skipped: ctest was not found")
                return _finish(record, args, 2)
            # Metadata is refreshed after ensure-build and before CTest; the inventory
            # now names the actual binaries used by this test run.
    record["steps"] = steps
    failed = next((s for s in steps if s["exit_code"] != 0), None)
    record["status"] = "passed" if not failed else "failed"
    record["summary"] = "CTest passed" if not failed else f"{failed['name']} failed (exit {failed['exit_code']})"
    record["command_result"] = steps[-1]
    return _finish(record, args, 0 if not failed else failed["exit_code"])

def command_snapshot(args: argparse.Namespace) -> int:
    record = _base_record("snapshot")
    backup_root = ROOT / "local_backups"
    try:
        snapshot = create_snapshot(ROOT, backup_root, args.label, record["commit"])
    except FileExistsError as exc:
        record.update(status="failed", summary=f"Snapshot refused: {exc}")
        return _finish(record, args, 1)
    record.update(status="passed", summary=f"Saved {snapshot['file_count']} private file(s) to {snapshot['archive']}",
                  **snapshot)
    return _finish(record, args, 0)

def _latest_reports() -> list[dict[str, Any]]:
    items = []
    seen = set()
    for path in sorted(REPORT_DIR.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if isinstance(data, dict) and "action" in data and "status" in data and data["action"] != "report":
            key = (data["action"], data.get("source", {}).get("sha256"), data.get("configuration"))
            if key not in seen:
                seen.add(key)
                items.append(data)
    return items

def command_report(args: argparse.Namespace) -> int:
    recent = _latest_reports()
    record = _base_record("report")
    record["latest_actions"] = recent[:20]
    current_hash = record["source"]["sha256"]
    record["current_source_sha256"] = current_hash
    record["current_checkpoint_actions"] = [item for item in recent if item.get("source", {}).get("sha256") == current_hash]
    record["historical_actions"] = [item for item in recent if item.get("source", {}).get("sha256") != current_hash]
    counts: dict[str, int] = {}
    for item in recent:
        key = item["status"]
        counts[key] = counts.get(key, 0) + 1
    record["status_counts"] = counts
    record["status"] = "ready" if recent else "pending"
    record["summary"] = (f"Collected {len(recent)} recent foundation report(s)" if recent
                         else "No foundation reports have been recorded")
    path = _save_record(record)
    if args.json:
        _print_record(record, path, True)
    else:
        print(record["summary"])
        print(f"Current checkpoint actions: {len(record['current_checkpoint_actions'])}")
        print(f"Historical actions: {len(record['historical_actions'])}")
        for key, count in sorted(counts.items()):
            print(f"{key}: {count}")
        print(f"Report: {path}")
    return 0

def command_pending(action: str, args: argparse.Namespace) -> int:
    record = _base_record(action, getattr(args, "config", None))
    record.update(status="pending", summary=f"{action.capitalize()} integration awaits the native CLI contract")
    return _finish(record, args, 2)

def _native_cli(config: str) -> Path | None:
    suffix = ".exe" if os.name == "nt" else ""
    for folder in (BUILD_DIR / config, BUILD_DIR):
        candidate = folder / f"wirescape_cli{suffix}"
        if candidate.is_file():
            return candidate
    return None

def command_benchmark(args: argparse.Namespace) -> int:
    record = _base_record("benchmark", args.config)
    executable = _native_cli(args.config)
    if not executable:
        record.update(status="skipped", summary="Benchmark skipped: build the native CLI first")
        return _finish(record, args, 2)
    output = _unique_report_path("benchmark-result", ".json")
    modules = BUILD_DIR / args.config if os.name == "nt" or (BUILD_DIR / args.config).is_dir() else BUILD_DIR
    cmd = [str(executable), "benchmark", "--modules", str(modules), "--json", str(output),
           "--blocks", str(args.blocks)]
    result = run_logged(cmd, ROOT, _unique_report_path("benchmark", ".log"))
    record["command_result"] = result
    record["benchmark_json"] = str(output) if output.is_file() else None
    if output.is_file():
        try:
            record["benchmark"] = json.loads(output.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            record["benchmark_parse_error"] = str(exc)
    success = result["exit_code"] == 0 and "benchmark" in record
    record["status"] = "passed" if success else "failed"
    record["summary"] = f"Benchmark recorded {args.blocks} blocks" if success else f"Benchmark failed (exit {result['exit_code']})"
    return _finish(record, args, 0 if success else (result["exit_code"] or 1))

def command_render(args: argparse.Namespace) -> int:
    record = _base_record("render", args.config)
    executable = _native_cli(args.config)
    if not executable:
        record.update(status="skipped", summary="Render skipped: build the native CLI first")
        return _finish(record, args, 2)
    modules = BUILD_DIR / args.config if os.name == "nt" or (BUILD_DIR / args.config).is_dir() else BUILD_DIR
    output = Path(args.output).expanduser().resolve() if args.output else _unique_report_path("rendered-demo", ".wav")
    if output.exists():
        record.update(status="failed", summary=f"Render refused: output already exists ({output})")
        return _finish(record, args, 1)
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(executable), "render", "--modules", str(modules), "--output", str(output)]
    result = run_logged(cmd, ROOT, _unique_report_path("render", ".log"))
    record["command_result"] = result
    record["render_output"] = str(output) if output.is_file() else None
    success = result["exit_code"] == 0 and output.is_file()
    record["status"] = "passed" if success else "failed"
    record["summary"] = (f"Rendered offline WAV to {output}" if success
                         else f"Offline render failed (exit {result['exit_code']})")
    return _finish(record, args, 0 if success else (result["exit_code"] or 1))

def _launch_freshness_error(executable: Path, config: str) -> str | None:
    """Never launch stale or unrecorded native/Electron artifacts."""
    meta = _build_metadata()
    if not isinstance(meta, dict):
        return "missing or corrupt build metadata"
    if meta.get("configuration") != config:
        return "requested configuration differs from the recorded build"
    if meta.get("desktop_verified") is not True or meta.get("source_stable") is not True:
        return "no successful Electron build for this configuration"
    try:
        if source_provenance(ROOT)["sha256"] != meta["source"]["sha256"]:
            return "native/tool source changed since the build"
        if _cache_values() != meta["cmake_cache"]:
            return "native build configuration changed since the build"
        if (_artifact_inventory(config) != meta["binaries"] or
                executable.relative_to(BUILD_DIR).as_posix() not in meta["binaries"]):
            return "native artifacts changed or are missing since the build"
        desktop = meta["desktop"]
        if _desktop_inventory() != desktop["sources"] or _desktop_inventory(True) != desktop["artifacts"]:
            return "Electron source or artifacts changed since the build"
        electron = _electron_executable()
        if not electron or sha256_file(electron) != desktop["electron"]:
            return "Electron executable changed or is missing"
    except (OSError, ValueError, KeyError, TypeError):
        return "build provenance could not be verified"
    return None

def command_launch(args: argparse.Namespace) -> int:
    record = _base_record("launch", args.config)
    engine, electron = _sidecar_executable(args.config), _electron_executable()
    problem = _launch_freshness_error(engine, args.config) if engine and electron else "engine or Electron is missing"
    if problem:
        record.update(status="failed", summary=f"Launch refused: {problem}. Run: python tools/dev.py build --config {args.config}")
        return _finish(record, args, 2)
    consent = [flag for flag, allowed in (("--confirm-audio", args.confirm_audio), ("--confirm-midi", args.confirm_midi)) if allowed]
    command = [str(electron), str(ROOT / "apps/desktop"), "--engine", str(engine), *(consent or ["--null-audio"])]
    log = _unique_report_path("electron-launch", ".log")
    log.parent.mkdir(parents=True, exist_ok=True)
    try:
        with log.open("w", encoding="utf-8") as stream:
            process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                                       stdout=stream, stderr=subprocess.STDOUT, start_new_session=os.name != "nt")
        code = process.poll()
        record.update(status="started" if code is None else "failed", process_id=process.pid,
                      summary="Electron started; GUI readiness is not checked" if code is None else f"Electron exited ({code})",
                      device_confirmations=consent, command_result={"command": command, "log": str(log), "exit_code": code})
        return _finish(record, args, 0 if code is None else (code or 1))
    except OSError as error:
        record.update(status="failed", summary=f"Could not start Electron: {error}")
        return _finish(record, args, 1)

def command_check(args: argparse.Namespace) -> int:
    """Run one bounded, sequential checkpoint without launching the desktop app."""
    record = _base_record("check", args.config)
    stages: list[tuple[str, Any]] = []
    test_log = _unique_report_path("check-python-tests", ".log")
    stream = io.StringIO()
    suite = unittest.defaultTestLoader.discover(str(ROOT / "tools" / "foundation" / "tests"))
    with redirect_stdout(stream), redirect_stderr(stream):
        result = unittest.TextTestRunner(stream=stream, verbosity=2, buffer=True).run(suite)
    test_log.parent.mkdir(parents=True, exist_ok=True)
    test_log.write_text(stream.getvalue(), encoding="utf-8")
    stages.append(("python_tests", 0 if result.wasSuccessful() else 1))
    functional_only = bool(getattr(args, "functional_only", False))
    plan = [
        ("build", command_build, argparse.Namespace(config=args.config, headless=True, json=False)),
        ("test", command_test, argparse.Namespace(config=args.config, json=False)),
        ("benchmark", None if functional_only else command_benchmark,
         argparse.Namespace(config=args.config, blocks=256, json=False)),
        ("render", command_render, argparse.Namespace(config=args.config, output=None, json=False)),
    ]
    artifacts = [{"name": "python_tests", "log": str(test_log), "exit_code": stages[0][1],
                  "tests_run": result.testsRun, "failures": len(result.failures) + len(result.errors)}]
    if stages[0][1] == 0:
        for name, function, stage_args in plan:
            if function is None:
                artifacts.append({"name": name, "status": "deferred",
                                  "reason": "--functional-only selected; performance benchmarking intentionally deferred"})
                continue
            before = {p.name for p in REPORT_DIR.glob(f"{name.replace('_', '-')}-*.json")}
            output = io.StringIO()
            with redirect_stdout(output):
                code = function(stage_args)
            candidates = [p for p in REPORT_DIR.glob(f"{name.replace('_', '-')}-*.json") if p.name not in before]
            report_path = max(candidates, key=lambda p: p.stat().st_mtime, default=None)
            stage = {"name": name, "exit_code": code, "report": str(report_path) if report_path else None}
            if report_path:
                try:
                    stage["log"] = json.loads(report_path.read_text(encoding="utf-8")).get("command_result", {}).get("log")
                except (OSError, json.JSONDecodeError):
                    pass
            artifacts.append(stage)
            if code:
                for pending, _, _ in plan[len(artifacts) - 1:]:
                    artifacts.append({"name": pending, "status": "blocked", "reason": f"stopped after {name} failure"})
                break
    else:
        artifacts.extend({"name": name, "status": "blocked", "reason": "Python helper tests failed"}
                         for name, _, _ in plan)
    failed = next((item for item in artifacts if item.get("exit_code", 0) != 0), None)
    if failed:
        summary = f"Checkpoint stopped at {failed['name']}"
    elif functional_only:
        summary = "Functional checkpoint completed; performance benchmark deferred (no performance result recorded)"
    else:
        summary = "Checkpoint passed"
    record.update(status="failed" if failed else "passed", stages=artifacts, summary=summary)
    path = _save_record(record)
    markdown = path.with_suffix(".md")
    lines = [f"# Wirescape checkpoint: {record['status']}", "", record["summary"], "",
             f"Configuration: {args.config}", "", "| Stage | Result | Report / log |", "|---|---|---|"]
    for item in artifacts:
        location = item.get("report") or item.get("log") or "—"
        state = item.get("status", "passed" if item.get("exit_code") == 0 else "failed")
        if "exit_code" in item and item["exit_code"] == 0:
            state = "passed"
        lines.append(f"| {item['name']} | {state} | `{location}` |")
    markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")
    record["markdown_summary"] = str(markdown)
    write_json(path, record)
    _print_record(record, path, args.json)
    return 0 if not failed else int(failed.get("exit_code") or 1)

def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Wirescape native foundation tools")
    sub = parser.add_subparsers(dest="action", required=True)
    from foundation.acceptance import run as run_acceptance
    child = sub.add_parser("accept", help="run a task's local acceptance commands")
    child.add_argument("issue", type=int, nargs="?")
    child.add_argument("--file", help="explicit task file instead of issue discovery")
    child.add_argument("--base", default="origin/main", help="diff base ref for size checks (default: origin/main)")
    child.add_argument("--timeout", type=float, default=600, help="seconds per command")
    child.add_argument("--json", action="store_true", help="print only the JSON summary")
    child.add_argument("--strict", action="store_true", help="exit nonzero on incomplete external/pending evidence")
    child.set_defaults(func=lambda args: run_acceptance(ROOT, args))
    for name, func in (("doctor", command_doctor), ("snapshot", command_snapshot), ("report", command_report)):
        child = sub.add_parser(name)
        child.add_argument("--json", action="store_true", help="print the full result as JSON")
        if name == "snapshot":
            child.add_argument("--label", default="private-data", help="short label for the backup filename")
        child.set_defaults(func=func)
    child = sub.add_parser("check", help="run the bounded sequential local checkpoint")
    child.add_argument("--config", choices=("Debug", "Release"), default="Release")
    child.add_argument("--functional-only", action="store_true",
                       help="defer performance benchmarking while running functional checks")
    child.add_argument("--json", action="store_true", help="print the full result as JSON")
    child.set_defaults(func=command_check)
    for name, func in (("build", command_build), ("test", command_test)):
        child = sub.add_parser(name)
        child.add_argument("--config", choices=("Debug", "Release"), default="Release")
        if name == "build":
            child.add_argument("--headless", action="store_true", help="build native artifacts only")
        child.add_argument("--json", action="store_true", help="print the full result as JSON")
        child.set_defaults(func=func)
    child = sub.add_parser("benchmark")
    child.add_argument("--config", choices=("Debug", "Release"), default="Release")
    child.add_argument("--blocks", type=int, default=256, help="bounded workload size (default: 256)")
    child.add_argument("--json", action="store_true", help="print the full result as JSON")
    child.set_defaults(func=command_benchmark)
    child = sub.add_parser("render")
    child.add_argument("--config", choices=("Debug", "Release"), default="Release")
    child.add_argument("--output", help="WAV destination; must not already exist")
    child.add_argument("--json", action="store_true", help="print the full result as JSON")
    child.set_defaults(func=command_render)
    child = sub.add_parser("launch", help="launch Electron with devices off unless explicitly confirmed")
    child.add_argument("--config", choices=("Debug", "Release"), default="Release")
    child.add_argument("--json", action="store_true")
    child.add_argument("--confirm-audio", action="store_true")
    child.add_argument("--confirm-midi", action="store_true")
    child.set_defaults(func=command_launch)
    return parser

def main(argv: list[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    return args.func(args)

def _finish(record: dict, args: argparse.Namespace, code: int) -> int:
    _print_record(record, _save_record(record), args.json)
    return code


def _desktop_inventory(built: bool = False) -> dict[str, str]:
    folders = (ROOT / "apps/desktop" / "build", ROOT / "apps/studio" / "dist") if built else (ROOT / "apps/desktop", ROOT / "apps/studio")
    excluded = {"node_modules", "build", "dist", "build_reports", "test-results", "__pycache__"}
    return {path.relative_to(ROOT).as_posix(): sha256_file(path)
            for folder in folders for path in sorted(folder.rglob("*")) if path.is_file()
            and (built or not excluded.intersection(path.relative_to(folder).parts))}


def _sidecar_executable(config: str) -> Path | None:
    name = "wirescape-engine" + (".exe" if os.name == "nt" else "")
    return next((folder / name for folder in (BUILD_DIR / "sidecar" / config, BUILD_DIR / "sidecar", BUILD_DIR / config, BUILD_DIR)
                 if (folder / name).is_file()), None)


def _electron_executable() -> Path | None:
    root = ROOT / "apps/desktop/node_modules/electron/dist"
    try:
        name = (root.parent / "path.txt").read_text(encoding="utf-8").strip()
        path = (root / name).resolve()
        return path if name and path.is_relative_to(root.resolve()) and path.is_file() else None
    except (OSError, ValueError):
        return None


def _node_npm() -> tuple[str | None, str | None]:
    node, npm = shutil.which("node"), shutil.which("npm")
    candidates = [Path(npm).resolve()] if npm else []
    if node:
        candidates.append(Path(node).parent / "node_modules/npm/bin/npm-cli.js")
    if npm:
        candidates.append(Path(npm).parent / "node_modules/npm/bin/npm-cli.js")
    cli = next((str(path) for path in candidates if path.name == "npm-cli.js" and path.is_file()), None)
    return node, cli

if __name__ == "__main__":
    raise SystemExit(main())
