# SPDX-License-Identifier: GPL-3.0-only
"""Report stale PLAN 'Now' references without changing project files."""
import argparse
from datetime import datetime, timezone, timedelta
import json
import os
from pathlib import Path
import re
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
REF = re.compile(r"(?<!\w)#(\d+)\b")
MAIN = re.compile(r"\bmain\s*=\s*`?([0-9a-f]{7,40})\b", re.I)


def now_section(plan):
    match = re.search(r"^## Now[^\n]*\n(.*?)(?=^## |\Z)", plan, re.M | re.S)
    if not match:
        raise ValueError("docs/PLAN.md has no '## Now' section")
    return match.group(1).rstrip("\n").splitlines()


def references(lines):
    """Yield (number, context) for all references, including grouped PRs."""
    context = ""
    for line in lines:
        if line.startswith("- "):
            if line.startswith("- **Open PRs:**"):
                context = "open"
            elif line.startswith("- **Next 3:**"):
                context = "next"
            else:
                context = ""
        for number in REF.findall(line):
            yield number, context


def request_json(url, token=""):
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "wirescape-docs-fresh"}
    if token:
        headers["Authorization"] = "Bearer " + token
    with urlopen(Request(url, headers=headers), timeout=15) as response:
        return json.load(response)


def check(plan, get_issue, get_commit, today):
    lines = now_section(plan)
    problems = []
    if len(lines) > 30:
        problems.append(f"PLAN Now has {len(lines)} lines (maximum 30)")
    found = {}
    for number, context in references(lines):
        found.setdefault(number, set()).add(context)
    for number, contexts in found.items():
        try:
            issue = get_issue(number)
        except Exception as error:
            problems.append(f"#{number}: cannot check GitHub state ({error})")
            continue
        is_pr = bool(issue.get("pull_request"))
        if "open" in contexts and is_pr and issue.get("state") != "open":
            problems.append(f"#{number}: closed PR listed under Open PRs")
        if "next" in contexts and not is_pr and issue.get("state") == "closed":
            problems.append(f"#{number}: closed issue listed under Next 3")
    for line in lines:
        match = MAIN.search(line)
        if not match:
            continue
        sha = match.group(1)
        try:
            commit = get_commit(sha)
            stamp = commit["commit"]["committer"]["date"]
            date = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
            if today - date > timedelta(days=3):
                problems.append(f"main = {sha}: SHA is older than 3 days")
        except Exception as error:
            problems.append(f"main = {sha}: cannot check commit date ({error})")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", type=Path, help="JSON fixture with plan/issues/commits/now")
    parser.add_argument("--plan", type=Path, default=ROOT / "docs/PLAN.md")
    parser.add_argument("--repo", default="kalharbi11/wirescape")
    args = parser.parse_args()
    if args.offline:
        fixture = json.loads(args.offline.read_text(encoding="utf-8"))
        plan = fixture["plan"]
        issues = fixture["issues"]
        commits = fixture.get("commits", {})
        get_issue = lambda n: issues[n]
        get_commit = lambda sha: commits[sha]
        today = datetime.fromisoformat(fixture["now"].replace("Z", "+00:00"))
    else:
        plan = args.plan.read_text(encoding="utf-8")
        base = f"https://api.github.com/repos/{args.repo}"
        token = os.environ.get("GITHUB_TOKEN", "")
        get_issue = lambda n: request_json(f"{base}/issues/{n}", token)
        get_commit = lambda sha: request_json(f"{base}/commits/{sha}", token)
        today = datetime.now(timezone.utc)
    try:
        problems = check(plan, get_issue, get_commit, today)
    except ValueError as error:
        problems = [str(error)]
    if problems:
        for problem in problems:
            print("STALE: " + problem)
        return 1
    print("PLAN Now references, age and length: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
