#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create bounded, read-only review packs; never execute PR code."""
import argparse
import base64
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
from urllib.request import HTTPRedirectHandler, Request, build_opener

ROOT = Path(__file__).resolve().parents[1]
PROMPTS = {
    "chatgpt": "ChatGPT pre-review: Report findings by severity with file:line and a test that catches each. Review the supplied task and evidence; distinguish unavailable evidence from failures.",
    "claude": "Claude review: Assess the task's acceptance and tier from this pack. Report actionable findings with file:line and verification; do not approve or merge.",
    "external": "External-queue task: Review this pack independently. Return an External result with findings by severity, file:line, and tests. Flag missing evidence; do not assume checks passed.",
}
CHECKLIST = """## Fixed review checklist
- Realtime: no allocation, locks, I/O, exceptions, or logging in callbacks.
- IDs: module, port, parameter and compatibility IDs remain stable.
- State/versioning: lossless migration, unknown-data retention, reset and state handling.
- Thresholds: no removed tests, weakened assertions, or unauthorized tolerance changes.
- Licences/SPDX: provenance, pinned revisions and SPDX headers are present.
- Devices: no audio/MIDI devices or LAN probes opened by tests/CI.
"""


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *_args, **_kwargs):
        raise ValueError("GitHub API redirect refused; credentials were not forwarded")


