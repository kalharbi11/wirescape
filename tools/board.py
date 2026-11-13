# SPDX-License-Identifier: GPL-3.0-only
"""Live work board: rebuilds the auto section of the board issue (#473) from labels.

Every open issue with a status/* label lands in exactly one section, with the agent it
belongs to (`agent/<name>`), its open PR, how long it has been idle and its `Next:` line. Humans write only outside the markers; this script writes only inside.

  python tools/board.py             # print the auto section (needs GH_TOKEN or GITHUB_TOKEN)
  python tools/board.py --write 473 # replace the section in issue #473's body
  python tools/board.py --followups # finish what gate merges skip (see followups())
  python tools/board.py --watch-merges 12 # wait for armed auto-merges, then verify main (post_merge.yml)
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone

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
        raw = resp.read()
        return json.loads(raw) if raw else {}


def _all(path, token):
    items, page = [], 1
    while True:
        batch = _api(f'{path}{"&" if "?" in path else "?"}per_page=100&page={page}', token)
        items += batch
        if len(batch) < 100 or page >= 10:
            return items
        page += 1


FOLLOW_DAYS = 7
MARKER = '<!-- board:closed-by-merge #{pr} -->'


def _prose(body):
    """The body without fenced code blocks and inline code: `Closes #N` quoted there is not a directive."""
    body = re.sub(r'^(```|~~~).*?^\1[^\n]*$', '', body or '', flags=re.M | re.S)
    return re.sub(r'`[^`\n]*`', '', body)


def pending_closes(merged, now, days=FOLLOW_DAYS):
    """(issue, pr, merge sha) for every `Closes #N` in a PR merged into main in the last `days`."""
    out = []
    for pr in merged:
        if not pr.get('merged_at') or (pr.get('base') or {}).get('ref') != 'main':
            continue
        if now - _when(pr['merged_at']) > timedelta(days=days):
            continue
        for n in sorted({int(n) for n in CLOSES.findall(_prose(pr.get('body')))}):
            out.append((n, pr['number'], pr.get('merge_commit_sha') or ''))
    return out


def should_close(issue, comments, pr):
    """Open, not a PR, and never closed by this tool for `pr` before (a reopen is left alone)."""
    if 'pull_request' in issue or issue.get('state') != 'open':
        return False
    marker = MARKER.format(pr=pr)
    return not any(marker in (c.get('body') or '') for c in comments)


def stale_status(items):
    """(number, label) for every status/* label left on a closed issue or PR. Status labels describe
    live work only; GitHub keeps them when an item closes, so closed items show up as in-progress."""
    return [(item['number'], name) for item in items if item.get('state') == 'closed'
            for name in _labels(item) if name.startswith('status/')]


def followups(token, now):
    """Merges the gate makes run as GITHUB_TOKEN, and GitHub starts no follow-ups for those:
    `Closes #N` is not applied, no push CI runs on main and schedules never register. This runs on
    agents' own issue/PR events instead. Three independent steps, most important first:
    1. start main_verify.yml once per main commit (it dispatches whatever CI the commit is missing);
    2. close the issues that recent main merges name with `Closes #N`;
    3. clear status/* labels from recently closed issues and PRs, however they were closed.
    A failing step never skips a later one (#696); any failure still makes the job exit 1."""
    failed = []
    for step in (_verify_main, _close_linked, _clear_closed_status):
        try:
            step(token, now)
        except (urllib.error.URLError, OSError, KeyError, ValueError) as error:
            failed.append(step.__name__)
            print(f'board.py: {step.__name__} failed: {error}', file=sys.stderr)
    return 1 if failed else 0


def _verify_main(token, now):
    sha = _api('commits/main', token)['sha']
    runs = _api(f'actions/workflows/main_verify.yml/runs?head_sha={sha}&per_page=1', token)
    if not runs.get('total_count'):
        _api('actions/workflows/main_verify.yml/dispatches', token, 'POST', {'ref': 'main'})
        print(f'board.py: dispatched main_verify.yml for main@{sha[:7]}')


def _close_linked(token, now):
    merged = _api('pulls?state=closed&base=main&sort=updated&direction=desc&per_page=50', token)
    for n, pr, sha in pending_closes(merged, now):
        issue = _api(f'issues/{n}', token)
        if not should_close(issue, _all(f'issues/{n}/comments', token), pr):
            continue
        note = (f'Closed by #{pr} (merged as {sha[:7]}). The merge gate merges as GITHUB_TOKEN, '
                f'so GitHub did not apply `Closes #{n}`; the board workflow did.\n{MARKER.format(pr=pr)}')
        # Close first: a run cancelled in between leaves a closed issue, never a note on an open one.
        _api(f'issues/{n}', token, 'PATCH', {'state': 'closed', 'state_reason': 'completed'})
        _api(f'issues/{n}/comments', token, 'POST', {'body': note})
        print(f'board.py: closed #{n} (from #{pr})')


def _clear_closed_status(token, now):
    recent = _api('issues?state=closed&sort=updated&direction=desc&per_page=50', token)
    for n, name in stale_status(recent):
        try:
            _api(f'issues/{n}/labels/{urllib.parse.quote(name, safe="")}', token, 'DELETE')
            print(f'board.py: removed {name} from closed #{n}')
        except urllib.error.HTTPError as error:  # already removed by someone else
            if error.code != 404:
                raise


def watch_merges(token, minutes, sleep=time.sleep, clock=time.monotonic):
    """Durable post-merge wakeup (#702, gpt-reviewer High). The gate's GITHUB_TOKEN auto-merge
    starts no workflow, and an event that ran just before it saw the old main. post_merge.yml runs
    this after every CI, Guard and Merge gate run: while any open PR has auto-merge armed it waits,
    bounded by `minutes`, and as soon as one has merged it verifies main (main_verify.yml reads
    main's live head and is idempotent per SHA). It always verifies main once before returning."""
    deadline = clock() + minutes * 60
    watched, first = set(), True
    while True:
        armed = {pr['number'] for pr in _all('pulls?state=open', token) if pr.get('auto_merge')}
        merged_now = False
        # A merged PR leaves the open list, so check the ones watched last pass that are gone
        # (gpt-builder1 pre-review: with a second PR still armed it was never seen merging).
        for n in sorted(watched - armed):
            if _api(f'pulls/{n}', token).get('merged'):
                merged_now = True
                print(f'board.py: #{n} merged; verifying main')
        watched = armed
        # The first pass also verifies: a merge that landed before this watcher started is
        # never in `watched`. main_verify is idempotent per SHA, so a repeat costs one read.
        if first or merged_now or not armed or clock() >= deadline:
            _verify_main(token, None)
        first = False
        if not armed or clock() >= deadline:
            return 0
        sleep(20)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--write', type=int, metavar='ISSUE', help='update this issue body in place')
    ap.add_argument('--followups', action='store_true', help='close issues of gate merges, clear stale status labels, verify main')
    ap.add_argument('--watch-merges', type=float, metavar='MINUTES',
                    help='wait (bounded) for armed auto-merges to land, then verify main')
    args = ap.parse_args(argv)
    token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
    if not token:
        sys.exit('board.py: set GH_TOKEN or GITHUB_TOKEN')
    if args.followups:
        return followups(token, datetime.now(timezone.utc))
    if args.watch_merges is not None:
        return watch_merges(token, min(max(args.watch_merges, 0), 30))
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
