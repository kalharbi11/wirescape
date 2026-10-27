#!/usr/bin/env python3
"""Run bounded configure/build/CTest checks and write a concise JSON report."""
import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path


def run(command, cwd, timeout):
    start = time.monotonic()
    try:
        result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout, check=False)
        return {"status": "passed" if result.returncode == 0 else "failed",
                "exit_code": result.returncode, "seconds": round(time.monotonic() - start, 2),
                "output_tail": result.stdout[-5000:]}
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        return {"status": "failed", "exit_code": None, "seconds": round(time.monotonic() - start, 2),
                "output_tail": (output + "\nTimed out.")[-5000:]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    root = Path(__file__).resolve().parents[1]
    build = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
    cmake, ctest = shutil.which("cmake"), shutil.which("ctest")
    report = {"scope": "offline C++ module scaffold", "steps": {}, "result": "failed"}
    if not cmake or not ctest:
        report["steps"]["tooling"] = {"status": "skipped", "reason": "cmake and ctest are both required"}
        report["result"] = "skipped"
        code = 2
    else:
        report["steps"]["configure"] = run([cmake, "-S", str(root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"], root, args.timeout)
        code = 0
        if report["steps"]["configure"]["status"] != "passed":
            code = 1
        else:
            report["steps"]["build"] = run([cmake, "--build", str(build), "--config", "Release", "--parallel", "2"], root, args.timeout)
            if report["steps"]["build"]["status"] != "passed":
                code = 1
            else:
                report["steps"]["ctest"] = run([ctest, "--test-dir", str(build), "--output-on-failure", "-C", "Release"], root, args.timeout)
                if report["steps"]["ctest"]["status"] != "passed":
                    code = 1
        if code == 0:
            report["result"] = "passed"
    build.mkdir(parents=True, exist_ok=True)
    report_path = build / "check_report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"Report: {report_path}")
    return code


if __name__ == "__main__":
    sys.exit(main())
