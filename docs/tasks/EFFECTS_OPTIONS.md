<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Separate effects module: candidate research (#168)

While higher-priority work waits for reviews, compare three existing effects in
`docs/reviews/EFFECTS_OPTIONS.md`. Record exact revisions, relevant file licences,
GPL-3.0 compatibility, host/RT concerns, Pi evidence limits and an integration
sketch. Recommend a bounded follow-up; implementation is a separate issue.
No dependency adoption, downloaded code in the tree, upstream changes, tape stage
or hardware access. Read #168 and inspect only the named candidate licence/DSP
headers at the recorded immutable revisions.

```acceptance
tier: T0
budget: 1 bounded research pass
size: <= 90 changed lines, <= 2 files
must_pass:
  - python -m unittest tools/foundation/tests/test_doc_links.py
  - python -c "from pathlib import Path; p=Path('docs/reviews/EFFECTS_OPTIONS.md'); s=p.read_text(); assert all(pin in s for pin in ('f4cfa7697095a28d292bff758beb51deb49b5a86', '2c72eaf9eac5fc0dca1919d65d606da907832618', '987b39dcb2e2e61e6225c3bc390fd00da824d226')); assert 'MIT' in s and 'GPL-3.0-or-later' in s"
stops: [new dependency or source integration, licence incompatibility, implementation scope]
```
