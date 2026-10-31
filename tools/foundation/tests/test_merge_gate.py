# SPDX-License-Identifier: GPL-3.0-only
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import merge_gate as gate

SHA = 'abc1234def'


def pr(*labels, base='main', draft=False, ref='task/1-x'):
    return {'state': 'open', 'draft': draft, 'base': {'ref': base}, 'head': {'sha': SHA, 'ref': ref},
            'labels': [{'name': name} for name in ('status/ready-to-merge',) + labels]}


def say(body, login='owner'):
    return {'user': {'login': login}, 'body': body}


class MergeGateTests(unittest.TestCase):
    def check(self, p, files=('native/x.cpp',), comments=()):
        return gate.decide(p, list(files), list(comments), 'owner')[0]

    def test_t0_needs_only_ready_label_on_main(self):
        self.assertTrue(self.check(pr()))
        self.assertFalse(self.check(pr(base='task/1-x')))
        self.assertFalse(self.check(pr(draft=True)))
        self.assertFalse(self.check(pr('status/blocked')))
        p = pr()
        p['labels'] = []
        self.assertFalse(self.check(p))

    def test_t1_needs_prereview_on_current_head_by_owner(self):
        self.assertFalse(self.check(pr('tier/T1')))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('ChatGPT: pre-review passed 0000000')]))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('ChatGPT: pre-review passed abc1234', 'stranger')]))
        self.assertTrue(self.check(pr('tier/T1'), comments=[say('ChatGPT: pre-review passed abc1234')]))

    def test_t2_accepts_chatgpt_approval_outside_claude_paths(self):
        self.assertFalse(self.check(pr('tier/T2'), comments=[say('ChatGPT: pre-review passed abc1234')]))
        self.assertTrue(self.check(pr('tier/T2'), comments=[say('ChatGPT: approved for auto-merge abc1234')]))

    def test_claude_paths_and_milestones_need_claude(self):
        chatgpt = [say('ChatGPT: approved for auto-merge abc1234')]
        for path in ('native/include/wirescape/module2.h', '.github/workflows/ci.yml', 'tools/ci/guard.py',
                     'AGENTS.md', 'docs/specs/ABI2_PLAN.md', 'tools/merge_gate.py'):
            self.assertFalse(self.check(pr(), files=[path], comments=chatgpt), path)
        self.assertFalse(self.check(pr('milestone-review'), comments=chatgpt))
        self.assertTrue(self.check(pr('milestone-review'), files=['AGENTS.md'],
                                   comments=[say('Claude: approved for auto-merge abc1234')]))


class ApprovalsV3Tests(unittest.TestCase):
    def check(self, p, files=('native/x.cpp',), comments=()):
        return gate.decide(p, list(files), list(comments), 'owner')[0]

    def test_codex_line_finishes_t1_but_not_t2_or_claude_paths(self):
        codex = [say('Codex: approved for auto-merge abc1234')]
        self.assertTrue(self.check(pr('tier/T1'), comments=codex))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('Codex: approved for auto-merge 0000000')]))
        self.assertFalse(self.check(pr('tier/T2'), comments=codex))
        self.assertFalse(self.check(pr('tier/T1'), files=['docs/specs/ABI2_PLAN.md'], comments=codex))
        self.assertFalse(self.check(pr('tier/T1', 'status/blocked'), comments=codex))

    def test_claude_line_finishes_any_tier(self):
        claude = [say('Claude: approved for auto-merge abc1234')]
        for p in (pr('tier/T1'), pr('tier/T2'), pr('milestone-review')):
            self.assertTrue(self.check(p, comments=claude))
        self.assertTrue(self.check(pr(ref='claude/x'), files=['docs/PLAN.md', 'AGENTS.md'], comments=claude))

    def test_claude_authored_control_change_needs_independent_chatgpt(self):
        claude = say('Claude: approved for auto-merge abc1234')
        chatgpt = say('ChatGPT: approved for auto-merge abc1234')
        for path in ('tools/merge_gate.py', '.github/workflows/ci.yml', 'tools/ci/guard.py',
                     'native/include/wirescape/module2.h'):
            self.assertFalse(self.check(pr(ref='claude/x'), files=[path], comments=[claude]), path)
            self.assertFalse(self.check(pr(ref='claude/x'), files=[path], comments=[chatgpt]), path)
            self.assertTrue(self.check(pr(ref='claude/x'), files=[path], comments=[claude, chatgpt]), path)
            self.assertTrue(self.check(pr(), files=[path], comments=[claude]), path)


