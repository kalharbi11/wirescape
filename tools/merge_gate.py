#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decide whether a PR may get GitHub auto-merge (docs/WORKFLOW.md §3b).

Agents that can only label and comment use this through .github/workflows/merge_gate.yml:
the workflow enables auto-merge when this prints "ok", and GitHub still waits for the
required checks. Only comments by the repository owner count as approvals, and each
approval must name the first 7 characters of the current head SHA.
"""
import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys

# Paths only Claude may approve: the guard's label-only list plus CI and specs.
CLAUDE_PATHS = ('AGENTS.md', 'CLAUDE.md', 'tools/ci/**', '.github/**', 'docs/specs/**', 'docs/PLAN.md',
                'native/include/wirescape/*.h', 'LICENSE*', 'LICENSES/**', 'docs/history/**', 'legacy/**',
                'tools/merge_gate.py')
CLAUDE_OK = 'claude: approved for auto-merge'
CHATGPT_OK = 'chatgpt: approved for auto-merge'
PREREVIEW_OK = 'chatgpt: pre-review passed'


def approval_lines(body):
    """Raw standalone lines; Markdown examples and lazy quote/list continuations do not count."""
    fence = None
    continuation = False
    for line in (body or '').splitlines():
        if fence:
            if re.fullmatch(' {0,3}' + re.escape(fence[0]) + '{' + str(fence[1]) + ',}[ \t]*', line):
                fence = None
            continue
        if not line.strip():
            continuation = False
            continue
        if line.startswith(('    ', '\t')):
            continue
        opening = re.match(r' {0,3}(`{3,}|~{3,})', line)
        if opening:
            marker = opening.group(1)
            fence = (marker[0], len(marker))
            continue
        if re.match(r' {0,3}(>|(?:[-+*]|[0-9]{1,9}[.)])[ \t]+)', line):
            continuation = True
            continue
        if not continuation:
            yield line.rstrip().lower()


def approved(comments, owner, phrase, sha7):
    """Require the owner and the exact current-head approval line specified by the task."""
    wanted = f'{phrase} {sha7}'.lower()
    return any(c.get('user', {}).get('login') == owner
               and wanted in approval_lines(c.get('body')) for c in comments)


def decide(pr, files, comments, owner):
    """Return (ok, reason)."""
    labels = {label['name'] for label in pr.get('labels', [])}
    if pr.get('state') != 'open' or pr.get('draft'):
        return False, 'PR is closed or a draft'
    if pr['base']['ref'] != 'main':
        return False, 'base is not main (stacked PRs wait for retargeting)'
    if 'status/ready-to-merge' not in labels:
        return False, 'no status/ready-to-merge label'
    if 'status/blocked' in labels:
        return False, 'status/blocked is set'
    sha7 = pr['head']['sha'][:7]
    if approved(comments, owner, CLAUDE_OK, sha7):
        return True, f'Claude approved {sha7}'
    claude_only = sorted(f for f in files if any(fnmatch.fnmatchcase(f, rule) for rule in CLAUDE_PATHS))
    if claude_only or 'milestone-review' in labels:
        return False, f'needs "Claude: approved for auto-merge {sha7}" ({", ".join(claude_only) or "milestone-review"})'
    if approved(comments, owner, CHATGPT_OK, sha7):
        return True, f'ChatGPT approved {sha7}'
    if 'tier/T2' in labels:
        return False, f'T2 needs "ChatGPT: approved for auto-merge {sha7}" after a second ChatGPT pass'
    if 'tier/T1' in labels and not approved(comments, owner, PREREVIEW_OK, sha7):
        return False, f'T1 needs "ChatGPT: pre-review passed {sha7}"'
    return True, 'T0/T1 conditions met'


def gh_json(path):
    out = subprocess.run(['gh', 'api', '--paginate', '--slurp', path], check=True, capture_output=True, text=True)
    pages = json.loads(out.stdout)
    return [item for page in pages for item in page] if isinstance(pages[0], list) else pages[0]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default=os.environ.get('GITHUB_REPOSITORY'))
    parser.add_argument('--pr', type=int, required=True)
    args = parser.parse_args(argv)
    base = f'repos/{args.repo}'
    pr = gh_json(f'{base}/pulls/{args.pr}')
    files = [f['filename'] for f in gh_json(f'{base}/pulls/{args.pr}/files?per_page=100')]
    comments = gh_json(f'{base}/issues/{args.pr}/comments?per_page=100')
    ok, reason = decide(pr, files, comments, args.repo.split('/')[0])
    print(('ok: ' if ok else 'wait: ') + reason)
    return 0 if ok else 3


if __name__ == '__main__':
    sys.exit(main())
