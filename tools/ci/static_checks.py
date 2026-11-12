# SPDX-License-Identifier: GPL-3.0-only
"""Run static checks without rewriting sources; summarize failures and counts."""
import argparse
import difflib
import hashlib
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


def cppcheck_model(root, source, dependencies):
    """Choose a model from the compiled TU and its *transitive* headers.

    Qt UI/moc sources require the Qt model even when they include product
    headers. For other TUs, product headers win over Qt dependencies.
    Unknown/unmapped owners default to native.
    """
    relative = source.relative_to(root) if source.is_relative_to(root) else None
    if source.is_relative_to(root / "native/ui") or (
            relative is not None and relative.parts[0].startswith("build")
            and any(part.endswith("_autogen") for part in relative.parts[:-1])):
        return "qt"
    paths = {source, *dependencies}
    product = ("native/modules2", "native/engine2", "native/sidecar")
    if any(path.is_relative_to(root / directory)
           for path in paths for directory in product):
        return "native"
    qt_parts = {"QtCore", "QtGui", "QtWidgets", "QtNetwork", "QtOpenGL",
                "QtQml", "QtQuick", "QtSvg", "QtTest"}
    return "qt" if any(qt_parts.intersection(path.parts) for path in paths) else "native"


def build_units(root, files, build):
    """Use compiler-recorded Ninja dependencies, including nested/X-macro includes."""
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
            selected.append({**entry, "file": str(source), "build": str(build),
                             "cppcheck_model": cppcheck_model(root, source, dependencies.get(key, set()))})
            covered.update(reached)
    return selected, covered


def analysis_units(root, files, builds=("build/static", "build/static-web")):
    """Require every configured build and cover changes with their union."""
    selected, covered = [], set()
    for name in builds:
        units, reached = build_units(root, files, root / name)
        selected.extend(units)
        covered.update(reached)
    missing = {(root / name).resolve() for name in files} - covered
    if missing:
        raise ValueError("No compiled owner for changed files: " +
                         ", ".join(str(p.relative_to(root)) for p in sorted(missing)))
    return selected


def pinned_cppcheck_baseline(root, output, code):
    """Recognize exact historical vendor findings, never first-party or new ones."""
    baseline_path = root / "native/modules2/macro_oscillator/cppcheck-baseline.json"
    if not baseline_path.exists():
        return output, code, 0
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    if baseline.get("schema") != 1 or not baseline.get("source_sha256"):
        raise ValueError("Invalid pinned cppcheck baseline")
    for name, expected in baseline["source_sha256"].items():
        source = (root / name).resolve()
        if not source.is_relative_to(root.resolve()) or hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise ValueError("Pinned cppcheck baseline invalidated by source change: " + name)
    known = {(d["path"], d["line"], d["column"], d["id"], d["message"])
             for d in baseline["diagnostics"]}
    prefixes = [
        (root / "build/static/_deps/macro_eurorack-src", "eurorack"),
        (root / "build/static-web/_deps/macro_eurorack-src", "eurorack"),
        (root / "build/static/engine2/macro_oscillator/upstream/stmlib", "stmlib"),
        (root / "build/static-web/upstream/stmlib", "stmlib"),
    ]
    diagnostic = re.compile(r"^(.*?):(\d+):(\d+): warning: (.*) \[([A-Za-z0-9]+)\]$")
    remaining, matched = [], 0
    for line in output.splitlines():
        match = diagnostic.fullmatch(line)
        inherited = False
        if match:
            path = Path(match[1])
            if not path.is_absolute():
                path = root / path
            path = path.resolve()
            for directory, vendor in prefixes:
                if path.is_relative_to(directory.resolve()):
                    name = vendor + "/" + path.relative_to(directory.resolve()).as_posix()
                    key = (name, int(match[2]), int(match[3]), match[5], match[4])
                    inherited = key in known
                    break
        if inherited:
            matched += 1
        else:
            remaining.append(line)
    filtered = "\n".join(remaining)
    # 42 is cppcheck's configured findings exit code; crashes/tool errors stay red.
    if code == 42 and matched and not re.search(r"^.*?:\d+:\d+:\s", filtered, re.M):
        code = 0
    return filtered, code, matched


