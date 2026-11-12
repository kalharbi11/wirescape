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

    def test_explicit_markdown_only_t0_needs_only_ready_label_on_main(self):
        md = ('docs/README.md', 'docs/tasks/TASK.md')
        self.assertTrue(self.check(pr('tier/T0'), files=md))
        self.assertFalse(self.check(pr('tier/T0', base='task/1-x'), files=md))
        self.assertFalse(self.check(pr('tier/T0', draft=True), files=md))
        self.assertFalse(self.check(pr('tier/T0', 'status/blocked'), files=md))
        p = pr('tier/T0')
        p['labels'] = []
        self.assertFalse(self.check(p, files=md))

    def test_t0_bypass_fails_closed_for_code_missing_or_conflicting_tier(self):
        docs = ('docs/README.md',)
        self.assertFalse(self.check(pr(), files=docs))
        self.assertFalse(self.check(pr(), files=('native/x.cpp',)))
        self.assertFalse(self.check(pr('tier/T0'), files=('native/x.cpp',)))
        self.assertFalse(self.check(pr('tier/T0'), files=('docs/README.md', 'native/x.cpp')))
        self.assertFalse(self.check(pr('tier/T0'), files=()))
        self.assertFalse(self.check(pr('tier/T0', 'tier/T2'), files=docs))
        self.assertFalse(self.check(pr('tier/T0', 'tier/T1'), files=docs))
        self.assertFalse(self.check(pr('tier/T0', 'milestone-review'), files=docs))
        approval = [say('Approved for merge abc1234\nSigned: builder-chat-2 (builder), 2026-11-12')]
        self.assertTrue(self.check(pr('tier/T0'), files=('native/x.cpp',), comments=approval))

    def test_t1_needs_real_approval_on_current_head_by_owner(self):
        self.assertFalse(self.check(pr('tier/T1')))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('ChatGPT: pre-review passed abc1234')]))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('Codex: approved for auto-merge 0000000')]))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('Codex: approved for auto-merge abc1234', 'stranger')]))
        self.assertTrue(self.check(pr('tier/T1'), comments=[say('Codex: approved for auto-merge abc1234')]))

    def test_t2_accepts_chatgpt_approval_outside_claude_paths(self):
        self.assertFalse(self.check(pr('tier/T2'), comments=[say('ChatGPT: pre-review passed abc1234')]))
        self.assertTrue(self.check(pr('tier/T2'), comments=[say('ChatGPT: approved for auto-merge abc1234')]))

    def test_protected_paths_and_milestones_need_approval_not_one_family(self):
        chatgpt = [say('ChatGPT: approved for auto-merge abc1234')]
        for path in ('native/include/wirescape/module2.h', '.github/workflows/ci.yml', 'tools/ci/guard.py',
                     'AGENTS.md', 'docs/specs/ABI2_PLAN.md', 'tools/merge_gate.py'):
            self.assertFalse(self.check(pr(), files=[path]), path)
            self.assertTrue(self.check(pr(), files=[path], comments=chatgpt), path)
        self.assertFalse(self.check(pr('milestone-review')))
        self.assertTrue(self.check(pr('milestone-review'), files=['AGENTS.md'], comments=chatgpt))


