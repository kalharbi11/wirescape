#!/usr/bin/env python3
"""Small standard-library command runner for the Wirescape native foundation."""

from __future__ import annotations

import argparse
import json
import os
import sys
import subprocess
import shutil
import io
import unittest
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
from typing import Any

from foundation.helpers import (
    create_snapshot, discover_toolchain, git_revision, tool_version,
    utc_iso, utc_stamp, write_json, run_logged, source_provenance, sha256_file,
    wait_for_readiness,
)
from foundation.package_inventory import inspect_package

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
                if key in {"Qt6_DIR", "CMAKE_PREFIX_PATH", "CMAKE_BUILD_TYPE", "CMAKE_CONFIGURATION_TYPES", "CMAKE_GENERATOR", "CMAKE_HOME_DIRECTORY", "CMAKE_CXX_COMPILER", "CMAKE_GENERATOR_PLATFORM", "CMAKE_SYSTEM_VERSION", "CMAKE_TOOLCHAIN_FILE"} or key.startswith("CMAKE_CXX_FLAGS"):
                    values[key] = value
    except OSError:
        pass
    return values


def _artifact_inventory(config: str) -> dict[str, str]:
    """Hash native artifacts in multi-config and single-config CMake layouts."""
    names = {"wirescape_studio", "wirescape_cli", "wirescape_engine", "native_engine_tests", "native_module_tests", "studio_ui_tests", "audio_device_tests", "controller_failure_tests", "document_reader_tests", "native_scale_offset_tests", "native_provider_bridge_tests", "native_lfo_tests", "returned_filter_stress", "essentials_noise_tests", "essentials_slew_tests", "essentials_function_envelope_tests", "essentials_clock_tests", "essentials_vcf_tests", "essentials_ladder_tests"}
    executable_suffixes = {".exe", ".dll", ".so", ".dylib"}
    result = {}
    for path in BUILD_DIR.rglob("*"):
        if not path.is_file() or path.name in {"build-metadata.json", "CMakeCache.txt"}:
            continue
        known_executable = path.stem in names and (path.suffix.lower() in executable_suffixes or not path.suffix)
        library = path.suffix.lower() in executable_suffixes and (path.name.startswith(("ws_", "libws_")) or "wirescape" in path.name)
        if known_executable or library:
            # Ignore binaries from another configuration where the path states one.
            if any(part in {"Debug", "Release"} for part in path.parts) and config not in path.parts and path.parent.name in {"Debug", "Release"}:
                continue
            result[path.relative_to(BUILD_DIR).as_posix()] = sha256_file(path)
    return result


def _refresh_build_metadata(config: str, expected_source: dict[str, Any] | None = None) -> tuple[dict[str, Any], bool]:
    current = source_provenance(ROOT)
    stable = expected_source is None or current["sha256"] == expected_source["sha256"]
    cache = _cache_values()
    qt_dir = cache.get("Qt6_DIR")
    qt_path = Path(qt_dir).resolve() if qt_dir and "NOTFOUND" not in qt_dir else None
    qt_prefix = str(qt_path.parents[2]) if qt_path and qt_path.is_dir() and len(qt_path.parents) >= 3 else None
    meta = _build_metadata() or {}
    meta.update(configuration=config, source=current, source_stable=stable,
                cmake_cache=cache, qt_prefix=qt_prefix or meta.get("qt_prefix"),
                artifact_root=str(BUILD_DIR), binaries=_artifact_inventory(config))
    write_json(BUILD_DIR / "build-metadata.json", meta)
    return meta, stable


def _base_record(action: str, config: str | None = None) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "action": action,
        "created_at": utc_iso(),
        "repository": str(ROOT),
        "commit": git_revision(ROOT),
        "source": source_provenance(ROOT),
        "build_metadata": _build_metadata(),
        "configuration": config,
        "python": {
            "version": sys.version.split()[0],
            "executable": str(Path(sys.executable).resolve()),
            "implementation": sys.implementation.name,
        },
        "platform": {"system": sys.platform, "architecture": __import__("platform").machine()},
    }


