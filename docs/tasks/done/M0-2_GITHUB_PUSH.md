# M0-2: First push, labels, milestones and issues (Copilot, local, in `wirescape_clean/`)

**Prerequisite:** M0-1 is finished and the planner has checked its report (done 2026-10-26).
Run every step in order. **If any check fails, STOP and report. Don't improvise a fix.**

## 1. Remove the 7 relocated duplicates
The planner moved these active documents to their proper places. Delete ONLY these old copies:
```
docs/history/ABI2_SPEC.md
docs/history/SDK_AUTHOR_GUIDE.md
docs/history/FOUNDATION_SLICE_REVIEW.md
docs/history/ABI2_ACCEPTANCE_SNAPSHOT.json
docs/history/ABI2_BASELINE_MANIFEST.json
docs/history/patch_v2_draft.schema.json
docs/history/patch_v2_draft.example.json
```
Then check that all of these exist: `docs/specs/ABI2_SPEC.md`, `docs/SDK_AUTHOR_GUIDE.md`, `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`, `docs/reviews/ABI2_ACCEPTANCE_SNAPSHOT.json`, `docs/reviews/ABI2_BASELINE_MANIFEST.json`, `docs/specs/patch_v2_draft.schema.json`, `docs/specs/patch_v2_draft.example.json`, and `docs/tasks/M1-R_PLANNER_REVIEW.md`.

Run `python tools/dev.py test`. It must pass.

## 2. Pre-push safety check
- Run `git init -b main`, then `git add -A`, then `git status --porcelain`, then `git diff --cached --name-only`.
- **Stop if any staged path matches:**
  - `\.(ttf|otf|woff2?|zip|log|bundle|pyd|dll|exe|so|lib|pdb|wav)$`
  - `(^|/)(user_data|local_backups|\.tools|build|build_reports|surgepy_bin|surge|incoming_modules|deliverables|__pycache__)/`
- **Stop if the staged contents contain** `C:\Users`, `C:/Users` or `sound_design` outside `docs/history/`, `legacy/` and `docs/tasks/M0-1_*.md` / `M0-2_*.md`. Use `git grep --cached -n` to check.
- Expect about 430 files and about 2.7 MB in total. Stop if the count is above 600.

## 3. Commit and push
```bash
git commit -s -m "Wirescape native foundation: clean public start"
git remote add origin https://github.com/kalharbi11/wirescape.git
git fetch origin
```
- **If the remote has no commits:** `git push -u origin main`.
- **If it has only GitHub-generated files** (README/LICENSE/.gitignore): run `git pull origin main --allow-unrelated-histories`, keep OUR versions of conflicting files, commit, then push.
- **If it has anything else:** STOP and ask the user.
- **Never force-push.**

Then: `git tag -a v0.6.0-alpha.0 -m "Clean public start"` and `git push origin v0.6.0-alpha.0`.

## 4. Labels and milestones (GitHub CLI)
```bash
for l in status/todo status/in-progress status/review status/handoff-ready status/blocked \
         agent/codex agent/copilot agent/claude size/s size/m size/l ci/windows \
         allow-protected allow-test-removal; do gh label create "$l" --force; done
for m in "M0 Migration" "M1 ABI2 foundation" "M2 Port and patch v2" "M3 UI for the contract" \
         "M4 Communication I" "M5 Communication II" "M6 Musical core" "M7 Instruments" "M8 Portability"; do
  gh api repos/kalharbi11/wirescape/milestones -f title="$m"; done
```

## 5. Issues
Each body is: "Spec: `docs/tasks/<file>`. Follow AGENTS.md."

| Title | Labels | Milestone |
|---|---|---|
| M0-3 CI, guardrails and repo hygiene (`docs/tasks/M0-3_CI_SETUP.md`) | `status/todo agent/codex size/m` | M0 Migration |
| M1-R Planner review of the ABI 2 foundation slice (`docs/tasks/M1-R_PLANNER_REVIEW.md`) | `status/todo agent/claude size/m` | M1 ABI2 foundation |
| M1-2 Audio capture and analysis harness (`docs/tasks/M1-2_AUDIO_CAPTURE_HARNESS.md`) | `status/todo agent/codex size/m` | M1 ABI2 foundation |

Don't create an M1-1 issue: it's already done. The planner writes M1-3 after the review.

## 6. Repository settings (use `gh api` if allowed; otherwise list them for the user)
- Actions → General → fork pull request workflows: **require approval for all outside collaborators**.
- Enable Issues and Discussions.
- Branch protection on `main` comes AFTER M0-3, using the check names from its handoff.

## 7. Local switch-over (tell the user; don't run without confirmation)
1. Rename the old folder `wirescape` → `wirescape_archive_local`, and keep both `.bundle` backups.
2. `git clone https://github.com/kalharbi11/wirescape.git` into the same parent folder, named `wirescape`.
3. Copy these from the archive into the clone (all git-ignored): `.tools/` (Qt, the private font, PortAudio) and `user_data/`.
4. Run `python tools/dev.py build`, then `test`, then rerun `tools/install_windows_shortcuts.ps1` with the new package path.
5. `wirescape_clean/` can be deleted once the clone builds.

## Report
Commit hash, tag, the created labels, milestones and issues (with links), and any settings that need manual action.
