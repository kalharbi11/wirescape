# SPDX-License-Identifier: GPL-3.0-only
"""Live work board: rebuilds the auto section of the board issue (#473) from labels.

Every open issue with a status/* label lands in exactly one section, with who it is
for (`for/<agent>`), who holds it (`agent/<agent>`), its open PR and how long it has
been idle. Humans write only outside the markers; this script writes only inside.

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
from datetime import datetime, timedelta, timezone

REPO = os.environ.get('GITHUB_REPOSITORY', 'kalharbi11/wirescape')
START = '<!-- board:auto:start -->'
END = '<!-- board:auto:end -->'
STALE = timedelta(hours=24)
# Reserved for a person or lead role: never opens to builders by idling.
NOT_BUILDERS = {'owner', 'compass', 'keel', 'codex-lead', 'coda'}
CLOSES = re.compile(r'\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\s+#(\d+)', re.I)


def _labels(item):
    return [label['name'] if isinstance(label, dict) else label for label in item.get('labels', [])]


def _pick(labels, prefix):
    return [name[len(prefix):] for name in labels if name.startswith(prefix)]


def _when(text):
    return datetime.strptime(text, '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc)


def _idle(delta):
    hours = int(delta.total_seconds() // 3600)
    return f'{hours // 24}d' if hours >= 48 else f'{hours}h'


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


def section(issue, labels):
    if 'status/blocked' in labels:
        return 'blocked'
    if {'status/review', 'status/ready-to-merge'} & set(labels):
        return 'review'
    if 'status/in-progress' in labels or _pick(labels, 'agent/'):
        return 'working'
    if 'status/handoff-ready' in labels:
        return 'handoff'
    if 'status/todo' in labels:
        return 'preferred' if _pick(labels, 'for/') else 'open'
    return None


TITLES = {
    'open': ('Open: take these first', 'Not reserved for anyone. Claim, then add your `agent/<name>` label.'),
    'handoff': ('Handed off: anyone may continue', 'Read the latest handoff; continue the same PR. Names in "for" go first if they are around.'),
    'preferred': ('Reserved for a named agent', 'Others take one only when it is marked **open to all** (idle 24 h+). `for/owner`, `for/keel` etc. never open.'),
    'working': ('In progress', '**STALE** = no activity for 24 h: post `Taking over from <name>` and continue the same PR.'),
    'review': ('Waiting for review or merge', 'Builders may review T1 from another session; T2 goes to the other model family.'),
    'blocked': ('Blocked', 'Waiting for a ruling or a dependency; the issue says on what.'),
}
ORDER = ['open', 'handoff', 'preferred', 'working', 'review', 'blocked']


def render(issues, prs, now, board=473):
    links = link_prs(prs)
    rows = {key: [] for key in ORDER}
    for issue in sorted(issues, key=lambda i: i['number']):
        if issue['number'] == board or issue.get('pull_request'):
            continue
        labels = _labels(issue)
        key = section(issue, labels)
        if key is None:
            continue
        pr = links.get(issue['number'])
        last = max([_when(issue['updated_at'])] + ([_when(pr['updated_at'])] if pr else []))
        idle = now - last
        holder = ', '.join(_pick(labels, 'agent/')) or ('unnamed claim' if key == 'working' else '')
        wanted_names = _pick(labels, 'for/')
        wanted = ', '.join(wanted_names)
        builder_wanted = wanted_names and not set(wanted_names) & NOT_BUILDERS
        note = ''
        if idle >= STALE and (key == 'working' or (builder_wanted and key in ('preferred', 'handoff'))):
            note = '**STALE**' if key == 'working' else '**open to all**'
        tier = ' '.join(_pick(labels, 'tier/'))
        pr_cell = f"#{pr['number']}" + (' draft' if pr.get('draft') else '') if pr else ''
        rows[key].append(f"| #{issue['number']} {issue['title'][:70]} | {tier} | {wanted} | {holder} | "
                         f"{pr_cell} | {_idle(idle)} {note}".rstrip() + ' |')
    out = [START, f"## Live board (auto, {now:%Y-%m-%d %H:%M} UTC)",
           'Rebuilt from labels by `tools/board.py` on every issue/PR change. Do not edit between the markers; '
           'change labels instead. Rules: AGENTS.md "Agent names and the board".', '']
    for key in ORDER:
        title, hint = TITLES[key]
        out.append(f"### {title} ({len(rows[key])})")
        out.append(hint)
        if rows[key]:
            out += ['', '| Issue | Tier | For | Claimed by | PR | Idle |', '|---|---|---|---|---|---|', *rows[key]]
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
                                          'Accept': 'application/vnd.github+json'})
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
