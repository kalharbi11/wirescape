# SPDX-License-Identifier: GPL-3.0-only
import unittest
from guard import violations


class GuardTests(unittest.TestCase):
    def check(self, path, status='A', added=(), labels=()):
        return violations(status, [path], added, set(labels))

    def test_protected(self):
        for path in ('AGENTS.md', '.github/workflows/ci.yml', 'LICENSES/MIT.txt',
                     'native/include/wirescape/module2.h', 'docs/specs/ABI2_PLAN.md'):
            with self.subTest(path=path):
                self.assertTrue(self.check(path))
                self.assertFalse(self.check(path, labels=['allow-protected']))

    def test_test_removal_and_rename(self):
        self.assertTrue(self.check('native/tests/example.cpp', status='D'))
        self.assertTrue(violations('R100', ['native/tests/a.cpp', 'native/a.cpp'], [], set()))
        self.assertFalse(self.check('native/tests/a.cpp', status='D', labels=['allow-test-removal']))
        self.assertFalse(self.check('native/tests/a.cpp', status='M'))

    def test_binary_private_cannot_be_overridden(self):
        for path in ('x.DLL', 'x.exe', 'x.zip', 'x.wav', 'x.ttf', 'x.otf',
                     'user_data/patch.json', '.tools/sdk', 'build/result'):
            self.assertTrue(self.check(path, labels=['allow-protected', 'allow-test-removal']))
        self.assertFalse(self.check('native/ui/assets/icon.png'))

    def test_machine_paths(self):
        for line in (r'C:\Users\person\sdk', 'D:/Users/person/sdk', '/home/person/sdk'):
            self.assertTrue(self.check('tools/a.py', added=[line]))
            self.assertFalse(self.check('docs/tasks/M0-1_LOCAL.md', added=[line]))
            self.assertFalse(self.check('legacy/a.py', added=[line], labels=['allow-protected']))
        self.assertFalse(self.check('README.md', added=['/usr/bin/python', '${HOME}/sdk']))


if __name__ == '__main__':
    unittest.main()
