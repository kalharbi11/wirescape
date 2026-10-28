# SPDX-License-Identifier: GPL-3.0-only
"""Run static checks without rewriting sources; summarize failures and counts."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys


def changed_ranges(diff):
    files, current = {}, None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            current = None if line == "+++ /dev/null" else shlex.split(line[4:])[0].removeprefix("b/")
            if current and Path(current).suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"):
                files.setdefault(current, [])
        match = re.match(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
        if current and match and Path(current).suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"):
            start, count = int(match[1]), int(match[2] or "1")
            if count:
                files.setdefault(current, []).append((start, start + count - 1))
    return files


def run_checks(root, diff, tidy_diff):
    files = changed_ranges(diff)
    jobs = []
    if any(files.values()):
        jobs.append(("clang-tidy diff", [sys.executable, tidy_diff, "-p1", "-path", "build/static",
                                       "-clang-tidy-binary", "clang-tidy-18", "-j2"], diff))
    if files:
        jobs.append(("cppcheck changed files", ["cppcheck", "--enable=warning,performance", "--error-exitcode=1",
                     "--suppress=missingIncludeSystem", "--template=gcc", "--std=c++17",
                     "-Inative/include", "-Inative/sdk", *["./" + name for name in files]], None))
        for name, ranges in files.items():
            if ranges:
                jobs.append(("clang-format " + name, ["clang-format-18", "--dry-run", "--Werror",
                             *[f"--lines={start}:{end}" for start, end in ranges], "./" + name], None))
    jobs.extend([("actionlint workflows", ["actionlint"], None),
                 ("ruff tools", ["ruff", "check", "tools/", "--output-format", "concise"], None)])
    rows = []
    for name, command, stdin in jobs:
        try:
            result = subprocess.run(command, cwd=root, input=stdin, text=True, capture_output=True, timeout=600)
            output, code = result.stdout + result.stderr, result.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            output, code = str(error), 1
        count = len(re.findall(r"^.*?:\d+:\d+:\s", output, re.M))
        failed = code != 0 or count > 0
        rows.append((name, count, "failed" if failed else "passed"))
        notice = f"{name}: {rows[-1][2]}; diagnostics={count}"
        print("::notice title=Static analysis::" + notice.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))
        print(f"\n{name}: {rows[-1][2]} ({count} diagnostics)\n{output}", flush=True)
    if not files:
        rows.insert(0, ("C++ diff checks", 0, "not applicable: no changed C++ lines"))
    report = "| Check | Diagnostics | Result |\n|---|---:|---|\n"
    report += "\n".join(f"| {name.replace('|', '/')} | {count} | {status} |" for name, count, status in rows)
    print(report)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write(report + "\n")
    return int(any(status == "failed" for _, _, status in rows))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", default="HEAD")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[2]
    diff = subprocess.check_output(["git", "diff", "--no-ext-diff", "--no-renames", "--unified=0",
                                    args.base + "..." + args.head, "--"], cwd=root, text=True)
    return run_checks(root, diff, os.environ.get("CLANG_TIDY_DIFF", "clang-tidy-diff-18.py"))


if __name__ == "__main__":
    raise SystemExit(main())
