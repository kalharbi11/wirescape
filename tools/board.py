# SPDX-License-Identifier: GPL-3.0-only
"""Live work board: rebuilds the auto section of the board issue (#473) from labels.

Every open issue with a status/* label lands in exactly one section, with the agent it
belongs to (`agent/<name>`), its open PR, how long it has been idle and its `Next:` line. Humans write only outside the markers; this script writes only inside.

  python tools/board.py             # print the auto section (needs GH_TOKEN or GITHUB_TOKEN)
  python tools/board.py --write 473 # replace the section in issue #473's body
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.request
from datetime import datetime, timezone

REPO = os.environ.get('GITHUB_REPOSITORY', 'kalharbi11/wirescape')
START = '<!-- board:auto:start -->'
END = '<!-- board:auto:end -->'
CLOSES = re.compile(r'\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\s+#(\d+)', re.I)
# Owner-triggered low-usage modes: a `usage/low-<family>` label on the board issue.
USAGE = {
    'claude': 'Claude usage is low: claude-builder* items and Claude-side reviews may wait unless urgent.',
    'codex': 'Codex usage is low: Codex Lead work may wait; ChatGPT agents and Coda cover what they can.',
}


def _labels(item):
    return [label['name'] if isinstance(label, dict) else label for label in item.get('labels', [])]


def _pick(labels, prefix):
    return [name[len(prefix):] for name in labels if name.startswith(prefix)]


def _when(text):
    return datetime.strptime(text, '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc)


def _idle(delta):
    hours = int(delta.total_seconds() // 3600)
    return f'{hours // 24}d' if hours >= 48 else f'{hours}h'


def next_step(body):
    """The `Next:` line of the issue body's `## Current state` block (edited in place by whoever holds it)."""
    block = re.split(r'^##\s+Current state\s*$', body or '', maxsplit=1, flags=re.M | re.I)
    if len(block) < 2:
        return ''
    for line in block[1].splitlines():
        if line.startswith('## '):
            break
        hit = re.match(r'^\s*[-*]?\s*\**Next:?\**:?\s*(.+)', line, re.I)
        if hit:
            return hit.group(1).replace('|', '/').strip()[:90]
    return ''


def link_prs(prs):
    """Map issue number -> newest open PR that says Closes #N (or names task/N- as its branch)."""
    out = {}
    for pr in sorted(prs, key=lambda p: p['updated_at']):
        nums = {int(n) for n in CLOSES.findall(pr.get('body') or '')}
        branch = re.match(r'task/(\d+)-', pr.get('head', {}).get('ref', ''))
        if branch:
            nums.add(int(branch.group(1)))
        for n in nums:
            out[n] = pr
    return out


def section(labels):
    if 'status/blocked' in labels:
        return 'blocked'
    if {'status/review', 'status/ready-to-merge'} & set(labels):
        return 'review'
    if 'status/in-progress' in labels:
        return 'working'
    if _pick(labels, 'agent/') and {'status/todo', 'status/handoff-ready'} & set(labels):
        return 'assigned'
    if {'status/todo', 'status/handoff-ready'} & set(labels):
        return 'open'
    return None


TITLES = {
    'open': ('Unassigned: any agent may take these', 'Claim one: comment, `status/in-progress`, add `agent/<your name>`.'),
    'assigned': ('Assigned, not started', 'Belongs to the named agent. Others leave it unless a lead reassigns it.'),
    'working': ('In progress', '"no name" = a claim without an `agent/` label; leads fix it.'),
    'review': ('Waiting for review or merge', 'Reviewers: AGENTS.md "Done, review, merge". The author keeps its `agent/` label until merge.'),
    'blocked': ('Blocked', 'Waiting for a ruling, the owner or a dependency; the issue says which.'),
}
ORDER = ['open', 'assigned', 'working', 'review', 'blocked']


def render(issues, prs, now, board=473):
    links = link_prs(prs)
    rows = {key: [] for key in ORDER}
    load = {}
    board_labels = []
    for issue in sorted(issues, key=lambda i: i['number']):
        if issue.get('pull_request'):
            continue
        labels = _labels(issue)
        if issue['number'] == board:
            board_labels = labels
            continue
        key = section(labels)
        if key is None:
            continue
        pr = links.get(issue['number'])
        last = max([_when(issue['updated_at'])] + ([_when(pr['updated_at'])] if pr else []))
        names = _pick(labels, 'agent/')
        for name in names:
            load.setdefault(name, []).append(f"#{issue['number']}")
        holder = ', '.join(names) or ('no name' if key == 'working' else '')
        tier = ' '.join(_pick(labels, 'tier/'))
        pr_cell = f"#{pr['number']}" + (' draft' if pr.get('draft') else '') if pr else ''
        rows[key].append(f"| #{issue['number']} {issue['title'][:70]} | {tier} | {holder} | {pr_cell} | "
                         f"{_idle(now - last)} | {next_step(issue.get('body'))} |")
    out = [START, f"## Live board (auto, {now:%Y-%m-%d %H:%M} UTC)",
           'Rebuilt from labels by `tools/board.py` on every issue/PR change. Do not edit between the markers; '
           'change labels instead. Rules: AGENTS.md "Agents and the board".', '']
    for family in _pick(board_labels, 'usage/low-'):
        out += [f"> **Low-usage mode ({family}):** {USAGE.get(family, 'set by the owner.')}", '']
    if load:
        out += ['**Who has what:** ' + ' · '.join(f"`{name}` {' '.join(items)}" for name, items in sorted(load.items())), '']
    for key in ORDER:
        title, hint = TITLES[key]
        out.append(f"### {title} ({len(rows[key])})")
        out.append(hint)
        if rows[key]:
            out += ['', '| Issue | Tier | Agent | PR | Idle | Next step |', '|---|---|---|---|---|---|', *rows[key]]
        out.append('')
    out.append(END)
    return '\n'.join(out)


def splice(body, auto):
    body = body or ''
    if START in body and END in body:
        head, rest = body.split(START, 1)
        return head + auto + rest.split(END, 1)[1]
    return auto + '\n\n' + body


def _api(path, token, method='GET', data=None):
    req = urllib.request.Request(f'https://api.github.com/repos/{REPO}/{path}', method=method,
                                 data=json.dumps(data).encode() if data else None,
                                 headers={'Authorization': f'Bearer {token}',
                                          'Accept': 'application/vnd.github+json',
                                          'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def _all(path, token):
    items, page = [], 1
    while True:
        batch = _api(f'{path}{"&" if "?" in path else "?"}per_page=100&page={page}', token)
        items += batch
        if len(batch) < 100 or page >= 10:
            return items
        page += 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--write', type=int, metavar='ISSUE', help='update this issue body in place')
    args = ap.parse_args(argv)
    token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
    if not token:
        sys.exit('board.py: set GH_TOKEN or GITHUB_TOKEN')
    issues = [i for i in _all('issues?state=open', token) if 'pull_request' not in i]
    prs = _all('pulls?state=open', token)
    auto = render(issues, prs, datetime.now(timezone.utc), board=args.write or 473)
    if not args.write:
        print(auto)
        return 0
    body = _api(f'issues/{args.write}', token)['body']
    new = splice(body, auto)
    if new != body:
        _api(f'issues/{args.write}', token, 'PATCH', {'body': new})
    print(f'board.py: issue #{args.write} {"updated" if new != body else "unchanged"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
