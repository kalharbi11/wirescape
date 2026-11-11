# SPDX-License-Identifier: GPL-3.0-only
"""Advisory preflight for task acceptance blocks; never runs listed commands.

Usage: python tools/foundation/acceptance_preflight.py docs/tasks/FILE.md ...
Exit 1 when a problem is found. Reports syntax and fence problems only.
"""
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foundation import acceptance  # noqa: E402

FENCE = re.compile(r"(?ms)^```acceptance[^\S\n]*\n.*?^```[^\S\n]*$")


def check_text(text):
    """Return a list of problem strings for one task file's text."""
    problems = []
    count = len(FENCE.findall(text))
    if count != 1:
        problems.append(f"expected exactly one acceptance block, found {count}")
        if count == 0:
            return problems
    try:
        spec = acceptance.parse(text)
    except ValueError as error:
        return problems + [str(error)]
    bash = shutil.which("bash")
    for index, command in enumerate(spec["must_pass"], 1):
        if acceptance.classify_line(command) != "run":
            continue
        try:
            shlex.split(command, comments=True)
        except ValueError as error:
            problems.append(f"command {index}: {error} ({command})")
            continue
        if not bash:
            continue
        words = shlex.split(command, comments=True)
        scripts = ([command] if acceptance.shell_syntax(command) else []) + inner_scripts(words)
        for script in scripts:
            result = subprocess.run([bash, "-n", "-c", script], capture_output=True, text=True)
            if result.returncode:
                problems.append(f"command {index}: bash -n: {result.stderr.strip()} ({command})")
                break
    return problems


def inner_scripts(argv):
    """Script strings passed to `bash|sh -c` (flags like -lc, -ec included)."""
    if not argv or Path(argv[0]).name not in ("bash", "sh"):
        return []
    for position, arg in enumerate(argv[1:], 1):
        if not arg.startswith("-") or arg.startswith("--"):
            break
        if "c" in arg[1:] and position + 1 < len(argv):
            return [argv[position + 1]]
    return []


def main(argv):
    if not argv:
        print("usage: acceptance_preflight.py TASK.md ...", file=sys.stderr)
        return 2
    bad = 0
    for name in argv:
        for problem in check_text(Path(name).read_text(encoding="utf-8")):
            bad += 1
            print(f"{name}: {problem}")
    print("advisory preflight: " + ("FAIL" if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
