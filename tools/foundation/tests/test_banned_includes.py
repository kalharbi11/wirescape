# SPDX-License-Identifier: GPL-3.0-only
"""Keep allocation, I/O and scheduler facilities out of portable DSP sources."""
from pathlib import Path
import re
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BANNED = ('vector', 'string', 'thread', 'mutex', 'iostream', 'filesystem', 'map',
          'unordered_map', 'list', 'deque', 'functional', 'sstream', 'fstream', 'chrono')
PATTERN = re.compile(r'^\s*#\s*include\s*[<"](' + '|'.join(BANNED) + r')[>"]', re.M)


def violations(root):
    return [(str(file.relative_to(root)), match.group(1))
            for directory in ('native/modules2', 'native/sdk', 'native/voice')
            for file in sorted((root / directory).rglob('*'))
            if file.is_file() and file.suffix in ('.h', '.hpp', '.c', '.cpp', '.cc')
            for match in PATTERN.finditer(re.sub(r'/\*.*?\*/|//[^\n]*', '', file.read_text(), flags=re.S))]


class IncludeTests(unittest.TestCase):
    def test_portable_dsp_sources(self):
        self.assertEqual(violations(ROOT), [])

    def test_guard_rejects_every_banned_header_in_each_source_tree(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for tree in ('modules2', 'sdk', 'voice'):
                file = root / 'native' / tree / 'nested/test.cpp'
                file.parent.mkdir(parents=True)
                file.write_text('\n'.join(f'# include <{name}>' for name in BANNED))
            self.assertEqual(len(violations(root)), 3 * len(BANNED))
