<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-k: docs freshness check (#93)

**Do:** `tools/docs_fresh.py`: parse the "Now" section of `docs/PLAN.md`; for every `#N` it mentions, query the REST API and flag: PR merged/closed but listed as open/waiting; issue closed but listed as next; the "main =" SHA older than 3 days. Exit 1 with a short list. Also flag "Now" over 30 lines. Run it in the Pi nightly (W1) and as a non-required CI step on PRs that touch `docs/PLAN.md`. Offline mode with a JSON fixture for tests.

```acceptance
tier: T0
budget: 1 ChatGPT session (or Codex if CI iteration is needed)
size: <= 200 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_docs_fresh*.py"
  - python tools/docs_fresh.py --offline tools/foundation/tests/fixtures/now_state.json
  - ci: linux-arm64, guard green
stops: [new dependency]
```
