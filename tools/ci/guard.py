# SPDX-License-Identifier: GPL-3.0-only
"""Review changed paths and added lines without executing PR content."""
import argparse
import fnmatch
import json
import os
import re
import subprocess
from pathlib import PurePosixPath

PROTECTED = ('AGENTS.md', 'CLAUDE.md', 'docs/PLAN.md', 'docs/PLANNER_HANDOFF.md',
             'docs/specs/**', 'LICENSE*', 'LICENSES/**', '.github/**', 'tools/ci/**',
             'native/include/wirescape/*.h', 'docs/history/**', 'legacy/**')
BINARY = {'.dll', '.exe', '.zip', '.wav', '.flac', '.ttf', '.otf', '.so', '.dylib',
          '.lib', '.pdb', '.obj', '.a', '.woff', '.woff2', '.mp3', '.aif', '.aiff'}
PRIVATE = {'user_data', '.tools', 'build', 'local_backups'}
FIXTURE_AUDIO = {'.wav', '.flac'}
# Protected paths an issue may unlock by naming them; these still need the label.
LABEL_ONLY = ('AGENTS.md', 'CLAUDE.md', 'tools/ci/**', '.github/workflows/guard.yml', '.github/CODEOWNERS',
              'native/include/wirescape/*.h', 'LICENSE*', 'LICENSES/**', 'docs/history/**', 'legacy/**')
NAMED = re.compile(r'(?im)^\W*protected paths? named:(.*)$')
CLOSES = re.compile(r'(?i)\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?|refs)\s+#(\d+)')
MACHINE = re.compile(r'(?i)(?:[a-z]:[\\/]Users[\\/]|/ho' r'me/[^/\s]+(?:/|$))')


def is_test(path):
    p = PurePosixPath(path)
    return any(part.lower().startswith('test') for part in p.parts) or 'test' in p.stem.lower()


def is_spdx_exempt(path):
    p = PurePosixPath(path)
    return (
        p.suffix.lower() in {'.json', '.png', '.svg', '.ico', '.wav', '.flac'}
        or p.name.startswith('LICENSE')
        or path.startswith(('LICENSES/', 'docs/history/', 'legacy/', 'native/tests/fixtures/'))
        or path in {'.gitignore', '.gitattributes', '.github/CODEOWNERS'}
    )


def is_allowed_fixture_audio(path, suffix):
    p = PurePosixPath(path)
    return suffix in FIXTURE_AUDIO and p.parts[:3] == ('native', 'tests', 'fixtures')


def named_paths(issue_body):
    """Backticked paths or globs on 'Protected path(s) named:' lines of the linked issue."""
    return {p.strip() for line in NAMED.findall(issue_body or '') for p in re.findall(r'`([^`]+)`', line)}


def linked_issue(pr_body):
    match = CLOSES.search(pr_body or '')
    return int(match.group(1)) if match else None


def unlocked(path, named):
    return (any(fnmatch.fnmatchcase(path, rule) for rule in named)
            and not any(fnmatch.fnmatchcase(path, rule) for rule in LABEL_ONLY))


def violations(status, paths, added, labels, named=()):
    errors = []
    if 'allow-protected' not in labels and any(
            fnmatch.fnmatchcase(p, rule) and not unlocked(p, named) for p in paths for rule in PROTECTED):
        errors.append('protected path requires allow-protected (or naming it in the linked issue)')
    if status[0] in 'DR' and is_test(paths[0]) and 'allow-test-removal' not in labels:
        errors.append('test deletion/rename requires allow-test-removal')
    if status[0] != 'D':
        path = paths[-1]
        p = PurePosixPath(path)
        suffix = p.suffix.lower()
        if (suffix in BINARY and not is_allowed_fixture_audio(path, suffix)) or PRIVATE.intersection(p.parts):
            errors.append('binary/private file is forbidden')
        exempt = path.startswith(('docs/history/', 'legacy/', 'docs/tasks/M0-1', 'docs/tasks/M0-2'))
        if not exempt and any(MACHINE.search(line) for line in added):
            errors.append('machine-specific user path is forbidden')
    if status[0] == 'A':
        path = paths[-1]
        if not is_spdx_exempt(path) and not any('SPDX-License-Identifier:' in line for line in added[:5]):
            errors.append('missing SPDX header')
    return errors


def git(*args):
    return subprocess.check_output(['git', *args])


def check(base, head, labels, named=()):
    entries = iter(git('diff', '--name-status', '-z', '-M', '--merge-base', base, head).split(b'\0')[:-1])
    errors = []
    for entry in entries:
        status = entry.decode()
        paths = [next(entries).decode('utf-8', 'surrogateescape')]
        if status[0] in 'RC':
            paths.append(next(entries).decode('utf-8', 'surrogateescape'))
        patch = git('diff', '--no-ext-diff', '--no-textconv', '--unified=0', '--merge-base',
                    base, head, '--', paths[-1])
        added = [line[1:] for line in patch.decode('utf-8', 'replace').splitlines()
                 if line.startswith('+') and not line.startswith('+++')]
        errors.extend(f'{paths[-1]}: {e}' for e in violations(status, paths, added, labels, named))
    return errors


def warn_tier(body=None):
    if body is None:
        number, repo = os.environ.get('PR'), os.environ.get('GITHUB_REPOSITORY')
        if not number or not repo:
            return
        try:
            body = subprocess.check_output(['gh', 'api', f'repos/{repo}/pulls/{number}', '--jq', '.body // ""'],
                                           text=True, stderr=subprocess.DEVNULL, timeout=15)
        except (OSError, subprocess.SubprocessError):
            print('warning: PR body unavailable; Tier line was not checked')
            return
    if not re.search(r'(?im)^\s*(?:\*\*)?Tier:', body):
        print('warning: PR body has no Tier: line (choose T0, T1 or T2)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', required=True)
    parser.add_argument('--head', required=True)
    parser.add_argument('--labels-json', default='[]')
    parser.add_argument('--pr-body')
    parser.add_argument('--issue-body-file', help='body of the issue the PR closes')
    args = parser.parse_args()
    warn_tier(args.pr_body)
    named = set()
    if args.issue_body_file:
        with open(args.issue_body_file, encoding='utf-8') as body:
            named = named_paths(body.read())
        print('unlocked by issue:', sorted(named) or 'none')
    errors = check(args.base, args.head, set(json.loads(args.labels_json)), named)
    print('\n'.join(errors) if errors else 'guard: passed')
    raise SystemExit(bool(errors))
