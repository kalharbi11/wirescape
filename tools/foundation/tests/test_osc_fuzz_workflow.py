# SPDX-License-Identifier: GPL-3.0-only
"""Device-off: the non-required OSC fuzz workflow keeps Keel's #676 CI-policy limits."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = ROOT / '.github/workflows/osc_fuzz.yml'
TASK = ROOT / 'docs/tasks/Wh_FUZZ.md'


def block(text, key):
    """Lines indented under a top-level `key:` until the next top-level key."""
    match = re.search(rf'(?m)^{key}:[^\n]*\n((?:^(?:[ \t].*)?\n)*)', text)
    return match.group(1) if match else ''


class OscFuzzWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text(encoding='utf-8')
        self.code = '\n'.join(line for line in self.text.splitlines() if not line.lstrip().startswith('#'))

    def test_triggers_are_path_filtered_pull_request_main_push_and_dispatch(self):
        on = block(self.text, 'on')
        self.assertEqual(sorted(re.findall(r'(?m)^  (\w+):', on)), ['pull_request', 'push', 'workflow_dispatch'])
        self.assertEqual(re.findall(r"(?m)^      - '([^']+)'", on),
                         ['native/io/osc/**', '.github/workflows/osc_fuzz.yml'] * 2)
        self.assertIn('branches: [main]', on)
        self.assertNotIn('pull_request_target', self.text)

    def test_read_only_token_no_secrets_and_bounded_job(self):
        self.assertEqual(block(self.text, 'permissions').strip(), 'contents: read')
        self.assertNotRegex(self.code, r'secrets\.|github\.token|GITHUB_TOKEN|issues:\s*write')
        self.assertEqual(re.findall(r'timeout-minutes:\s*(\d+)', self.text), ['15'])
        self.assertIn('cancel-in-progress: true', block(self.text, 'concurrency'))
        self.assertIn('retention-days: 14', self.text)

    def test_runs_exactly_the_94_acceptance_fuzz_commands(self):
        fence = re.search(r'```acceptance\n(.*?)```', TASK.read_text(encoding='utf-8'), re.S).group(1)
        commands = [line.strip()[2:] for line in fence.splitlines()
                    if line.strip().startswith('- ') and ('fuzz' in line)]
        self.assertEqual(len(commands), 4)
        for command in commands:
            self.assertIn(command, self.text)
        self.assertEqual(self.text.count('-max_total_time=60'), 2)