class StrictApprovalLineTests(unittest.TestCase):
    def ok(self, body):
        return gate.approved([say(body)], 'owner', gate.CHATGPT_OK, 'abc1234')

    def test_standalone_line_counts(self):
        self.assertTrue(self.ok('Review text.\n\nChatGPT: approved for auto-merge abc1234'))
        self.assertTrue(self.ok('CHATGPT: APPROVED FOR AUTO-MERGE ABC1234   '))

    def test_quoted_fenced_negated_or_detached_lines_do_not_count(self):
        self.assertFalse(self.ok('Review request only; no approval yet.\n> ChatGPT: approved for auto-merge abc1234'))
        self.assertFalse(self.ok('```\nChatGPT: approved for auto-merge abc1234\n```'))
        self.assertFalse(self.ok('Please post "ChatGPT: approved for auto-merge abc1234" when done'))
        self.assertFalse(self.ok('Not yet: ChatGPT: approved for auto-merge abc1234'))
        self.assertFalse(self.ok('ChatGPT: approved for auto-merge\nhead abc1234'))
        self.assertFalse(self.ok('ChatGPT: approved for auto-merge 0000000'))

    def test_markdown_context_and_nonexact_lines_fail_closed(self):
        line = gate.CHATGPT_OK + ' abc1234'
        examples = [
            '    ' + line, '\t' + line, ' ' + line, '`' + line + '`',
            '**' + line + '**', '"' + line + '"', line + '.', line.replace(' abc', ' on abc'),
            '- ' + line, '* ' + line, '1. ' + line,
            '- Example\n' + line, '> Example\n' + line,
            '~~~text\n' + line + '\n~~~',
            '````text\n```\n' + line + '\n````',
            '```text\n~~~\n' + line + '\n```',
            '```text\n```not-a-close\n' + line + '\n```',
        ]
        for body in examples:
            with self.subTest(body=body):
                if body.startswith(('    ', '\t')):
                    self.assertEqual(list(gate.approval_lines(body)), [])
                self.assertFalse(self.ok(body))
                self.assertFalse(gate.decide(pr('tier/T2'), ['native/x.cpp'], [say(body)], 'owner')[0])

    def test_hidden_html_comments_never_approve(self):
        for phrase, labels, files in ((gate.PREREVIEW_OK, ('tier/T1',), ['native/x.cpp']),
                                      (gate.CHATGPT_OK, ('tier/T2',), ['native/x.cpp']),
                                      (gate.CLAUDE_OK, ('tier/T2',), ['tools/merge_gate.py'])):
            line = phrase + ' abc1234'
            for body in ('<!-- ' + line + ' -->', '<!--\n' + line + '\n-->',
                         '<!-- open\n\n' + line, '<!-- first --> <!-- second\n' + line + '\n-->',
                         '<!-- open\n```\n' + line + '\n-->'):
                with self.subTest(body=body, phrase=phrase):
                    self.assertFalse(gate.approved([say(body)], 'owner', phrase, 'abc1234'))
                    self.assertFalse(gate.decide(pr(*labels), files, [say(body)], 'owner')[0])
            self.assertTrue(gate.decide(pr(*labels), files, [say('<!-- closed -->\n' + line)], 'owner')[0])
            self.assertTrue(gate.decide(pr(*labels), files, [say('<!-- open\n-->\n' + line)], 'owner')[0])

    def test_valid_exact_tier_lines_and_context_exit(self):
        for phrase, labels, files in ((gate.PREREVIEW_OK, ('tier/T1',), ['native/x.cpp']),
                                      (gate.CHATGPT_OK, ('tier/T2',), ['native/x.cpp']),
                                      (gate.CLAUDE_OK, ('tier/T2',), ['tools/merge_gate.py'])):
            line = phrase + ' abc1234'
            self.assertTrue(gate.decide(pr(*labels), files, [say(line)], 'owner')[0])
            self.assertFalse(gate.decide(pr(*labels), files, [say('    ' + line)], 'owner')[0])
            self.assertFalse(gate.decide(pr(*labels), files, [say(line, 'stranger')], 'owner')[0])
        line = gate.CHATGPT_OK + ' abc1234'
        self.assertTrue(self.ok('~~~\nexample\n~~~\n' + line))
        self.assertTrue(self.ok('- Example\n\n' + line))


if __name__ == '__main__':
    unittest.main()
