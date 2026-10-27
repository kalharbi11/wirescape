# SPDX-License-Identifier: GPL-3.0-only
import re
import subprocess
from pathlib import Path, PurePosixPath
import unittest

ROOT = Path(__file__).resolve().parents[3]

_INLINE_LINK = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
_BACKTICK_DOC = re.compile(r"`(docs/[^`]+\.md)`")
_EXTERNAL_PREFIXES = ("http://", "https://", "mailto:", "#")

# Protected files are not edited by this task; keep known unresolved links here when needed.
KNOWN_BROKEN: set[tuple[str, str]] = {
}


def _is_excluded_markdown(path: str) -> bool:
    normalized = PurePosixPath(path)
    if normalized.parts[:2] == ("docs", "history"):
        return True
    if normalized.parts and normalized.parts[0] == "legacy":
        return True
    return bool(normalized.parts and normalized.parts[0].startswith("build"))


def _tracked_markdown_files() -> list[Path]:
    output = subprocess.check_output(["git", "ls-files", "*.md"], cwd=ROOT, text=True)
    files = []
    for rel_path in output.splitlines():
        if rel_path and not _is_excluded_markdown(rel_path):
            files.append(ROOT / rel_path)
    return files


def _iter_inline_markdown_links(text: str):
    in_code_fence = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.lstrip().startswith("```"):
            in_code_fence = not in_code_fence
            continue
        if in_code_fence:
            continue
        for match in _INLINE_LINK.finditer(line):
            yield line_number, match.group(1).strip()


def _resolve_target(markdown_file: Path, target: str) -> Path | None:
    if target.startswith(_EXTERNAL_PREFIXES):
        return None
    path_only = target.split("#", 1)[0]
    if not path_only:
        return None
    if path_only.startswith("/"):
        return ROOT / path_only.lstrip("/")
    return markdown_file.parent / path_only


def _collect_broken_links() -> list[tuple[str, int, str]]:
    broken = []
    for markdown_file in _tracked_markdown_files():
        text = markdown_file.read_text(encoding="utf-8", errors="ignore")
        rel_file = markdown_file.relative_to(ROOT).as_posix()
        for line_number, target in _iter_inline_markdown_links(text):
            resolved = _resolve_target(markdown_file, target)
            if resolved is not None and not resolved.exists():
                broken.append((rel_file, line_number, target))

    for rel_file in ("docs/PLAN.md", "AGENTS.md"):
        markdown_file = ROOT / rel_file
        text = markdown_file.read_text(encoding="utf-8", errors="ignore")
        for line_number, line in enumerate(text.splitlines(), start=1):
            for target in _BACKTICK_DOC.findall(line):
                if "<" in target or "*" in target:
                    continue
                if not (ROOT / target).exists():
                    broken.append((rel_file, line_number, target))
    return broken


class DocLinkTests(unittest.TestCase):
    def test_markdown_links_resolve(self):
        broken = _collect_broken_links()
        broken_pairs = {(rel_file, target) for rel_file, _, target in broken}
        stale_known = sorted(known for known in KNOWN_BROKEN if known not in broken_pairs)
        unknown_broken = [item for item in broken if (item[0], item[2]) not in KNOWN_BROKEN]

        errors = [f"{rel_file}:{line} -> {target}" for rel_file, line, target in unknown_broken]
        errors.extend(
            f"KNOWN_BROKEN resolved: {rel_file} -> {target}" for rel_file, target in stale_known
        )
        self.assertEqual(errors, [], "\n".join(errors))

    def test_inline_parser_samples(self):
        sample = "\n".join([
            "[root](/docs/PLAN.md)",
            "[anchor](docs/PLAN.md#current-milestone)",
            "[external](https://example.com/x)",
            "```md",
            "[ignored](docs/DOES_NOT_EXIST.md)",
            "```",
            "[broken](docs/DOES_NOT_EXIST.md)",
        ])

        links = list(_iter_inline_markdown_links(sample))
        self.assertEqual(
            links,
            [
                (1, "/docs/PLAN.md"),
                (2, "docs/PLAN.md#current-milestone"),
                (3, "https://example.com/x"),
                (7, "docs/DOES_NOT_EXIST.md"),
            ],
        )

        markdown_file = ROOT / "README.md"
        resolved = [
            target
            for target in (_resolve_target(markdown_file, link_target) for _, link_target in links)
            if target is not None and not target.exists()
        ]
        self.assertEqual(resolved, [ROOT / "docs/DOES_NOT_EXIST.md"])


if __name__ == "__main__":
    unittest.main()
