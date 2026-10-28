#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Print bounded error context from failed GitHub Actions jobs (REST only)."""
import argparse
import json
import os
import re
from urllib.error import HTTPError
from urllib.parse import urlparse
from urllib.request import HTTPRedirectHandler, Request, build_opener

REPO = "kalharbi11/wirescape"


class SafeRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urlparse(newurl).scheme != "https":
            raise ValueError("refusing non-HTTPS log redirect")
        result = super().redirect_request(req, fp, code, msg, headers, newurl)
        if urlparse(newurl).hostname != "api.github.com":
            result.remove_header("Authorization")
        return result


def request(path, raw=False):
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise ValueError("GITHUB_TOKEN or GH_TOKEN authentication is required")
    req = Request("https://api.github.com/" + path, headers={
        "Authorization": "Bearer " + token, "Accept": "application/vnd.github+json"})
    with build_opener(SafeRedirect()).open(req, timeout=30) as response:
        body = response.read()
    return body.decode("utf-8", "replace") if raw else json.loads(body)


def errors(text, limit=80, context=2):
    lines = text.splitlines()
    selected = set()
    for index, line in enumerate(lines):
        if re.search(r"error|FAILED|Assertion", line, re.I):
            selected.update(range(max(0, index-context), min(len(lines), index+context+1)))
    output = []
    for index in sorted(selected):
        line = lines[index]
        clean = re.sub(r"^\S+Z ", "", line)
        if (re.match(r"\s*(?:env:|[A-Z_][A-Z0-9_]*\s*=)", clean)
                or re.match(r"\s+[A-Z_][A-Z0-9_]*\s*:", clean)):
            continue
        for name in ("GITHUB_TOKEN", "GH_TOKEN"):
            value = os.environ.get(name)
            if value:
                line = line.replace(value, "[REDACTED]")
        line = re.sub(r"(?:github_pat_|gh[pousr]_)[A-Za-z0-9_]+", "[REDACTED]", line)
        line = re.sub(r"(?i)(authorization\s*[:=]\s*).*", r"\1[REDACTED]", line)
        output.append(line)
    return output[-min(limit, 200):]


def self_test():
    sample = "2026-10-28T00:00:00Z configure\n2026-10-28T00:00:01Z error: missing header\nTOKEN=secret\nFAILED: compile\nfinished\n"
    result = errors(sample, context=0)
    assert result == ["2026-10-28T00:00:01Z error: missing header", "FAILED: compile"]
    assert len(errors("error\n" * 300, 300)) == 200
    assert "ghp_private" not in "\n".join(errors("error: ghp_private"))
    redirected = SafeRedirect().redirect_request(Request('https://api.github.com/logs',
        headers={'Authorization': 'Bearer sentinel'}), None, 302, '', {}, 'https://logs.example/file')
    assert redirected.get_header('Authorization') is None
    print("\n".join(result))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", nargs="?", help="PR number (or #N), or Actions run ID")
    parser.add_argument("--tail", type=int, default=80)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.target or not args.target.lstrip("#").isdigit() or args.tail <= 0:
        parser.error("provide a positive PR/run ID and --tail")
    number = args.target.lstrip("#")
    try:
        pr = request(f"repos/{REPO}/pulls/{number}")
    except HTTPError as error:
        if error.code != 404 or args.target.startswith("#"):
            raise
        runs = [{"id": number}]
    else:
        runs = request(f"repos/{REPO}/actions/runs?head_sha={pr['head']['sha']}&per_page=100")["workflow_runs"]
    remaining = min(args.tail, 200)
    for run in runs:
        for page in range(1, 101):
            jobs = request(f"repos/{REPO}/actions/runs/{run['id']}/jobs?per_page=100&page={page}")["jobs"]
            for job in jobs:
                if job["conclusion"] != "failure" or remaining <= 0:
                    continue
                lines = errors(request(f"repos/{REPO}/actions/jobs/{job['id']}/logs", raw=True), remaining)
                print("\n".join(lines))
                remaining -= len(lines)
            if len(jobs) < 100:
                break
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        # Never print request headers, signed redirect URLs or credential values.
        print(f"CI log retrieval failed ({type(error).__name__}); check API permissions and log-host access.")
        raise SystemExit(1)