class ApprovalsV3Tests(unittest.TestCase):
    def check(self, p, files=('native/x.cpp',), comments=()):
        return gate.decide(p, list(files), list(comments), 'owner')[0]

    def test_legacy_codex_line_finishes_all_tiers_and_protected(self):
        codex = [say('Codex: approved for auto-merge abc1234')]
        self.assertTrue(self.check(pr('tier/T1'), comments=codex))
        self.assertFalse(self.check(pr('tier/T1'), comments=[say('Codex: approved for auto-merge 0000000')]))
        self.assertTrue(self.check(pr('tier/T2'), comments=codex))
        self.assertTrue(self.check(pr('tier/T1'), files=['docs/specs/ABI2_PLAN.md'], comments=codex))
        self.assertFalse(self.check(pr('tier/T1', 'status/blocked'), comments=codex))

    def test_claude_line_finishes_any_tier(self):
        claude = [say('Claude: approved for auto-merge abc1234')]
        for p in (pr('tier/T1'), pr('tier/T2'), pr('milestone-review')):
            self.assertTrue(self.check(p, comments=claude))
        self.assertTrue(self.check(pr(ref='claude/x'), files=['docs/PLAN.md', 'AGENTS.md'], comments=claude))

    def test_control_change_needs_one_approval_cross_family_review_is_external(self):
        claude = say('Claude: approved for auto-merge abc1234')
        chatgpt = say('ChatGPT: approved for auto-merge abc1234')
        for path in ('tools/merge_gate.py', '.github/workflows/ci.yml', 'tools/ci/guard.py',
                     'native/include/wirescape/module2.h'):
            self.assertFalse(self.check(pr(ref='claude/x'), files=[path]), path)
            self.assertTrue(self.check(pr(ref='claude/x'), files=[path], comments=[claude]), path)
            self.assertTrue(self.check(pr(ref='claude/x'), files=[path], comments=[chatgpt]), path)


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
        for phrase, labels, files in ((gate.CODEX_OK, ('tier/T1',), ['native/x.cpp']),
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
        for phrase, labels, files in ((gate.CODEX_OK, ('tier/T1',), ['native/x.cpp']),
                                      (gate.CHATGPT_OK, ('tier/T2',), ['native/x.cpp']),
                                      (gate.CLAUDE_OK, ('tier/T2',), ['tools/merge_gate.py'])):
            line = phrase + ' abc1234'
            self.assertTrue(gate.decide(pr(*labels), files, [say(line)], 'owner')[0])
            self.assertFalse(gate.decide(pr(*labels), files, [say('    ' + line)], 'owner')[0])
            self.assertFalse(gate.decide(pr(*labels), files, [say(line, 'stranger')], 'owner')[0])
        line = gate.CHATGPT_OK + ' abc1234'
        self.assertTrue(self.ok('~~~\nexample\n~~~\n' + line))
        self.assertTrue(self.ok('- Example\n\n' + line))


class NeutralApprovalTests(unittest.TestCase):
    def check(self, p, comments=(), files=('native/x.cpp',)):
        return gate.decide(p, list(files), list(comments), 'owner')

    def test_t0_stays_ready_only_except_protected(self):
        self.assertTrue(self.check(pr('tier/T0'), files=['docs/README.md'])[0])
        self.assertFalse(self.check(pr('tier/T0'), files=['tools/merge_gate.py'])[0])
        ok, reason = self.check(pr('tier/T0'), files=['tools/merge_gate.py'], comments=[
            say('Approved for merge abc1234\nSigned: Codex (GPT-6), 2026-11-08')])
        self.assertTrue(ok)
        self.assertIn('Codex (GPT-6)', reason)

    def test_t1_t2_milestone_and_protected_accept_any_signed_owner(self):
        body = 'No High findings.\n\nApproved for merge abc1234\nSigned: ChatGPT (GPT-6), 2026-11-08'
        for p, files in ((pr('tier/T1'), ['native/x.cpp']), (pr('tier/T2'), ['native/x.cpp']),
                         (pr('milestone-review'), ['native/x.cpp']),
                         (pr('tier/T2', ref='claude/control'), ['.github/workflows/merge_gate.yml'])):
            with self.subTest(p=p, files=files):
                ok, why = self.check(p, comments=[say(body)], files=files)
                self.assertTrue(ok)
                self.assertIn('Signed: ChatGPT (GPT-6)', why)

    def test_signed_line_must_be_visible_after_approval_in_same_owner_comment(self):
        good = 'Approved for merge abc1234\n\nSigned: Codex Lead (GPT-6), 2026-11-08'
        self.assertEqual(gate.signed_approval([say(good)], 'owner', 'abc1234'), 'Codex Lead (GPT-6)')
        invalid = [
            'Approved for merge abc1234',
            'Approved for merge abc1234\nSigned:',
            'Approved for merge abc1234\nSigned: Example',
            'Signed: Codex Lead (GPT-6), 2026-11-08\nApproved for merge abc1234',
            'Approved for merge abc1234\n> Signed: Codex (GPT-6), 2026-11-08',
            'Approved for merge abc1234\n- Signed: Codex (GPT-6), 2026-11-08',
            'Approved for merge abc1234\n```\nSigned: Codex (GPT-6), 2026-11-08\n```',
            'Approved for merge abc1234\n<!-- Signed: Codex (GPT-6), 2026-11-08 -->',
            '> Approved for merge abc1234\n\nSigned: Codex (GPT-6), 2026-11-08',
            '```\nApproved for merge abc1234\n```\nSigned: Codex (GPT-6), 2026-11-08',
        ]
        for body in invalid:
            with self.subTest(body=body):
                self.assertFalse(self.check(pr('tier/T2'), comments=[say(body)])[0])
        self.assertFalse(self.check(pr('tier/T2'), comments=[
            say('Approved for merge abc1234'), say('Signed: Codex (GPT-6), 2026-11-08')])[0])

    def test_exact_head_and_owner_strict(self):
        good = 'Approved for merge abc1234\nSigned: GPT-6 reviewer, 2026-11-08'
        for body in (
                good.replace('abc1234', '0000000'),
                good.replace('Approved for merge', 'Approved for auto-merge'),
                good.replace('Approved for merge', 'Not approved for merge'),
                good.replace('abc1234', 'abc1234!'),
                '  ' + good,
                '* Approved for merge abc1234\n\nSigned: GPT-6 reviewer, 2026-11-08'):
            with self.subTest(body=body):
                self.assertFalse(self.check(pr('tier/T1'), comments=[say(body)])[0])
        self.assertFalse(self.check(pr('tier/T1'), comments=[say(good, 'stranger')])[0])
        self.assertFalse(self.check(pr('tier/T2', 'status/blocked'), comments=[say(good)])[0])
        self.assertFalse(self.check(pr('tier/T2', draft=True), comments=[say(good)])[0])
        self.assertFalse(self.check(pr('tier/T2', base='feature'), comments=[say(good)])[0])

    def test_legacy_lines_still_accepted_but_prereview_is_not_approval(self):
        for name in ('Claude', 'ChatGPT', 'Codex'):
            with self.subTest(name=name):
                body = f'{name}: approved for auto-merge abc1234'
                self.assertTrue(self.check(pr('tier/T2'), comments=[say(body)])[0])
                self.assertTrue(self.check(pr(), comments=[say(body)],
                                           files=['tools/merge_gate.py'])[0])
                self.assertFalse(self.check(pr('tier/T1'), comments=[
                    say(f'{name}: approved for auto-merge 0000000')])[0])
        self.assertFalse(self.check(pr('tier/T1'), comments=[
            say('ChatGPT: pre-review passed abc1234')])[0])


class MergeGateWorkflowTests(unittest.TestCase):
    def test_new_push_only_disables_auto_merge_without_running_pr_code(self):
        workflow = (Path(__file__).resolve().parents[3] /
                    '.github/workflows/merge_gate.yml').read_text(encoding='utf-8')
        self.assertRegex(workflow, r'(?m)^    types: \\[labeled, ready_for_review, synchronize\\])
        disable = workflow.split('      - name: Disable auto-merge on new push\n', 1)[1].split(
            '      - uses: actions/checkout@', 1)[0]
        self.assertIn("if: github.event_name == 'pull_request_target' && github.event.action == 'synchronize'",
                      disable)
        self.assertIn('gh pr merge "$PR" --repo "$GITHUB_REPOSITORY" --disable-auto', disable)
        self.assertNotIn('--auto --squash', disable)
        self.assertIn("if: github.event.action != 'synchronize'\n        with:\n          ref: main",
                      workflow)
        self.assertIn("      - name: Decide and enable auto-merge\n"
                      "        if: github.event.action != 'synchronize'", workflow)


if __name__ == '__main__':
    unittest.main()
