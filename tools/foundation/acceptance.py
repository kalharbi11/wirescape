# SPDX-License-Identifier: GPL-3.0-only
"""Run the small, dependency-free task acceptance format."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

from foundation.helpers import git_revision, run_logged, utc_iso, write_json

EXTERNAL = ("pi:", "ci:", "user:", "chatgpt-pre-review:")

# Planner-approved exceptions for historical, user-run tasks (issue #114).
# These are skipped and visibly NOT verified, never reported as a task pass.
LEGACY_EXCEPTIONS = {45: "user J9 session", 64: "deferred laptop measurement"}


def parse(text):
    blocks = re.findall(r"(?ms)^```acceptance[^\S\n]*\n(.*?)^```[^\S\n]*$", text)
    if not blocks:
        raise ValueError("task has no fenced acceptance block")
    fields, commands = {}, []
    in_commands = False
    for raw in blocks[-1].splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("- ") and in_commands:
            commands.append(line[2:].strip())
            continue
        match = re.fullmatch(r"(tier|budget|size|must_pass|stops):\s*(.*)", line)
        if not match:
            raise ValueError("unsupported acceptance syntax: " + line)
        key, value = match.groups()
        if key in fields:
            raise ValueError("duplicate acceptance field: " + key)
        fields[key] = value
        in_commands = key == "must_pass"
    if not commands:
        raise ValueError("must_pass has no commands")
    size = fields.get("size", "")
    if size != "n/a" and not re.fullmatch(r"<=\s*\d+ changed lines,\s*<=\s*\d+ files", size):
        raise ValueError("size must name changed-line and file limits, or n/a")
    return {**fields, "must_pass": commands, "block": blocks[-1]}


def find_task(root, issue):
    matches = []
    for path in sorted((root / "docs/tasks").glob("*.md")):
        title = next((line for line in path.read_text(encoding="utf-8").splitlines()
                      if line.startswith("# ")), "")
        if re.search(rf"#{int(issue)}\b", title):
            matches.append(path)
    if len(matches) != 1:
        raise ValueError(f"expected one task title naming #{issue}; found {len(matches)}; use --file")
    return matches[0]



def select_pr_task(root, pr_body):
    """Return (issue, task, note); fail closed for referenced unverified tasks."""
    match = re.search(r"(?i)\\bCloses\\s+#(\\d+)\\b", pr_body or "")
    if not match:
        return None, None, "No Closes #N reference; acceptance not selected."
    issue = int(match.group(1))
    if issue in LEGACY_EXCEPTIONS:
        return issue, None, (f"legacy exception — not verified: #{issue} "
                             f"({LEGACY_EXCEPTIONS[issue]}); this is not an acceptance pass")
    try:
        task = find_task(Path(root), issue)
        spec = parse(task.read_text(encoding="utf-8"))
        if spec.get("tier") not in ("T0", "T1", "T2"):
            raise ValueError("acceptance block must declare a valid tier")
    except (OSError, ValueError) as error:
        raise ValueError(f"not verified: issue #{issue}: {error}") from error
    return issue, task, ""



def size_check(root, spec, base="origin/main"):
    if spec == "n/a":
        return {"status": "not_applicable"}
    revision = subprocess.check_output(["git", "rev-parse", "--verify", "--end-of-options",
                                        base + "^{commit}"], cwd=root).decode().strip()
    line_limit, file_limit = map(int, re.findall(r"\d+", spec))
    # Numstat -z has extra path records for renames. Disable rename detection so
    # deleted and added paths both count rather than misparsing those records.
    raw = subprocess.check_output(["git", "diff", "--no-renames", "--numstat", "-z",
                                   revision + "...HEAD"], cwd=root)
    entries = [entry.split(b"\t", 2) for entry in raw.split(b"\0") if entry]
    binary = any(a == b"-" or d == b"-" for a, d, _ in entries)
    lines = sum(int(a) + int(d) for a, d, _ in entries if a != b"-" and d != b"-")
    return {"status": "passed" if not binary and lines <= line_limit and len(entries) <= file_limit else "failed",
            "changed_lines": lines, "files": len(entries), "binary_changes": binary,
            "line_limit": line_limit, "file_limit": file_limit, "base": base, "base_commit": revision}


def shell_syntax(command):
    quote, escaped = None, False
    for index, char in enumerate(command):
        if escaped:
            escaped = False
            continue
        if char == "\\" and quote != "'":
            escaped = True
            continue
        if quote:
            if char == quote:
                quote = None
            elif quote == '"' and (char == '`' or (char == '$' and re.match(r'[A-Za-z_0-9({?*@#!$-]', command[index + 1:]))):
                return True
        elif char in "'\"":
            quote = char
        elif char == '#':
            break
        elif char in ';&|<>()`' or (char == '$' and re.match(r'[A-Za-z_0-9({?*@#!$-]', command[index + 1:])):
            return True
    return False


def command_argv(command):
    words = shlex.split(command, comments=True)
    if not words or words[0] not in ("python", "python3", "ctest", "bash", "actionlint", "pre-commit"):
        raise ValueError("unsupported local command; expected python, ctest, bash, actionlint or pre-commit")
    needs_shell = shell_syntax(command)
    if needs_shell or words[0] == "bash":
        bash = shutil.which("bash")
        if not bash:
            raise ValueError("Bash is required for shell syntax or scripts; install a working Bash or use plain Python/ctest")
        return [bash, "-c", command] if needs_shell else [bash, *words[1:]]
    if words[0] in ("python", "python3"):
        words[0] = sys.executable
    return words


def run(root, args):
    root = Path(root)
    key = str(args.issue) if args.issue is not None else "file"
    out = root / "build_reports" / f"accept-{key}"
    record = {"issue": args.issue, "created_at": utc_iso(), "commit": git_revision(root),
              "commands": [], "status": "failed"}
    try:
        if args.timeout <= 0:
            raise ValueError("timeout must be positive")
        if args.issue is None and not args.file:
            raise ValueError("provide an issue number or --file")
        task = Path(args.file).resolve() if args.file else find_task(root, args.issue)
        text = task.read_text(encoding="utf-8")
        block = parse(text)
        record.update(task=str(task), task_sha256=hashlib.sha256(text.encode()).hexdigest(), tier=block.get("tier"))
        record["size"] = size_check(root, block["size"], getattr(args, "base", "origin/main"))
        for index, command in enumerate(block["must_pass"]):
            item = {"command": command}
            if command.startswith(EXTERNAL):
                item["status"] = "external"
            else:
                try:
                    argv = command_argv(command)
                except ValueError as error:
                    item.update(status="failed", error=str(error))
                else:
                    result = run_logged(argv, root,
                                        out.with_name(f"{out.name}-{index}.log"), timeout=args.timeout)
                    item.update(result, command=command, status="passed" if result["exit_code"] == 0 else "failed")
            record["commands"].append(item)
        record["status"] = "passed" if record["size"]["status"] != "failed" and all(
            item["status"] in ("passed", "external") for item in record["commands"]) else "failed"
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        record["error"] = str(error)
    lines = [f"Acceptance: {record['status']}", "", f"Commit: `{record['commit']}`", "",
             "| Command | Result |", "|---|---|"]
    for item in record["commands"]:
        label = item["command"].replace("|", "\\|")
        lines.append(f"| `{label}` | {item['status']} |")
    lines.extend(["", "Size: " + json.dumps(record.get("size", {})),
                  "", "External entries are pending evidence; local success does not verify them."])
    if record.get("error"):
        lines.append(record["error"])
    markdown = "\n".join(lines) + "\n"
    write_json(out.with_suffix(".json"), record)
    out.with_suffix(".md").write_text(markdown, encoding="utf-8")
    print(json.dumps(record, indent=2) if args.json else markdown, end="\n" if args.json else "")
    return int(record["status"] != "passed")