def api(path, diff=False):
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise ValueError("GITHUB_TOKEN (or GH_TOKEN) is required for online mode")
    request = Request("https://api.github.com/" + path, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github.v3.diff" if diff else "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28"})
    with build_opener(NoRedirect()).open(request, timeout=30) as response:
        text = response.read().decode("utf-8")
    return text if diff else json.loads(text)


def acceptance(task):
    if task is None:
        return "Unavailable: no task file identified."
    blocks = re.findall(r"^```acceptance[^\n]*\n(.*?)^```\s*$", task, re.M | re.S)
    return "```acceptance\n" + blocks[-1].rstrip() + "\n```" if blocks else "Unavailable: task has no acceptance block."


def diff_sections(text):
    sections = re.split(r"(?=^diff --git )", text, flags=re.M)
    kept, omitted, stats = [], [], []
    for section in sections:
        if not section.strip():
            continue
        if section.strip() == "# SPDX-License-Identifier: GPL-3.0-only":
            continue
        first = section.splitlines()[0]
        if not first.startswith("diff --git "):
            raise ValueError("expected a Git unified diff with diff --git headers")
        words = shlex.split(first)
        if len(words) != 4:
            raise ValueError("invalid diff header")
        name = words[3].removeprefix("b/")
        binary = "GIT binary patch" in section or "\nBinary files " in section
        additions = sum(line.startswith("+") and not line.startswith("+++") for line in section.splitlines())
        deletions = sum(line.startswith("-") and not line.startswith("---") for line in section.splitlines())
        stats.append(f"- `{name}`: " + ("binary" if binary else f"+{additions} / -{deletions}"))
        parts = set(Path(name).parts)
        reason = "binary" if binary else "fixture" if "fixtures" in parts else "generated" if (
            parts & {"build", "build_reports", "generated", "__pycache__"}
            or name.endswith((".min.js", ".map", ".pyc"))) else None
        if reason:
            omitted.append(f"- `{name}` ({reason})")
        else:
            kept.append((name, section))
    return kept, omitted, stats


def local_accept(root, sha):
    if not sha:
        return None
    paths = sorted((root / "build_reports").glob("accept-*.json"), key=lambda p: p.stat().st_mtime, reverse=True)
    for path in paths:
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
            if record.get("commit") != sha:
                continue
            rows = [f"Local `{path.name}` at exact SHA `{sha}`: {record.get('status', 'unknown')}"]
            rows.extend(f"- {item['command']}: {item.get('status', 'unknown')}" for item in record.get("commands", []))
            rows.append("External entries remain pending evidence; a local result does not verify them.")
            return "\n".join(rows)
        except (OSError, ValueError, KeyError, TypeError):
            continue
    return None


def ci_summary(repo, sha):
    checks = api(f"repos/{repo}/commits/{sha}/check-runs?per_page=100")
    latest = {}
    for check in checks.get("check_runs", []):
        name = check["name"]
        if name not in latest or check.get("started_at", "") > latest[name].get("started_at", ""):
            latest[name] = check
    rows = [f"Exact PR head `{sha}`; latest returned check per name:"]
    rows.extend(f"- {name}: {c.get('conclusion') or c.get('status', 'unknown')} ({c.get('html_url', 'no URL')})"
                for name, c in sorted(latest.items()))
    if not latest:
        rows.append("Unavailable: no checks returned for this SHA.")
    if checks.get("total_count", 0) > 100:
        rows.append("Warning: API result limited to 100 checks; older checks omitted.")
    statuses = api(f"repos/{repo}/commits/{sha}/status?per_page=100")
    rows.extend(f"- {s['context']}: {s['state']} ({s.get('target_url') or 'no URL'})" for s in statuses.get("statuses", []))
    accept = [f"{name}: {c.get('conclusion') or c.get('status')}: {c.get('output', {}).get('summary') or 'No summary supplied.'}"
              for name, c in latest.items() if "accept" in name.lower()]
    return "\n".join(rows), "\n".join(accept) or "Unavailable: no accept check summary returned."


def task_path(root, body):
    match = re.search(r"docs/tasks/[A-Za-z0-9_.-]+\.md", body)
    return root / match[0] if match else None


def collect(args, root):
    sha, ci, accept = None, "Unavailable: offline mode; no CI request made.", None
    task = args.task
    if args.diff:
        diff = args.diff.read_text(encoding="utf-8")
        title, intent = "Offline review", "Supplied diff; PR title and intent unavailable offline."
    else:
        pr = api(f"repos/{args.repo}/pulls/{args.pr}")
        title, intent, sha = pr["title"], pr.get("body") or "No intent supplied.", pr["head"]["sha"]
        diff = api(f"repos/{args.repo}/compare/{pr['base']['sha']}...{sha}", diff=True)
        task = task or task_path(root, intent)
        if task is None:
            issue = re.search(r"(?:Closes|Fixes|Resolves)\s+#(\d+)", intent, re.I)
            if issue:
                task = task_path(root, api(f"repos/{args.repo}/issues/{issue[1]}").get("body") or "")
        try:
            ci, accept = ci_summary(args.repo, sha)
        except (OSError, ValueError):
            ci, accept = "Unavailable: CI REST request failed for the exact head.", None
    accept = local_accept(root, sha) or accept or "Unavailable: no exact-SHA accept report."
    text = args.task.read_text(encoding="utf-8") if args.task else None
    if not args.diff and task and not args.task:
        try:
            document = api(f"repos/{args.repo}/contents/{task.relative_to(root).as_posix()}?ref={sha}")
            if document.get("encoding") != "base64":
                raise ValueError("unsupported task content encoding")
            text = base64.b64decode(document["content"], validate=False).decode("utf-8")
        except (OSError, ValueError, KeyError):
            text = None
    return title, intent, sha, diff, acceptance(text), ci, accept


def clip(text, limit):
    raw = text.encode("utf-8")
    return text if len(raw) <= limit else raw[:limit].decode("utf-8", errors="ignore") + "\n[trimmed]"


def make_pack(args, evidence):
    title, intent, sha, diff, block, ci, accept = evidence
    kept, omitted, stats = diff_sections(diff)
    core = (PROMPTS[args.prompt] + f"\n\n# {clip(title, 300)}\n\nHead: `{sha or 'unavailable'}`\n\n"
            + "## Intent\n" + clip(intent, 1500) + "\n\n## Diff stat\n" + clip("\n".join(stats), 3000)
            + "\n\n## Trimmed noise\n" + clip("\n".join(omitted) or "None.", 3000)
            + "\n\n## Task acceptance (last block)\n" + block + "\n\n## Latest CI\n" + clip(ci, 3000)
            + "\n\n## Accept summary\n" + clip(accept, 3000) + "\n\n" + CHECKLIST)
    budget = args.max_bytes // args.split * args.split - 3 * args.split
    # Keep the evidence and checklist; drop only diff detail when reaching the cap.
    marker = "\n[Diff truncated by size cap; omitted detail may need separate review.]\n"
    prefix, suffix = "\n## Trimmed diff\n````diff\n", "\n````\n"
    available = budget - len((core + prefix + suffix + marker).encode("utf-8"))
    if available < 0:
        raise ValueError("metadata and task acceptance exceed size cap; increase --max-bytes")
    selected = "".join(section for _, section in kept)
    raw = selected.encode("utf-8")
    if len(raw) > available:
        selected = raw[:available].decode("utf-8", errors="ignore") + marker
    return core + prefix + selected + suffix


def write_pack(text, output, stem, count, cap):
    output.mkdir(parents=True, exist_ok=True)
    raw, paths = text.encode("utf-8"), []
    for index in range(count):
        target = min(cap // count, math.ceil(len(raw) / (count - index)))
        part = raw[:target].decode("utf-8", errors="ignore")
        raw = raw[len(part.encode("utf-8")):]
        path = output / (f"{stem}.md" if count == 1 else f"{stem}-{index + 1}-of-{count}.md")
        path.write_bytes(part.encode("utf-8"))
        paths.append(path)
    if raw:
        raise ValueError("split could not fit text within byte caps")
    return paths


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pr", type=int, nargs="?")
    parser.add_argument("--repo", default="kalharbi11/wirescape")
    parser.add_argument("--diff", type=Path)
    parser.add_argument("--task", type=Path)
    parser.add_argument("--prompt", choices=PROMPTS, default="chatgpt")
    parser.add_argument("--split", type=int, default=1)
    parser.add_argument("--max-bytes", "--cap", type=int, default=60 * 1024)
    parser.add_argument("--output", type=Path, default=ROOT / "build_reports/review")
    args = parser.parse_args(argv)
    try:
        if not args.diff and (args.pr is None or args.pr <= 0):
            raise ValueError("provide a positive PR number or --diff")
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo):
            raise ValueError("expected owner/repository")
        if args.split < 1 or args.max_bytes // args.split < 512:
            raise ValueError("split must be positive with at least 512 bytes per part")
        pack = make_pack(args, collect(args, ROOT))
        paths = write_pack(pack, args.output, f"pr-{args.pr}" if args.pr else "offline", args.split, args.max_bytes)
        for path in paths:
            print(f"{path}: {path.stat().st_size} bytes")
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