def _save_record(record: dict[str, Any], log: str | None = None) -> Path:
    stamp = utc_stamp()
    path = REPORT_DIR / f"{record['action']}-{stamp}.json"
    # Keep filenames unique if two invocations start within the same second.
    suffix = 1
    while path.exists():
        path = REPORT_DIR / f"{record['action']}-{stamp}-{suffix}.json"
        suffix += 1
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
    report = _save_record(record)
    _print_record(record, report, args.json)
    return 0 if not issues else 1


def _cmake_and_generator() -> tuple[str | None, str | None, str | None]:
    toolchain = discover_toolchain()
    return toolchain["cmake"], toolchain["generator"], toolchain["architecture"]


def _qt_prefix(explicit: str | None) -> Path | None:
    candidates = [Path(explicit).expanduser().resolve()] if explicit else []
    if not explicit:
        local_qt = os.environ.get("WIRESCAPE_QT_DIR")
        if local_qt:
            candidates.append(Path(local_qt).expanduser().resolve())
        candidates.extend(Path(value).expanduser().resolve()
                          for value in os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep) if value)
        candidates.append(ROOT / ".tools/qt/6.10.3/msvc2022_64")
    for candidate in candidates:
        if (candidate / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
            return candidate
    return None


def command_build(args: argparse.Namespace) -> int:
    record = _base_record("build", args.config)
    source_before = record["source"]
    cmake, generator, architecture = _cmake_and_generator()
    cmake_file = ROOT / "native" / "CMakeLists.txt"
    if not cmake or not generator or not cmake_file.is_file():
        missing = "CMake" if not cmake else "a supported CMake generator" if not generator else "native/CMakeLists.txt"
        record.update(status="skipped", summary=f"Build skipped: {missing} is unavailable")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
    configure = [cmake, "-S", str(ROOT / "native"), "-B", str(BUILD_DIR), "-G", generator]
    if generator.startswith("Visual Studio"):
        configure.extend(["-A", "x64"])
    if not generator.startswith("Visual Studio"):
        configure.append(f"-DCMAKE_BUILD_TYPE={args.config}")
    qt_prefix = _qt_prefix(args.qt_prefix)
    if qt_prefix:
        configure.append(f"-DCMAKE_PREFIX_PATH={qt_prefix}")
        configure.append(f"-DQt6_DIR={qt_prefix / 'lib/cmake/Qt6'}")
    elif args.qt_prefix:
        record.update(status="failed", summary=f"Qt prefix does not contain lib/cmake/Qt6/Qt6Config.cmake: {args.qt_prefix}")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
    configure.append(f"-DWIRESCAPE_BUILD_UI={'OFF' if args.headless else 'ON'}")
    env = os.environ.copy()
    configure_result = run_logged(configure, ROOT, _unique_report_path("build-configure", ".log"), env)
    steps = [{"name": "configure", **configure_result}]
    if configure_result["exit_code"] == 0:
        build = [cmake, "--build", str(BUILD_DIR), "--config", args.config, "--parallel"]
        build_result = run_logged(build, ROOT, _unique_report_path("build-compile", ".log"), env)
        steps.append({"name": "build", **build_result})
    record["steps"] = steps
    failed = next((s for s in steps if s["exit_code"] != 0), None)
    metadata = None
    if not failed:
        metadata, stable = _refresh_build_metadata(args.config, source_before)
        if not stable:
            failed = {"name": "source_stability", "exit_code": 1}
            steps.append({"name": "source_stability", "exit_code": 1,
                          "error": "source changed during build; output is not a stable snapshot"})
    record["tools"] = {"cmake": cmake, "generator": generator, "architecture": architecture,
                       "qt_prefix": str(qt_prefix) if qt_prefix else None,
                       "ui_requested": not args.headless,
                       "ui_built": not failed and not args.headless and (BUILD_DIR / "ui" / "wirescape-ui-enabled.txt").is_file() and _studio_executable(args.config) is not None}
    record["status"] = "passed" if not failed else "failed"
    record["summary"] = (f"Configured and built {args.config} with {generator}" if not failed
                         else f"{failed['name']} failed (exit {failed['exit_code']})")
    record["command_result"] = steps[-1]
    if metadata:
        metadata.update(configure_command=configure, tools=record["tools"])
        write_json(BUILD_DIR / "build-metadata.json", metadata)
        record["build_metadata"] = metadata
    report = _save_record(record)
    _print_record(record, report, args.json)
    return 0 if not failed else failed["exit_code"]


def command_test(args: argparse.Namespace) -> int:
    record = _base_record("test", args.config)
    source_before = record["source"]
    cmake = discover_toolchain()["cmake"]
    cache = BUILD_DIR / "CMakeCache.txt"
    if not cmake or not cache.is_file():
        record.update(status="skipped", summary="Tests skipped: configure and build the native project first")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
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
                for line in text.splitlines():
                    if re.search(r"\bTest\s+#\d+:", line):
                        print(line.strip())

            else:
                record.update(status="skipped", summary="Tests skipped: ctest was not found")
                report = _save_record(record)
                _print_record(record, report, args.json)
                return 2
            # Metadata is refreshed after ensure-build and before CTest; the inventory
            # now names the actual binaries used by this test run.
    record["steps"] = steps
    failed = next((s for s in steps if s["exit_code"] != 0), None)
    record["status"] = "passed" if not failed else "failed"
    record["summary"] = "CTest passed" if not failed else f"{failed['name']} failed (exit {failed['exit_code']})"
    record["command_result"] = steps[-1]
    report = _save_record(record)
    _print_record(record, report, args.json)
    return 0 if not failed else failed["exit_code"]


def command_snapshot(args: argparse.Namespace) -> int:
    record = _base_record("snapshot")
    backup_root = ROOT / "local_backups"
    try:
        snapshot = create_snapshot(ROOT, backup_root, args.label, record["commit"])
    except FileExistsError as exc:
        record.update(status="failed", summary=f"Snapshot refused: {exc}")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 1
    record.update(status="passed", summary=f"Saved {snapshot['file_count']} private file(s) to {snapshot['archive']}",
                  **snapshot)
    report = _save_record(record)
    _print_record(record, report, args.json)
    return 0


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
    path = _save_record(record)
    _print_record(record, path, args.json)
    return 2


def _native_cli(config: str) -> Path | None:
    suffix = ".exe" if os.name == "nt" else ""
    for folder in (BUILD_DIR / config, BUILD_DIR):
        candidate = folder / f"wirescape_cli{suffix}"
        if candidate.is_file():
            return candidate
    return None


def _studio_executable(config: str) -> Path | None:
    suffix = ".exe" if os.name == "nt" else ""
    for folder in (BUILD_DIR / config, BUILD_DIR / "ui" / config, BUILD_DIR):
        candidate = folder / f"wirescape_studio{suffix}"
        if candidate.is_file():
            return candidate
    return None


def _qt_runtime_env(executable: Path) -> dict[str, str]:
    env = os.environ.copy()
    metadata = _build_metadata()
    prefix = Path(metadata["qt_prefix"]) if metadata and metadata.get("qt_prefix") else None
    app_dir = executable.parent
    if prefix:
        bin_dir = prefix / "bin"
        plugins_dir = prefix / "plugins"
        qml_dir = prefix / "qml"
        env["PATH"] = os.pathsep.join([str(bin_dir), str(app_dir), env.get("PATH", "")])
        if plugins_dir.is_dir():
            env["QT_PLUGIN_PATH"] = str(plugins_dir)
        if qml_dir.is_dir():
            env["QML2_IMPORT_PATH"] = str(qml_dir)
    return env


def command_benchmark(args: argparse.Namespace) -> int:
    record = _base_record("benchmark", args.config)
    executable = _native_cli(args.config)
    if not executable:
        record.update(status="skipped", summary="Benchmark skipped: build the native CLI first")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
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
    path = _save_record(record)
    _print_record(record, path, args.json)
    return 0 if success else (result["exit_code"] or 1)


def command_render(args: argparse.Namespace) -> int:
    record = _base_record("render", args.config)
    executable = _native_cli(args.config)
    if not executable:
        record.update(status="skipped", summary="Render skipped: build the native CLI first")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
    modules = BUILD_DIR / args.config if os.name == "nt" or (BUILD_DIR / args.config).is_dir() else BUILD_DIR
    output = Path(args.output).expanduser().resolve() if args.output else _unique_report_path("rendered-demo", ".wav")
    if output.exists():
        record.update(status="failed", summary=f"Render refused: output already exists ({output})")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(executable), "render", "--modules", str(modules), "--output", str(output)]
    result = run_logged(cmd, ROOT, _unique_report_path("render", ".log"))
    record["command_result"] = result
    record["render_output"] = str(output) if output.is_file() else None
    success = result["exit_code"] == 0 and output.is_file()
    record["status"] = "passed" if success else "failed"
    record["summary"] = (f"Rendered offline WAV to {output}" if success
                         else f"Offline render failed (exit {result['exit_code']})")
    path = _save_record(record)
    _print_record(record, path, args.json)
    return 0 if success else (result["exit_code"] or 1)


