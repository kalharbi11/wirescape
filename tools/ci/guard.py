# SPDX-License-Identifier: GPL-3.0-only
"""Review changed paths and added lines without executing PR content."""
import argparse
import fnmatch
import json
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
MACHINE = re.compile(r'(?i)(?:[a-z]:[\\/]Users[\\/]|/ho' r'me/[^/\s]+(?:/|$))')


def is_test(path):
    p = PurePosixPath(path)
    return any(part.lower().startswith('test') for part in p.parts) or 'test' in p.stem.lower()


def is_allowed_fixture_audio(path, suffix):
    p = PurePosixPath(path)
    return suffix in FIXTURE_AUDIO and p.parts[:3] == ('native', 'tests', 'fixtures')


def violations(status, paths, added, labels):
    errors = []
    if 'allow-protected' not in labels and any(
            fnmatch.fnmatchcase(p, rule) for p in paths for rule in PROTECTED):
        errors.append('protected path requires allow-protected')
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
    return errors


def git(*args):
    return subprocess.check_output(['git', *args])


def check(base, head, labels):
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
        errors.extend(f'{paths[-1]}: {e}' for e in violations(status, paths, added, labels))
    return errors


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', required=True)
    parser.add_argument('--head', required=True)
    parser.add_argument('--labels-json', default='[]')
    args = parser.parse_args()
    errors = check(args.base, args.head, set(json.loads(args.labels_json)))
    print('\n'.join(errors) if errors else 'guard: passed')
    raise SystemExit(bool(errors))
