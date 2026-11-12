# SPDX-License-Identifier: GPL-3.0-only
import io
import sys
import unittest
import urllib.error
from unittest import mock
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import board

NOW = datetime(2026, 11, 12, 12, 0, tzinfo=timezone.utc)
FRESH = '2026-11-12T10:00:00Z'
OLD = '2026-11-10T10:00:00Z'


def issue(number, *labels, updated=FRESH, title='Item', body=''):
    return {'number': number, 'title': title, 'updated_at': updated, 'body': body,
            'labels': [{'name': n} for n in labels]}


def pr(number, body='', ref='feature', updated=FRESH, draft=False):
    return {'number': number, 'body': body, 'head': {'ref': ref}, 'updated_at': updated, 'draft': draft}


def rows(text, heading):
    part = text.split(f'### {heading}', 1)[1].split('###', 1)[0]
    return [line for line in part.splitlines() if line.startswith('| #')]


class BoardTests(unittest.TestCase):
    def test_each_status_lands_in_one_section(self):
        text = board.render([
            issue(1, 'status/todo'),
            issue(2, 'status/todo', 'agent/gpt-builder1'),
            issue(3, 'status/in-progress', 'agent/gpt-builder2'),
            issue(4, 'status/review', 'agent/claude-builder1'),
            issue(5, 'status/blocked'),
            issue(6, 'status/handoff-ready'),
            issue(7, 'status/handoff-ready', 'agent/gpt-builder1'),
            issue(8),  # tracking issue without a status: not on the board
        ], [], NOW)
        self.assertEqual(len(rows(text, 'Unassigned')), 2)
        self.assertEqual(len(rows(text, 'Assigned')), 2)
        self.assertIn('gpt-builder2', rows(text, 'In progress')[0])
        self.assertEqual(len(rows(text, 'Waiting')), 1)
        self.assertEqual(len(rows(text, 'Blocked')), 1)
        self.assertNotIn('| #8 ', text)

    def test_who_has_what_lists_every_item_per_agent(self):
        text = board.render([issue(2, 'status/todo', 'agent/gpt-builder1'),
                             issue(9, 'status/review', 'agent/gpt-builder1'),
                             issue(4, 'status/in-progress', 'agent/keel')], [], NOW)
        self.assertIn('`gpt-builder1` #2 #9', text)
        self.assertIn('`keel` #4', text)

    def test_claim_without_a_name_is_flagged(self):
        self.assertIn('| no name |', rows(board.render([issue(3, 'status/in-progress')], [], NOW), 'In progress')[0])

    def test_board_issue_is_skipped_and_its_usage_label_shows_a_banner(self):
        text = board.render([issue(473, 'status/todo', 'usage/low-claude')], [], NOW)
        self.assertNotIn('| #473 ', text)
        self.assertIn('Low-usage mode (claude)', text)
        self.assertNotIn('Low-usage mode', board.render([issue(473)], [], NOW))

    def test_idle_counts_from_latest_issue_or_pr_activity(self):
        text = board.render([issue(3, 'status/in-progress', 'agent/x', updated=OLD)],
                            [pr(30, 'Closes #3', updated=FRESH, draft=True)], NOW)
        line = rows(text, 'In progress')[0]
        self.assertIn('#30 draft', line)
        self.assertIn('| 2h |', line)

    def test_pr_links_by_branch_name(self):
        self.assertIn('#90', rows(board.render([issue(9, 'status/todo')], [pr(90, ref='task/9-thing')], NOW),
                                  'Unassigned')[0])

    def test_next_step_comes_from_current_state_block(self):
        body = 'Goal\nNext: not this\n\n## Current state\nHead: abc1234\n**Next:** fix Windows red | rerun\n## Notes\nNext: nor this'
        line = rows(board.render([issue(3, 'status/in-progress', body=body)], [], NOW), 'In progress')[0]
        self.assertIn('| fix Windows red / rerun |', line)
        self.assertEqual(board.next_step('no block here\nNext: x'), '')

    def test_splice_replaces_only_between_markers(self):
        body = f'notes above\n{board.START}\nold\n{board.END}\nnotes below'
        out = board.splice(body, f'{board.START}\nnew\n{board.END}')
        self.assertEqual(out, f'notes above\n{board.START}\nnew\n{board.END}\nnotes below')

    def test_splice_prepends_when_markers_missing(self):
        out = board.splice('lead notes', f'{board.START}\nx\n{board.END}')
        self.assertTrue(out.startswith(board.START))
        self.assertTrue(out.endswith('lead notes'))


    def test_pending_closes_takes_recent_main_merges_only(self):
        def merged(number, body, at='2026-11-12T09:00:00Z', base='main'):
            return {'number': number, 'body': body, 'merged_at': at, 'base': {'ref': base},
                    'merge_commit_sha': 'abcdef0123'}
        out = board.pending_closes([
            merged(10, 'Closes #664\nRefs #662'),
            merged(11, 'Fixes #5, resolves #6'),
            merged(12, 'Closes #7', at='2026-11-01T09:00:00Z'),  # older than the window
            merged(13, 'Closes #8', base='release'),
            {'number': 14, 'body': 'Closes #9', 'merged_at': None, 'base': {'ref': 'main'}},  # closed, not merged
        ], NOW)
        self.assertEqual(out, [(664, 10, 'abcdef0123'), (5, 11, 'abcdef0123'), (6, 11, 'abcdef0123')])

    def test_should_close_leaves_prs_closed_and_reopened_issues(self):
        note = {'body': 'Closed by #10\n' + board.MARKER.format(pr=10)}
        self.assertTrue(board.should_close({'state': 'open'}, [], 10))
        self.assertFalse(board.should_close({'state': 'closed'}, [], 10))
        self.assertFalse(board.should_close({'state': 'open', 'pull_request': {}}, [], 10))
        self.assertFalse(board.should_close({'state': 'open'}, [note], 10))  # reopened after our close
        self.assertTrue(board.should_close({'state': 'open'}, [note], 11))

    def test_stale_status_lists_status_labels_on_closed_items_only(self):
        items = [
            dict(issue(1, 'status/in-progress', 'agent/keel', 'tier/T1'), state='closed'),
            dict(issue(2, 'status/review'), state='open'),  # live work keeps its label
            dict(issue(3, 'status/ready-to-merge', 'status/blocked'), state='closed', pull_request={}),
            dict(issue(4, 'tier/T0'), state='closed'),
        ]
        self.assertEqual(board.stale_status(items),
                         [(1, 'status/in-progress'), (3, 'status/ready-to-merge'), (3, 'status/blocked')])

    def test_closes_quoted_in_code_is_not_a_directive(self):
        body = 'Use `Closes #5` in the PR.\n```\nCloses #6\n```\nCloses #7'
        pr = {'number': 20, 'body': body, 'merged_at': '2026-11-12T09:00:00Z', 'base': {'ref': 'main'},
              'merge_commit_sha': 'abc1234'}
        self.assertEqual(board.pending_closes([pr], NOW), [(7, 20, 'abc1234')])

    def _fake_api(self, calls, label_error=None):
        closed_pr = {'number': 30, 'state': 'closed', 'pull_request': {}, 'labels': [{'name': 'status/review'}]}

        def api(path, token, method='GET', data=None):
            calls.append((method, path))
            if path == 'commits/main':
                return {'sha': 'f' * 40}
            if path.startswith('actions/workflows/main_verify.yml/runs'):
                return {'total_count': 0}
            if path.startswith('pulls?'):
                return []
            if path.startswith('issues?state=closed'):
                return [closed_pr]
            if method == 'DELETE' and label_error:
                raise urllib.error.HTTPError(path, label_error, 'denied', {}, io.BytesIO())
            return {}
        return api

    def test_followups_verify_main_first_and_label_403_stays_visible(self):
        calls = []
        with mock.patch.object(board, '_api', self._fake_api(calls, label_error=403)), \
                mock.patch.object(sys, 'stderr', io.StringIO()) as err:
            self.assertEqual(board.followups('t', NOW), 1)
        methods = [c for c in calls if c[0] != 'GET']
        self.assertEqual(methods[0], ('POST', 'actions/workflows/main_verify.yml/dispatches'))
        self.assertIn(('DELETE', 'issues/30/labels/status%2Freview'), methods)
        self.assertIn('_clear_closed_status failed', err.getvalue())

    def test_followups_label_404_is_idempotent(self):
        calls = []
        with mock.patch.object(board, '_api', self._fake_api(calls, label_error=404)):
            self.assertEqual(board.followups('t', NOW), 0)

    def _watch_api(self, polls, calls):
        """Open PR #40 has auto-merge armed for `polls` reads, then GitHub merges it."""
        seen = {'n': 0}

        def api(path, token, method='GET', data=None):
            calls.append((method, path))
            if path.startswith('pulls?state=open'):
                seen['n'] += 1
                return [{'number': 40, 'auto_merge': {'merge_method': 'squash'}}] if seen['n'] <= polls else []
            if path == 'pulls/40':
                return {'merged': False}
            if path == 'commits/main':
                return {'sha': 'a' * 40 if seen['n'] <= polls else 'b' * 40}
            if path.startswith('actions/workflows/main_verify.yml/runs'):
                return {'total_count': 0}
            return {}
        return api

    def test_watch_verifies_main_after_a_merge_that_lands_after_the_board_ran(self):
        # #702 High: the approval comment's Board run saw the old main; the gate's auto-merge lands
        # afterwards and starts no workflow. The watcher (started by the gate run) must catch it.
        calls, sleeps = [], []
        with mock.patch.object(board, '_api', self._watch_api(2, calls)), \
                mock.patch.object(board, '_all', lambda path, token: board._api(path, token)):
            self.assertEqual(board.watch_merges('t', 12, sleep=sleeps.append, clock=lambda: 0), 0)
        self.assertEqual(len(sleeps), 2)
        self.assertEqual(calls[-1], ('POST', 'actions/workflows/main_verify.yml/dispatches'))
        reads = [i for i, c in enumerate(calls) if c[1] == 'commits/main']
        self.assertEqual(len(reads), 2)  # once on start, once after the merge (against the new main)
        self.assertIn(('GET', 'pulls/40'), calls[:reads[1]])

    def test_watch_without_armed_prs_verifies_main_once(self):
        calls, sleeps = [], []
        with mock.patch.object(board, '_api', self._watch_api(0, calls)), \
                mock.patch.object(board, '_all', lambda path, token: board._api(path, token)):
            self.assertEqual(board.watch_merges('t', 12, sleep=sleeps.append, clock=lambda: 0), 0)
        self.assertEqual(sleeps, [])
        self.assertIn(('POST', 'actions/workflows/main_verify.yml/dispatches'), calls)

    def test_watch_is_bounded_when_auto_merge_never_lands(self):
        calls, sleeps, now = [], [], [0.0]
        def clock():
            return now[0]
        def sleep(s):
            sleeps.append(s)
            now[0] += s
        with mock.patch.object(board, '_api', self._watch_api(10 ** 6, calls)), \
                mock.patch.object(board, '_all', lambda path, token: board._api(path, token)):
            self.assertEqual(board.watch_merges('t', 1, sleep=sleep, clock=clock), 0)
        self.assertLessEqual(len(sleeps), 4)
        self.assertIn(('POST', 'actions/workflows/main_verify.yml/dispatches'), calls)

    def test_watch_sees_a_merge_while_another_pr_stays_armed(self):
        # gpt-builder1 pre-review: #40 merges and leaves the open list while #41 stays armed.
        calls, sleeps, state = [], [], {'polls': 0}
        def api(path, token, method='GET', body=None):
            calls.append((method, path))
            if path.startswith('pulls?state=open'):
                state['polls'] += 1
                prs = [{'number': 41, 'auto_merge': {'merge_method': 'squash'}}]
                return ([{'number': 40, 'auto_merge': {'merge_method': 'squash'}}] if state['polls'] == 1 else []) + prs
            if path == 'pulls/40':
                return {'merged': True}
            if path == 'commits/main':
                return {'sha': ('a' if state['polls'] == 1 else 'b') * 40}
            if path.startswith('actions/workflows/main_verify.yml/runs'):
                return {'total_count': 1 if path.endswith('a' * 40 + '&per_page=1') else 0}
            return {}
        now = [0.0]
        def sleep(s):
            sleeps.append(s)
            now[0] += s
        with mock.patch.object(board, '_api', api), \
                mock.patch.object(board, '_all', lambda path, token: board._api(path, token)):
            self.assertEqual(board.watch_merges('t', 1, sleep=sleep, clock=lambda: now[0]), 0)
        dispatch = calls.index(('POST', 'actions/workflows/main_verify.yml/dispatches'))
        self.assertEqual(sum(1 for c in calls[:dispatch] if c[1].startswith('pulls?state=open')), 2)
        self.assertNotIn(('GET', 'pulls/41'), calls)

    def test_post_merge_watch_runs_after_ci_guard_and_gate(self):
        workflow = (Path(__file__).resolve().parents[3] / '.github/workflows/post_merge.yml').read_text(encoding='utf-8')
        self.assertIn('workflows: [CI, Guard, Merge gate]', workflow)
        self.assertIn("conclusion != 'skipped'", workflow)
        self.assertIn('--watch-merges', workflow)

if __name__ == '__main__':
    unittest.main()
