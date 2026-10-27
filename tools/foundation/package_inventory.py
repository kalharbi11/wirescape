#!/usr/bin/env python3
"""Read-only inventory and integrity check for a staged Wirescape package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_package(package: Path) -> dict[str, Any]:
    root = package.resolve()
    manifest_path = root / "package-manifest.json"
    if not manifest_path.is_file():
        raise ValueError(f"missing package manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest root must be an object")
    expected = manifest.get("files")
    if not isinstance(expected, dict):
        raise ValueError("manifest must contain a 'files' object mapping relative paths to SHA-256 hashes")

    normalized: dict[str, str] = {}
    invalid_manifest_entries: list[str] = []
    for key, value in expected.items():
        rel = Path(str(key))
        if rel.is_absolute() or ".." in rel.parts or not isinstance(value, str):
            invalid_manifest_entries.append(str(key))
        else:
            normalized[rel.as_posix()] = value.lower()

    actual: dict[str, str] = {}
    for path in root.rglob("*"):
        if path.is_file() and path != manifest_path:
            rel = path.relative_to(root).as_posix()
            actual[rel] = _sha256(path)

    missing = sorted(set(normalized) - set(actual))
    unexpected = sorted(set(actual) - set(normalized))
    mismatched = sorted(key for key in normalized.keys() & actual.keys() if normalized[key] != actual[key])
    files = sorted(actual)
    qt_files = [p for p in files if Path(p).name.startswith("Qt6") and Path(p).suffix.lower() == ".dll"]
    processor_files = [p for p in files if Path(p).name.lower().startswith("ws_") and Path(p).suffix.lower() == ".dll"]
    portaudio_files = [p for p in files if "portaudio" in Path(p).name.lower()]
    notice_files = [p for p in files if any(word in Path(p).name.lower() for word in ("license", "licence", "notice", "copyright"))]

    build = manifest.get("build", {})
    if not isinstance(build, dict):
        raise ValueError("manifest build metadata must be an object")
    tools = build.get("tools", {})
    if not isinstance(tools, dict):
        raise ValueError("manifest build tools must be an object")
    qt_version = None
    prefix = str(tools.get("qt_prefix", build.get("qt_prefix", "")))
    # Infer only a version explicitly present in the configured Qt directory name.
    match = re.search(r"(?:^|[\\/])([0-9]+\.[0-9]+\.[0-9]+)(?:[\\/]|$)", prefix)
    if match:
        qt_version = match.group(1)

    sbom = Path(prefix) / "sbom" if prefix else None
    sbom_files = sorted(p.name for p in sbom.iterdir() if p.is_file()) if sbom and sbom.is_dir() else []

    return {
        "package": str(root),
        "classification": "experimental local development package; not a public redistribution release",
        "manifest": str(manifest_path),
        "integrity": {
            "expected_file_count": len(normalized),
            "actual_file_count": len(actual),
            "missing": missing,
            "unexpected": unexpected,
            "hash_mismatches": mismatched,
            "invalid_manifest_entries": sorted(invalid_manifest_entries),
            "ok": not (missing or unexpected or mismatched or invalid_manifest_entries),
        },
        "runtime_inventory": {
            "qt": {"version": qt_version or "unknown", "evidence": "configured Qt prefix in package build metadata" if qt_version else "unknown", "packaged_qt6_dll_count": len(qt_files), "examples": qt_files[:8]},
            "msvc": {"version": "unknown", "evidence": f"CMake generator: {tools['generator']} (toolset version not inferred)" if tools.get("generator") else "unknown", "architecture": tools.get("architecture", "unknown")},
            "portaudio": {"version": "unknown", "evidence": "packaged PortAudio-named runtime file(s); package manifest does not record a version", "files": portaudio_files},
            "stock_processors": {"version": "Wirescape package build", "evidence": "packaged ws_*.dll filenames", "files": processor_files},
        },
        "notices": {"bundled_files": notice_files, "source_evidence": ["repository LICENSE (GNU GPL v3 text)", "native/ui/third_party/PORTAUDIO_LICENSE.txt (PortAudio license text)"], "qt_sbom_evidence": {"sdk_prefix": prefix or None, "local_documents": sbom_files, "bundled_documents": [p for p in files if p.lower().endswith((".spdx", ".spdx.json", ".cdx.json"))], "note": "Presence of metadata alone does not establish full distribution compliance"}},
        "gaps": [
            "No clean-machine Windows installation or startup result is established by this inventory.",
            "No Windows Application Control bypass or executable launch is attempted by this tool.",
            "MSVC runtime/toolset patch version, PortAudio version and per-binary Qt component versions are not recorded in the package manifest.",
            "Review all dependency notices and source obligations before any redistribution; this inventory is not a legal compliance determination.",
            "ARM64 headless tool availability and Raspberry Pi execution remain unverified.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path, help="staged package directory")
    parser.add_argument("--output", type=Path, help="write JSON report here (default: stdout)")
    args = parser.parse_args(argv)
    try:
        report = inspect_package(args.package)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"package inventory: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        sys.stdout.write(rendered)
    return 0 if report["integrity"]["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
