"""Portable, side-effect-limited helpers for tools/dev.py."""

from __future__ import annotations

import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
import signal
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

_WINDOWS_JOBS: dict[int, Any] = {}
_READINESS_PROCESSES: dict[int, subprocess.Popen[Any]] = {}


def _attach_windows_job(process: subprocess.Popen[Any]) -> None:
    """Attach an invocation to a Job Object so timeout cleanup covers descendants."""
    if os.name != "nt":
        return
    try:
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateJobObjectW.restype = wintypes.HANDLE
        kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        job = kernel.CreateJobObjectW(None, None)
        if not job:
            return
        kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        if not kernel.AssignProcessToJobObject(job, wintypes.HANDLE(int(process._handle))):
            kernel.CloseHandle(job)
            return
        _WINDOWS_JOBS[process.pid] = job
    except (AttributeError, OSError, ValueError):
        return


def _close_windows_job(pid: int, terminate: bool = False) -> None:
    job = _WINDOWS_JOBS.pop(pid, None)
    if job is None:
        return
    try:
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        if terminate:
            kernel.TerminateJobObject(job, 124)
        kernel.CloseHandle(job)
    except (AttributeError, OSError):
        pass


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_revision(root: Path) -> str | None:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False,
        )
    except OSError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def _which_or_windows_path(name: str) -> str | None:
    found = shutil.which(name)
    if found:
        return str(Path(found).resolve())
    if os.name == "nt" and name.lower() in {"cmake", "cmake.exe", "ctest", "ctest.exe"}:
        program_files = os.environ.get("ProgramFiles")
        if program_files:
            binary = "cmake.exe" if name.lower().startswith("cmake") else "ctest.exe"
            candidate = Path(program_files) / "CMake" / "bin" / binary
            if candidate.is_file():
                return str(candidate.resolve())
    return None


def _vs_installation() -> dict[str, str] | None:
    if os.name != "nt":
        return None
    root = os.environ.get("ProgramFiles(x86)")
    if not root:
        return None
    vswhere = Path(root) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    try:
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            timeout=10, check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    path = result.stdout.strip()
    if result.returncode != 0 or not path:
        return None
    install = Path(path)
    compilers = list((install / "VC" / "Tools" / "MSVC").glob("*"))
    if not any((version / "bin" / "Hostx64" / "x64" / "cl.exe").is_file()
               for version in compilers):
        return None
    match = re.search(r"\\(20\d{2})\\", str(install))
    year = match.group(1) if match else ""
    # Visual Studio's major generator version follows the year for recent releases.
    major = {"2019": "16", "2022": "17", "2026": "18"}.get(year)
    if not major:
        return {"path": str(install), "year": year, "major": ""}
    return {"path": str(install), "year": year, "major": major}


def discover_toolchain() -> dict[str, Any]:
    cmake = _which_or_windows_path("cmake")
    vs = _vs_installation()
    ninja = _which_or_windows_path("ninja")
    make = _which_or_windows_path("make")
    ctest = _which_or_windows_path("ctest")
    generator: str | None = None
    architecture = platform.machine() or "unknown"
    if os.name == "nt":
        if vs and vs.get("major"):
            generator = f"Visual Studio {vs['major']} {vs['year']}"
            architecture = "x64"
        elif ninja:
            generator = "Ninja"
    elif ninja:
        generator = "Ninja"
    elif make:
        generator = "Unix Makefiles"
    return {
        "cmake": cmake,
        "generator": generator,
        "architecture": architecture,
        "ninja": ninja,
        "make": make,
        "ctest": ctest,
        "visual_studio": vs,
    }


def tool_version(executable: str | None, args: Iterable[str] = ("--version",)) -> dict[str, Any]:
    if not executable:
        return {"path": None, "version": None, "available": False}
    try:
        result = subprocess.run(
            [executable, *args], text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=10, check=False,
        )
        first = result.stdout.strip().splitlines()
        return {
            "path": executable,
            "version": first[0] if first else "unknown",
            "available": result.returncode == 0,
        }
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"path": executable, "version": None, "available": False, "error": str(exc)}


def run_logged(args: list[str], cwd: Path, log_path: Path,
               env: dict[str, str] | None = None, timeout: float = 600) -> dict[str, Any]:
    """Run a command and save complete combined output for later diagnosis."""
    started = time.monotonic()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    output = ""
    process = None
    error = None
    code = 127
    try:
        options: dict[str, Any] = {}
        if os.name == "nt":
            options["creationflags"] = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
        else:
            options["start_new_session"] = True
        # A file avoids descendants retaining an inherited pipe and making
        # communicate() wait forever after their owned root has exited.
        with log_path.open("w", encoding="utf-8", errors="replace") as log_stream:
            process = subprocess.Popen(args, cwd=cwd, env=env, text=True,
                                       stdout=log_stream, stderr=subprocess.STDOUT,
                                       **options)
            _attach_windows_job(process)
        try:
            process.wait(timeout=timeout)
            code = process.returncode
            _close_windows_job(process.pid)
        except subprocess.TimeoutExpired:
            # End only the process group created for this invocation. On Windows,
            # taskkill /T scopes the tree to this exact owned root PID.
            _terminate_process_tree(process)
            output = f"\nFAILED: command timed out after {timeout} seconds.\n"
            code = 124
            error = f"timeout after {timeout} seconds"
    except OSError as exc:
        output = f"Could not start {args[0]}: {exc}\n"
        code = 127
        error = str(exc)
    if output:
        with log_path.open("a", encoding="utf-8", errors="replace") as stream:
            stream.write(output)
    return {
        "command": args,
        "exit_code": code,
        "duration_seconds": round(time.monotonic() - started, 3),
        "log": str(log_path),
        "error": error,
        "timeout_seconds": timeout,
        "timed_out": code == 124 and error is not None,
    }


