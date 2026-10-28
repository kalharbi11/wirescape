# SPDX-License-Identifier: GPL-3.0-only
"""Run the small, dependency-free task acceptance format."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

from foundation.helpers import git_revision, run_logged, utc_iso, write_json

EXTERNAL = ("pi:", "ci:", "user:", "chatgpt-pre-review:")


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


def size_check(root, spec):
    if spec == "n/a":
        return {"status": "not_applicable"}
    line_limit, file_limit = map(int, re.findall(r"\d+", spec))
    # Numstat -z has extra path records for renames. Disable rename detection so
    # deleted and added paths both count rather than misparsing those records.
    raw = subprocess.check_output(["git", "diff", "--no-renames", "--numstat", "-z",
                                   "origin/main...HEAD"], cwd=root)
    entries = [entry.split(b"\t", 2) for entry in raw.split(b"\0") if entry]
    binary = any(a == b"-" or d == b"-" for a, d, _ in entries)
    lines = sum(int(a) + int(d) for a, d, _ in entries if a != b"-" and d != b"-")
    return {"status": "passed" if not binary and lines <= line_limit and len(entries) <= file_limit else "failed",
            "changed_lines": lines, "files": len(entries), "binary_changes": binary,
            "line_limit": line_limit, "file_limit": file_limit}


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
        record["size"] = size_check(root, block["size"])
        for index, command in enumerate(block["must_pass"]):
            item = {"command": command}
            if command.startswith(EXTERNAL):
                item["status"] = "external"
            else:
                words = shlex.split(command)
                if not words or words[0] not in ("python", "python3", "ctest", "bash"):
                    item.update(status="failed", error="unsupported local command; expected python, ctest or bash")
                else:
                    result = run_logged(["bash", "-c", command], root,
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