def command_launch(args: argparse.Namespace) -> int:
    record = _base_record("launch", args.config)
    executable = _studio_executable(args.config)
    if not executable:
        record.update(status="skipped", summary="Launch skipped: build the Wirescape Studio desktop app first")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
    env = _qt_runtime_env(executable)
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    ready_path = _unique_report_path("studio-ready", ".json")
    result = wait_for_readiness([str(executable)], ROOT, _unique_report_path("studio-launch", ".log"),
                                ready_path, env, timeout=20)
    success = result["ready"]
    record.update(status="passed" if success else "failed",
                  summary="Wirescape Studio is ready; audio has not started" if success
                  else f"Studio did not become ready: {result['error']}",
                  executable=str(executable), command_result=result,
                  process_id=result.get("process_id"), readiness_report=str(ready_path))
    report = _save_record(record)
    _print_record(record, report, args.json)
    return 0 if success else 1


def command_ui_test(args: argparse.Namespace) -> int:
    record = _base_record("ui-test", args.config)
    executable = _studio_executable(args.config)
    if not executable:
        record.update(status="skipped", summary="UI smoke test skipped: build the Wirescape Studio desktop app first")
        report = _save_record(record)
        _print_record(record, report, args.json)
        return 2
    env = _qt_runtime_env(executable)
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["QT_QUICK_BACKEND"] = "software"
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    layouts = []
    for width, height in ((1400, 900), (1000, 650)):
        screenshot = _unique_report_path(f"studio-smoke-{width}x{height}", ".png")
        ui_report = _unique_report_path(f"ui-smoke-result-{width}x{height}", ".json")
        cmd = [str(executable), "--smoke-test", "--window-size", f"{width}x{height}",
               "--screenshot", str(screenshot), "--report", str(ui_report)]
        result = run_logged(cmd, ROOT, _unique_report_path(f"ui-smoke-{width}x{height}", ".log"), env, timeout=60)
        item: dict[str, Any] = {"requested_width": width, "requested_height": height,
                                "command_result": result,
                                "screenshot": str(screenshot) if screenshot.is_file() else None,
                                "ui_report": str(ui_report) if ui_report.is_file() else None}
        if ui_report.is_file():
            try:
                item["ui_test"] = json.loads(ui_report.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as exc:
                item["ui_report_parse_error"] = str(exc)
        report_data = item.get("ui_test", {})
        item_success = (result["exit_code"] == 0 and screenshot.is_file()
                        and isinstance(report_data, dict) and report_data.get("status") == "passed"
                        and report_data.get("logical_width") == width
                        and report_data.get("logical_height") == height
                        and report_data.get("screenshot_saved") is True)
        item["status"] = "passed" if item_success else "failed"
        layouts.append(item)
        if not item_success:
            break
    record["layouts"] = layouts
    # Keep the original single-layout fields available to existing report consumers.
    if layouts:
        record["command_result"] = layouts[-1]["command_result"]
        record["screenshot"] = layouts[-1]["screenshot"]
        record["ui_report"] = layouts[-1]["ui_report"]
        if "ui_test" in layouts[-1]: record["ui_test"] = layouts[-1]["ui_test"]
    success = len(layouts) == 2 and all(item["status"] == "passed" for item in layouts)
    record["status"] = "passed" if success else "failed"
    record["summary"] = "Qt offscreen UI passed at 1400x900 and 1000x650" if success else "Qt UI size check failed; see per-layout reports"
    path = _save_record(record)
    _print_record(record, path, args.json)
    return 0 if success else (result["exit_code"] or 1)


def command_startup_test(args: argparse.Namespace) -> int:
    """Verify readiness using a self-closing, audio-off GUI launch."""
    record = _base_record("startup-test", args.config)
    if not args.portable and os.name != "nt":
        record.update(status="skipped", summary="Normal-window startup check is Windows-only")
        path = _save_record(record); _print_record(record, path, args.json); return 2
    executable = (Path(args.portable).expanduser().resolve() / ("wirescape_studio.exe" if os.name == "nt" else "wirescape_studio")
                  if args.portable else _studio_executable(args.config))
    if not executable or not executable.is_file():
        record.update(status="skipped", summary="Startup check skipped: Wirescape Studio executable was not found")
        path = _save_record(record); _print_record(record, path, args.json); return 2
    ready_path = _unique_report_path("studio-startup-ready", ".json")
    env = _qt_runtime_env(executable) if not args.portable else _portable_runtime_env(executable.parent)
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    result = wait_for_readiness([str(executable), "--startup-check"],
                                executable.parent if args.portable else ROOT,
                                _unique_report_path("studio-startup", ".log"), ready_path, env, timeout=20)
    ready: dict[str, Any] = {}
    try:
        ready = json.loads(ready_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        record["readiness_parse_error"] = str(exc)
    valid = _valid_startup_report(result, ready)
    record.update(status="passed" if valid else "failed",
                  summary="Normal-window startup reached ready state with audio off" if valid else "Startup readiness report failed validation",
                  executable=str(executable), command_result=result, readiness_report=str(ready_path),
                  readiness=ready, scope="Self-closing startup check; does not start audio")
    path = _save_record(record); _print_record(record, path, args.json)
    return 0 if valid else 1


def command_package(args: argparse.Namespace) -> int:
    """Stage a versioned local portable app and smoke it without SDK paths/audio."""
    record = _base_record("package", args.config)
    executable = _studio_executable(args.config)
    metadata = _build_metadata()
    prefix = Path(metadata["qt_prefix"]) if metadata and metadata.get("qt_prefix") else None
    deploy = prefix / "bin" / "windeployqt.exe" if prefix else None
    candidates = [Path(args.portaudio).resolve()] if args.portaudio else []
    if not args.portaudio:
        external = os.environ.get("WIRESCAPE_PORTAUDIO")
        if external:
            candidates.append(Path(external).expanduser().resolve())
        candidates.extend((ROOT / ".tools/portaudio/libportaudio64bit.dll",))
        if executable:
            candidates.append(executable.parent / "libportaudio64bit.dll")
    backend = next((path for path in candidates if path.is_file()), None)
    if os.name != "nt" or not executable or not deploy or not deploy.is_file() or not backend or not backend.is_file():
        record.update(status="failed", summary="Package requires a Windows Qt build and PortAudio DLL (set WIRESCAPE_PORTAUDIO or use --portaudio PATH).")
        path = _save_record(record); _print_record(record, path, args.json); return 1
    if metadata.get("configuration") != args.config:
        record.update(status="failed", summary="Build configuration does not match requested package configuration.")
        path = _save_record(record); _print_record(record, path, args.json); return 1
    destination = ROOT / "build" / "packages" / f"wirescape-{args.config.lower()}-{utc_stamp()}"
    destination.mkdir(parents=True, exist_ok=False)
    for name in ("wirescape_studio.exe", "wirescape_cli.exe", "wirescape_engine.dll"):
        shutil.copy2(executable.parent / name, destination / name)
    for pattern in ("ws_*.dll", "ws_*.module.json"):
        for module in executable.parent.glob(pattern): shutil.copy2(module, destination / module.name)
    shutil.copy2(backend, destination / "libportaudio64bit.dll")
    shutil.copy2(ROOT / "LICENSE", destination / "LICENSE")
    shutil.copy2(ROOT / "native/ui/third_party/PORTAUDIO_LICENSE.txt", destination / "PORTAUDIO_LICENSE.txt")
    (destination / "README.txt").write_text(
        "Wirescape experimental local development package. Open wirescape_studio.exe.\n"
        "Audio starts OFF. Refresh outputs, select a device, Start, then hold Gate or play a note.\n"
        "Test render is 4 seconds mono; long releases may be cut off.\n"
        "Not an installer or public redistribution release. Qt 6 and MSVC runtimes are dynamically deployed.\n"
        "A clean Windows machine and physical audio still require validation.\n", encoding="utf-8")
    env = os.environ.copy(); env["PATH"] = str(prefix / "bin") + os.pathsep + env.get("PATH", "")
    deploy_result = run_logged([str(deploy), "--" + args.config.lower(), "--no-translations", "--compiler-runtime", "--include-plugins", "qoffscreen",
                               "--qmldir", str(ROOT / "native/ui/qml"), str(destination / executable.name)],
                              ROOT, _unique_report_path("package-deploy", ".log"), env, timeout=180)
    record["steps"] = [{"name":"deploy", **deploy_result}]
    if deploy_result["exit_code"] == 0:
        (destination / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\n", encoding="utf-8")
        # Exclude development SDK/plugin/backend overrides. This is a local isolated-path
        # smoke check, not proof of installation on a clean Windows machine.
        clean = _portable_runtime_env(destination)
        clean["QT_QPA_PLATFORM"] = "offscreen"; clean["QT_QUICK_BACKEND"] = "software"
        clean["QT_FORCE_STDERR_LOGGING"] = "1"
        smoke = run_logged([str(destination / executable.name), "--smoke-test", "--screenshot",
                            str(destination / "package-smoke.png"), "--report", str(destination / "package-smoke.json")],
                           destination, _unique_report_path("package-smoke", ".log"), clean, timeout=60)
        record["steps"].append({"name":"isolated_path_smoke", **smoke})
        try:
            smoke_data = json.loads((destination / "package-smoke.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            smoke_data = {}
            record["smoke_report_error"] = str(exc)
        smoke_valid = _valid_smoke_report(smoke_data)
        record["smoke_report"] = smoke_data
        record["steps"][-1]["report_valid"] = smoke_valid
        if not smoke_valid:
            record["steps"][-1]["exit_code"] = smoke["exit_code"] or 1
        # Probe the normal window with the same stripped environment, without forcing offscreen.
        ready_path = _unique_report_path("package-startup-ready", ".json")
        startup = wait_for_readiness([str(destination / "wirescape_studio.exe"), "--startup-check"],
                                     destination, _unique_report_path("package-startup", ".log"),
                                     ready_path, _portable_runtime_env(destination), timeout=20)
        try:
            ready = json.loads(ready_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            ready = {}
            record["startup_report_error"] = str(exc)
        startup_valid = _valid_startup_report(startup, ready)
        record["steps"].append({"name": "normal_window_startup", **startup, "report_valid": startup_valid})
        record["startup_readiness"] = ready
    failed = next((item for item in record["steps"] if item["exit_code"] or item.get("report_valid") is False), None)
    record["status"] = "failed" if failed else "passed"
    record["command_result"] = record["steps"][-1]
    record["package_directory"] = str(destination)
    record["summary"] = "Local portable package and isolated-path offline smoke passed" if not failed else f"Package {failed['name']} failed; see log"
    record["scope"] = "Local SDK-independent path smoke only; no clean-machine, hardware audio or public distribution claim"
    record["files"] = {p.relative_to(destination).as_posix():sha256_file(p) for p in destination.rglob("*") if p.is_file()}
    write_json(destination / "package-manifest.json", {"build":metadata, "files":record["files"]})
    try:
        inventory = inspect_package(destination)
        inventory_path = _unique_report_path("package-inventory", ".json")
        write_json(inventory_path, inventory)
        record["inventory_report"] = str(inventory_path)
        record["inventory_integrity"] = inventory["integrity"]
        if not inventory["integrity"]["ok"]:
            failed = failed or {"name": "package_inventory"}
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        record["inventory_error"] = str(exc)
        failed = failed or {"name": "package_inventory"}
    record["status"] = "failed" if failed else "passed"
    record["summary"] = "Local portable package, offline smoke, normal-window startup and integrity inventory passed" if not failed else f"Package {failed['name']} failed; see log/report"
    path = _save_record(record); _print_record(record, path, args.json)
    return 1 if failed else 0


def _portable_runtime_env(package: Path) -> dict[str, str]:
    """Environment for portable probes, excluding inherited Qt and audio overrides."""
    clean = os.environ.copy()
    for key in ("QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "QML_IMPORT_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH",
                "QT_QPA_PLATFORM", "QT_QUICK_BACKEND", "WIRESCAPE_PORTAUDIO"):
        clean.pop(key, None)
    system_root = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
    if os.name == "nt" and not system_root:
        raise RuntimeError("Windows runtime probes require SystemRoot or WINDIR")
    system = Path(system_root or os.sep)
    clean["PATH"] = os.pathsep.join([str(package), str(system / "System32"), str(system)])
    clean["QT_FORCE_STDERR_LOGGING"] = "1"
    return clean


def _valid_smoke_report(report: Any) -> bool:
    return isinstance(report, dict) and report.get("status") == "passed" and report.get("audio_started") is False


def _valid_startup_report(result: Any, report: Any) -> bool:
    return (isinstance(result, dict) and result.get("ready") is True and isinstance(report, dict)
            and report.get("status") == "ready" and report.get("audio_started") is False
            and report.get("normal_window") is True)


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
        ("build", command_build, argparse.Namespace(config=args.config, qt_prefix=None, headless=False, json=False)),
        ("test", command_test, argparse.Namespace(config=args.config, json=False)),
        ("ui_test", command_ui_test, argparse.Namespace(config=args.config, json=False)),
        ("startup_test", command_startup_test, argparse.Namespace(config=args.config, portable=None, json=False)),
        ("benchmark", None if functional_only else command_benchmark,
         argparse.Namespace(config=args.config, blocks=256, json=False)),
        ("render", command_render, argparse.Namespace(config=args.config, output=None, json=False)),
    ]
    if args.package:
        plan.append(("package", command_package,
                     argparse.Namespace(config=args.config, portaudio=None, json=False)))
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
        if "exit_code" in item and item["exit_code"] == 0: state = "passed"
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
    child.set_defaults(func=lambda args: run_acceptance(ROOT, args))
    for name, func in (("doctor", command_doctor), ("snapshot", command_snapshot), ("report", command_report)):
        child = sub.add_parser(name)
        child.add_argument("--json", action="store_true", help="print the full result as JSON")
        if name == "snapshot":
            child.add_argument("--label", default="private-data", help="short label for the backup filename")
        child.set_defaults(func=func)
    child = sub.add_parser("check", help="run the bounded sequential local checkpoint")
    child.add_argument("--config", choices=("Debug", "Release"), default="Release")
    child.add_argument("--package", action="store_true", help="include the portable package smoke stage")
    child.add_argument("--functional-only", action="store_true",
                       help="defer performance benchmarking while running functional checks")
    child.add_argument("--json", action="store_true", help="print the full result as JSON")
    child.set_defaults(func=command_check)
    for name, func in (("build", command_build), ("test", command_test)):
        child = sub.add_parser(name)
        child.add_argument("--config", choices=("Debug", "Release"), default="Release")
        if name == "build":
            child.add_argument("--qt-prefix", help="Qt SDK prefix; WIRESCAPE_QT_DIR, then CMAKE_PREFIX_PATH, then .tools/qt/6.10.3/msvc2022_64")
            child.add_argument("--headless", action="store_true", help="disable the optional Qt UI target")
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
    for name, func in (("launch", command_launch), ("ui-test", command_ui_test), ("startup-test", command_startup_test), ("package", command_package)):
        child = sub.add_parser(name)
        child.add_argument("--config", choices=("Debug", "Release"), default="Release")
        child.add_argument("--json", action="store_true", help="print the full result as JSON")
        if name == "package":
            child.add_argument("--portaudio", help="Explicit PortAudio DLL to include (defaults to built app directory)")
        if name == "startup-test":
            child.add_argument("--portable", help="Check a staged portable app directory instead of the development build")
        child.set_defaults(func=func)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