def _terminate_process_tree(process: subprocess.Popen[Any]) -> None:
    if os.name == "nt":
        _close_windows_job(process.pid, terminate=True)
        if process.poll() is not None:
            return
        try:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=4, check=False)
        except (OSError, subprocess.TimeoutExpired):
            pass
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=1)
        except (ProcessLookupError, subprocess.TimeoutExpired):
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2)


def wait_for_readiness(command: list[str], cwd: Path, log_path: Path,
                       report_path: Path, env: dict[str, str], timeout: float = 20) -> dict[str, Any]:
    """Launch an app and require its explicit audio-off readiness report."""
    started = time.monotonic()
    report_path.unlink(missing_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_stream = log_path.open("w", encoding="utf-8", errors="replace")
    process = None
    error = None
    status = "failed"
    try:
        options: dict[str, Any] = {}
        if os.name == "nt": options["creationflags"] = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
        else: options["start_new_session"] = True
        process = subprocess.Popen([*command, "--ready-report", str(report_path)], cwd=cwd,
                                   env=env, stdout=log_stream, stderr=subprocess.STDOUT,
                                   **options)
        _attach_windows_job(process)
        deadline = started + timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                error = f"application exited before readiness (exit {process.returncode})"
                break
            try:
                report = json.loads(report_path.read_text(encoding="utf-8"))
                if not isinstance(report, dict):
                    error = "invalid readiness report: expected a JSON object"
                    break
                if (report.get("status") == "ready" and report.get("audio_started") is False
                        and report.get("normal_window") is True):
                    status = "ready"
                    break
                error = f"invalid readiness report: {report}"
                break
            except (OSError, json.JSONDecodeError):
                time.sleep(.05)
        if status != "ready":
            if error is None:
                error = f"readiness timed out after {timeout} seconds"
            if process is not None and process.poll() is None:
                _terminate_process_tree(process)
            elif process is not None:
                _close_windows_job(process.pid)
        elif process is not None:
            # Keep the handle owned while the normal desktop app continues running.
            _READINESS_PROCESSES[process.pid] = process
    except OSError as exc:
        error = str(exc)
    finally:
        log_stream.close()
    return {"status": status, "ready": status == "ready", "process_id": process.pid if process else None,
            "exit_code": process.poll() if process else None, "error": error,
            "readiness_report": str(report_path), "log": str(log_path),
            "duration_seconds": round(time.monotonic() - started, 3), "timeout_seconds": timeout}


def source_provenance(root: Path) -> dict[str, Any]:
    """Fingerprint native/tool sources, including untracked files, without private data."""
    digest = hashlib.sha256()
    count = 0
    for folder in (root / "native", root / "tools"):
        for path in sorted(folder.rglob("*")):
            if not path.is_file() or "__pycache__" in path.parts:
                continue
            relative = path.relative_to(root).as_posix()
            digest.update(relative.encode("utf-8") + b"\0")
            digest.update(bytes.fromhex(sha256_file(path)))
            count += 1
    try:
        status = subprocess.run(["git", "status", "--porcelain", "--untracked-files=normal"],
                                cwd=root, capture_output=True, text=True, timeout=10)
        dirty = bool(status.stdout.strip()) if status.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        dirty = None
    return {"sha256": digest.hexdigest(), "file_count": count, "git_dirty": dirty,
            "scope": "native/ and tools/ excluding __pycache__"}


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temp.replace(path)


def local_data_files(root: Path) -> list[Path]:
    """Enumerate only private user data and the local Surge binary bundle."""
    files: list[Path] = []
    for relative in (Path("user_data"), Path("modules/chaos/surgepy_bin")):
        source = root / relative
        if source.is_dir():
            files.extend(p for p in source.rglob("*") if p.is_file())
        elif source.is_file():
            files.append(source)
    return sorted(files, key=lambda p: p.relative_to(root).as_posix().casefold())


def create_snapshot(root: Path, backup_root: Path, label: str,
                    commit: str | None = None, stamp: str | None = None) -> dict[str, Any]:
    """Archive only user_data and the local Surge binding, refusing overwrite."""
    clean_label = re.sub(r"[^A-Za-z0-9._-]+", "-", label).strip(".-") or "local-data"
    name = f"{stamp or utc_stamp()}-{clean_label}"
    backup_root.mkdir(parents=True, exist_ok=True)
    archive = backup_root / f"{name}.zip"
    manifest_path = backup_root / f"{name}.manifest.json"
    if archive.exists() or manifest_path.exists():
        raise FileExistsError(f"Snapshot destination already exists: {archive}")
    files = local_data_files(root)
    entries = [{"path": p.relative_to(root).as_posix(), "size_bytes": p.stat().st_size,
                "sha256": sha256_file(p)} for p in files]
    try:
        with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as bundle:
            for file in files:
                bundle.write(file, file.relative_to(root).as_posix())
        manifest = {
            "schema_version": 1,
            "created_at": utc_iso(),
            "repository": str(root),
            "commit": commit,
            "archive": archive.name,
            "archive_sha256": sha256_file(archive),
            "files": entries,
        }
        write_json(manifest_path, manifest)
    except Exception:
        archive.unlink(missing_ok=True)
        manifest_path.unlink(missing_ok=True)
        raise
    return {"archive": str(archive), "manifest": str(manifest_path), "file_count": len(entries)}

