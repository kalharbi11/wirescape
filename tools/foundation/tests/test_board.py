# SPDX-License-Identifier: GPL-3.0-only
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import board

NOW = datetime(2026, 11, 12, 12, 0, tzinfo=timezone.utc)
FRESH = '2026-11-12T10:00:00Z'
OLD = '2026-11-10T10:00:00Z'


def issue(number, *labels, updated=FRESH, title='Item'):
    return {'number': number, 'title': title, 'updated_at': updated, 'labels': [{'name': n} for n in labels]}


def pr(number, body='', ref='feature', updated=FRESH, draft=False):
    return {'number': number, 'body': body, 'head': {'ref': ref}, 'updated_at': updated, 'draft': draft}


def rows(text, heading):
    part = text.split(f'### {heading}', 1)[1].split('###', 1)[0]
    return [line for line in part.splitlines() if line.startswith('| #')]


class BoardTests(unittest.TestCase):
    def test_each_status_lands_in_one_section(self):
        text = board.render([
            issue(1, 'status/todo'),
            issue(2, 'status/todo', 'for/builder-chat-1'),
            issue(3, 'status/in-progress', 'agent/builder-chat-2'),
            issue(4, 'status/review'),
            issue(5, 'status/blocked', 'agent/builder-chat-3'),
            issue(6, 'status/handoff-ready'),
            issue(7),  # tracking issue without a status: not on the board
        ], [], NOW)
        self.assertEqual(len(rows(text, 'Open')), 1)
        self.assertIn('builder-chat-1', rows(text, 'Reserved')[0])
        self.assertIn('builder-chat-2', rows(text, 'In progress')[0])
        self.assertEqual(len(rows(text, 'Waiting')), 1)
        self.assertEqual(len(rows(text, 'Blocked')), 1)
        self.assertEqual(len(rows(text, 'Handed off')), 1)
        self.assertNotIn('| #7 ', text)

    def test_board_issue_itself_is_skipped(self):
        self.assertNotIn('| #473 ', board.render([issue(473, 'status/todo')], [], NOW))

    def test_idle_claim_is_stale_and_unnamed_claim_is_flagged(self):
        text = board.render([issue(3, 'status/in-progress', updated=OLD)], [], NOW)
        line = rows(text, 'In progress')[0]
        self.assertIn('unnamed claim', line)
        self.assertIn('**STALE**', line)

    def test_idle_reservation_opens_to_all(self):
        text = board.render([issue(2, 'status/todo', 'for/builder-claude-1', updated=OLD)], [], NOW)
        self.assertIn('**open to all**', rows(text, 'Reserved')[0])

    def test_lead_or_owner_reservation_never_opens(self):
        text = board.render([issue(2, 'status/todo', 'for/owner', updated=OLD)], [], NOW)
        self.assertNotIn('open to all', rows(text, 'Reserved')[0])

    def test_recent_pr_push_keeps_claim_fresh(self):
        text = board.render([issue(3, 'status/in-progress', updated=OLD)],
                            [pr(30, 'Closes #3', updated=FRESH, draft=True)], NOW)
        line = rows(text, 'In progress')[0]
        self.assertIn('#30 draft', line)
        self.assertNotIn('STALE', line)

    def test_pr_links_by_branch_name(self):
        text = board.render([issue(9, 'status/todo')], [pr(90, ref='task/9-thing')], NOW)
        self.assertIn('#90', rows(text, 'Open')[0])

    def test_splice_replaces_only_between_markers(self):
        body = f'notes above\n{board.START}\nold\n{board.END}\nnotes below'
        out = board.splice(body, f'{board.START}\nnew\n{board.END}')
        self.assertEqual(out, f'notes above\n{board.START}\nnew\n{board.END}\nnotes below')

    def test_splice_prepends_when_markers_missing(self):
        out = board.splice('lead notes', f'{board.START}\nx\n{board.END}')
        self.assertTrue(out.startswith(board.START))
        self.assertTrue(out.endswith('lead notes'))


if __name__ == '__main__':
    unittest.main()