def format_patch(root, name, command):
    """Emit an applicable patch for the exact failed changed-line format command."""
    source = (root / name).read_text(encoding="utf-8")
    formatted = subprocess.run([command[0], *command[3:]], cwd=root, text=True,
                               capture_output=True, timeout=600, check=True).stdout
    return "".join(difflib.unified_diff(
        source.splitlines(keepends=True), formatted.splitlines(keepends=True),
        fromfile="a/" + name, tofile="b/" + name))


def publish_format_patch(root, patches):
    if not patches:
        return
    content = "".join(patches)
    target = root / "build/static/format.patch"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")
    print("=== format.patch begin ===\n" + content + "\n=== format.patch end ===", flush=True)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as out:
            out.write("### clang-format fix patch\n\n```diff\n" +
                      content[:60000] + "\n```\n")


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
            jobs.append(("clang-tidy diff " + entry["file"], ["clang-tidy-18", "-p", entry["build"],
                         "--header-filter=.*", "--line-filter=" + line_filter, entry["file"]], None))
        for model in ("native", "qt"):
            owners = [entry for entry in units if entry.get("cppcheck_model", "native") == model]
            if not owners:
                continue
            # Preserve the historical check name and all analyzer options. Only
            # the project membership and Qt keyword model differ per TU.
            suffix = "_qt" if model == "qt" else ""
            project = root / ("build/static/cppcheck_changed" + suffix + ".json")
            project.parent.mkdir(parents=True, exist_ok=True)
            project.write_text(json.dumps([
                {k: v for k, v in entry.items() if k not in ("build", "cppcheck_model")}
                for entry in owners
            ]), encoding="utf-8")
            command = ["cppcheck", "--enable=warning,performance",
                       "--error-exitcode=42", "--suppress=missingIncludeSystem",
                       "--inline-suppr", "--template=gcc"]
            if model == "qt":
                command.append("--library=qt")
            command.append("--project=" + str(project))
            jobs.append(("cppcheck changed files", command, None))
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
    rows, patches = [], []
    for name, command, stdin in jobs:
        try:
            result = subprocess.run(command, cwd=root, input=stdin, text=True, capture_output=True, timeout=600)
            output, code = result.stdout + result.stderr, result.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            output, code = str(error), 1
        if name == "cppcheck changed files":
            # Keep the complete evidence in the log, collapsed by default.
            print("::group::Full cppcheck report (including pinned vendor baseline)")
            print(output)
            print("::endgroup::")
            try:
                output, code, inherited = pinned_cppcheck_baseline(root, output, code)
                if inherited:
                    print(f"Pinned vendor baseline: {inherited} exact findings; new findings still fail")
            except (OSError, ValueError, KeyError, TypeError) as error:
                output += "\nPinned baseline validation failed: " + str(error)
                code = 1
        count = len(re.findall(r"^.*?:\d+:\d+:\s", output, re.M))
        failed = code != 0 or count > 0
        rows.append((name, count, "failed" if failed else "passed"))
        if failed and name.startswith("clang-format "):
            try:
                patch_text = format_patch(root, name[len("clang-format "):], command)
                if patch_text:
                    patches.append(patch_text)
            except (OSError, subprocess.SubprocessError) as error:
                print(f"format.patch for {name} not generated: {error}", flush=True)
        notice = f"{name}: {rows[-1][2]}; diagnostics={count}"
        print("::notice title=Static analysis::" + notice.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))
        print(f"\n{name}: {rows[-1][2]} ({count} diagnostics)\n{output}", flush=True)
    publish_format_patch(root, patches)
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
