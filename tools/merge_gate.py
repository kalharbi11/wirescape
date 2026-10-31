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
# Merge-control and contract paths: a Claude-authored change here also needs an independent ChatGPT line.
CONTROL_PATHS = ('tools/merge_gate.py', 'tools/ci/**', '.github/**', 'native/include/wirescape/*.h')
CLAUDE_OK = 'claude: approved for auto-merge'
CODEX_OK = 'codex: approved for auto-merge'
CHATGPT_OK = 'chatgpt: approved for auto-merge'
PREREVIEW_OK = 'chatgpt: pre-review passed'


def approval_lines(body):
    """Raw standalone lines; Markdown examples and lazy quote/list continuations do not count."""
    fence = None
    html_comment = False
    continuation = False
    for line in (body or '').splitlines():
        if fence:
            if re.fullmatch(' {0,3}' + re.escape(fence[0]) + '{' + str(fence[1]) + ',}[ \t]*', line):
                fence = None
            continue
        # Hidden HTML comments cannot grant approval; process every marker so
        # closing one comment and opening another on the same line stays hidden.
        if html_comment or '<!--' in line:
            cursor = 0
            while True:
                marker = '-->' if html_comment else '<!--'
                position = line.find(marker, cursor)
                if position < 0:
                    break
                html_comment = not html_comment
                cursor = position + len(marker)
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


def matching(files, rules):
    return sorted(f for f in files if any(fnmatch.fnmatchcase(f, rule) for rule in rules))


def decide(pr, files, comments, owner):
    """Return (ok, reason). Rules: docs/WORKFLOW.md §3 (approvals v3)."""
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
    says = lambda phrase: approved(comments, owner, phrase, sha7)
    claude_only = matching(files, CLAUDE_PATHS)
    if claude_only or 'milestone-review' in labels:
        if not says(CLAUDE_OK):
            return False, f'needs "Claude: approved for auto-merge {sha7}" ({", ".join(claude_only) or "milestone-review"})'
        control = matching(files, CONTROL_PATHS)
        if control and pr['head'].get('ref', '').startswith('claude/') and not says(CHATGPT_OK):
            return False, f'Claude-authored control change needs "ChatGPT: approved for auto-merge {sha7}" too'
        return True, f'Claude approved {sha7}'
    if says(CLAUDE_OK):
        return True, f'Claude approved {sha7}'
    if 'tier/T2' in labels:
        if says(CHATGPT_OK):
            return True, f'ChatGPT approved {sha7}'
        return False, f'T2 needs "ChatGPT: approved for auto-merge {sha7}" or a Claude line'
    if 'tier/T1' in labels:
        for phrase, who in ((CHATGPT_OK, 'ChatGPT'), (PREREVIEW_OK, 'ChatGPT pre-review'), (CODEX_OK, 'Codex')):
            if says(phrase):
                return True, f'T1: {who} approved {sha7}'
        return False, f'T1 needs "Codex: approved for auto-merge {sha7}" (or a ChatGPT/Claude line)'
    return True, 'T0: ready label and required checks'


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
