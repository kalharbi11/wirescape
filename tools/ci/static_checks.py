# SPDX-License-Identifier: GPL-3.0-only
"""Run static checks without rewriting sources; summarize failures and counts."""
import argparse
import json
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
            if current and Path(current).suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inc"):
                files.setdefault(current, [])
        match = re.match(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
        if current and match and Path(current).suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inc"):
            start, count = int(match[1]), int(match[2] or "1")
            if count:
                files.setdefault(current, []).append((start, start + count - 1))
    return files


def analysis_units(root, files):
    """Use compiler-recorded Ninja dependencies, including nested/X-macro includes."""
    build = root / "build/static"
    database = json.loads((build / "compile_commands.json").read_text())
    output = subprocess.check_output(["ninja", "-C", str(build), "-t", "deps"], text=True)
    dependencies, current = {}, None
    for line in output.splitlines():
        if line and not line[0].isspace() and ": #deps " in line:
            current = line.split(": #deps ", 1)[0]
            if "(VALID)" not in line:
                raise ValueError("Stale compiler dependencies for " + current)
            dependencies[current] = set()
        elif current and line.startswith("    "):
            dependencies[current].add((build / line.strip()).resolve())
    wanted = {(root / name).resolve() for name in files}
    selected, covered = [], set()
    for entry in database:
        directory = Path(entry["directory"])
        source = (directory / entry["file"]).resolve()
        args = entry.get("arguments") or shlex.split(entry["command"])
        obj = (directory / args[args.index("-o") + 1]).resolve()
        key = os.path.relpath(obj, build)
        reached = ({source} | dependencies.get(key, set())) & wanted
        if reached:
            selected.append(entry)
            covered.update(reached)
    if wanted - covered:
        raise ValueError("No compiled owner for changed files: " +
                         ", ".join(str(p.relative_to(root)) for p in sorted(wanted - covered)))
    return selected


def run_checks(root, diff, tidy_diff):
    files = changed_ranges(diff)
    jobs = []
    if files:
        try:
            units = analysis_units(root.resolve(), files)
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            jobs.append(("compile database coverage", [sys.executable, "-c",
                         "import sys; sys.exit(" + repr(str(error)) + ")"], None))
            units = []
        line_filter = json.dumps([{"name": str((root / name).resolve()), "lines": ranges}
                                  for name, ranges in files.items() if ranges])
        for entry in units if any(files.values()) else []:
            jobs.append(("clang-tidy diff " + entry["file"], ["clang-tidy-18", "-p", "build/static",
                         "--header-filter=.*", "--line-filter=" + line_filter, entry["file"]], None))
        if units:
            project = root / "build/static/cppcheck_changed.json"
            project.parent.mkdir(parents=True, exist_ok=True)
            project.write_text(json.dumps(units), encoding="utf-8")
            jobs.append(("cppcheck changed files", ["cppcheck", "--enable=warning,performance",
                         "--error-exitcode=1", "--suppress=missingIncludeSystem", "--template=gcc",
                         "--library=qt", "--project=" + str(project)], None))
        for name, ranges in files.items():
            if ranges:
                jobs.append(("clang-format " + name, ["clang-format-18", "--dry-run", "--Werror",
                             *[f"--lines={start}:{end}" for start, end in ranges], "./" + name], None))
    jobs.append(("actionlint workflows", ["actionlint"], None))
    python_files = []
    for line in diff.splitlines():
        if line.startswith("+++ ") and line != "+++ /dev/null":
            name = shlex.split(line[4:])[0].removeprefix("b/")
            if name.startswith("tools/") and Path(name).suffix == ".py":
                python_files.append("./" + name)
    if python_files:
        jobs.append(("ruff changed files", ["ruff", "check", "--output-format", "concise",
                                           "--", *dict.fromkeys(python_files)], None))
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
    if not python_files:
        rows.append(("ruff changed files", 0, "not applicable: no changed Python files"))
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
