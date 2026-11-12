#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decide whether a PR may get GitHub auto-merge (docs/WORKFLOW.md §4).

The gate only accepts owner-authored exact-head standalone approvals and never
bypasses the required GitHub checks. New approvals must be signed in the same
comment; legacy named approvals remain valid during the 30-day migration.
"""
import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys

# These paths require an approval regardless of tier. The separate guard checks
# allow-protected and the issue's "Protected path named:" authorization.
PROTECTED_PATHS = ('AGENTS.md', 'CLAUDE.md', 'tools/ci/**', '.github/**', 'docs/specs/**', 'docs/PLAN.md',
                   'native/include/wirescape/*.h', 'LICENSE*', 'LICENSES/**', 'docs/history/**', 'legacy/**',
                   'tools/merge_gate.py')
CLAUDE_OK = 'claude: approved for auto-merge'
CODEX_OK = 'codex: approved for auto-merge'
CHATGPT_OK = 'chatgpt: approved for auto-merge'
PREREVIEW_OK = 'chatgpt: pre-review passed'  # Historical pre-review is not an approval.


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
            yield line.rstrip()


def approved(comments, owner, phrase, sha7):
    """Retain exact-head, standalone, owner-authored legacy approval parsing."""
    wanted = f'{phrase} {sha7}'.casefold()
    return any(c.get('user', {}).get('login') == owner
               and any(line.casefold() == wanted for line in approval_lines(c.get('body')))
               for c in comments)


def signed_approval(comments, owner, sha7):
    """Return the signer for a current-head neutral approval, or None.

    Both the approval and a later signature must be standalone and visible in
    the SAME owner-authored comment. A signature on a different comment (or in
    a quote, list, code fence or HTML comment) cannot grant approval.
    """
    wanted = f'Approved for merge {sha7}'.casefold()
    for comment in comments:
        if comment.get('user', {}).get('login') != owner:
            continue
        lines = list(approval_lines(comment.get('body')))
        for index, line in enumerate(lines):
            if line.casefold() != wanted:
                continue
            for following in lines[index + 1:]:
                signature = re.fullmatch(r'Signed:[ \t]+(.+),[ \t]+\d{4}-\d{2}-\d{2}[ \t]*',
                                         following, flags=re.IGNORECASE)
                if signature and signature.group(1).strip():
                    return signature.group(1).strip()
    return None


def matching(files, rules):
    return sorted(f for f in files if any(fnmatch.fnmatchcase(f, rule) for rule in rules))


def decide(pr, files, comments, owner):
    """Return (ok, reason). Only explicit, unprotected T0 Markdown PRs bypass approval."""
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
    protected = matching(files, PROTECTED_PATHS)
    t0_markdown_only = ('tier/T0' in labels and
                        not ({'tier/T1', 'tier/T2'} & labels) and
                        bool(files) and all(path.endswith('.md') for path in files))
    if t0_markdown_only and not protected and 'milestone-review' not in labels:
        return True, 'T0: explicit tier/T0 Markdown-only PR; required checks still apply'

    signer = signed_approval(comments, owner, sha7)
    if signer:
        return True, f'Approved for merge {sha7}; Signed: {signer}'

    # Old owner-authored lines remain valid for 30 days; removal is a follow-up,
    # not a timer that would break existing in-flight approvals without review.
    for phrase, name in ((CLAUDE_OK, 'Claude'), (CHATGPT_OK, 'ChatGPT'), (CODEX_OK, 'Codex')):
        if approved(comments, owner, phrase, sha7):
            return True, f'Legacy {name} approved {sha7}'

    requirement = ', '.join(protected) if protected else (
        'milestone-review' if 'milestone-review' in labels else
        'T2' if 'tier/T2' in labels else
        'T1' if 'tier/T1' in labels else
        'explicit tier/T0 Markdown-only diff (all other PRs need signed approval)')
    return False, f'needs owner-authored "Approved for merge {sha7}" and Signed: line ({requirement})'


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
    print(f"head: {pr['head']['sha']}")
    print(('ok: ' if ok else 'wait: ') + reason)
    return 0 if ok else 3


if __name__ == '__main__':
    sys.exit(main())
