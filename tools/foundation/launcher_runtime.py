# SPDX-License-Identifier: GPL-3.0-only
from __future__ import annotations

import re
import hashlib
import json
import sys
from pathlib import Path
from typing import Iterable


def _valid(path: Path, kind: str) -> bool:
    if kind == "qt":
        required = (
            "lib/cmake/Qt6/Qt6Config.cmake",
            "bin/Qt6Core.dll",
            "bin/Qt6Gui.dll",
            "bin/Qt6Qml.dll",
            "bin/Qt6Quick.dll",
            "plugins/platforms/qwindows.dll",
        )
        return all((path / item).is_file() for item in required)
    if kind == "portaudio":
        return path.is_file() and path.name.lower() == "libportaudio64bit.dll"
    raise ValueError(f"unknown dependency kind: {kind}")


def _resolve(value: str | Path) -> Path:
    try:
        return Path(value).expanduser().resolve()
    except (OSError, RuntimeError, ValueError) as exc:
        raise ValueError(f"invalid dependency path: {value}") from exc


def choose_path(
    kind: str,
    explicit: str | Path | None = None,
    remembered: str | Path | None = None,
    candidates: Iterable[tuple[str, str | Path]] = (),
) -> dict[str, str]:
    if kind not in ("qt", "portaudio"):
        raise ValueError(f"unknown dependency kind: {kind}")
    for source, value in (("explicit", explicit), ("remembered", remembered)):
        if value is not None:
            path = _resolve(value)
            if not _valid(path, kind):
                raise ValueError(f"invalid {kind} path from {source}: {path}")
            return {"path": str(path), "source": source}
    for source, value in candidates:
        if value is None:
            continue
        try:
            path = _resolve(value)
        except ValueError:
            continue
        if _valid(path, kind):
            return {"path": str(path), "source": str(source)}
    raise ValueError(f"no valid {kind} path among supplied candidates")


def shortcut_target(checkout: str | Path, package: str | Path) -> str:
    root = _resolve(checkout)
    allowed = (root / "build" / "packages").resolve()
    package_root = _resolve(package)
    try:
        package_root.relative_to(allowed)
    except ValueError as exc:
        raise ValueError("package is outside active checkout build/packages") from exc
    executable = package_root / "wirescape_studio.exe"
    if not executable.resolve().is_relative_to(package_root):
        raise ValueError("package executable escapes package directory")
    if not executable.is_file():
        raise ValueError("package does not contain wirescape_studio.exe")
    manifest = package_root / "package-manifest.json"
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
        build = data["build"]
        expected = data["files"]["wirescape_studio.exe"]
        source_root = _resolve(build["source_root"])
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise ValueError("package manifest lacks required provenance") from exc
    if source_root != root:
        raise ValueError("package manifest is from a different checkout")
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    if not isinstance(expected, str) or digest != expected:
        raise ValueError("studio executable does not match package manifest")
    return str(executable)


_SHA = re.compile(r"^[0-9a-fA-F]{40}$")


def compare_revisions(installed: str, latest: str) -> str:
    if not isinstance(installed, str) or not _SHA.fullmatch(installed):
        raise ValueError("installed revision must be a full 40-character SHA")
    if not isinstance(latest, str) or not _SHA.fullmatch(latest):
        raise ValueError("latest revision must be a full 40-character SHA")
    return "current" if installed.lower() == latest.lower() else "different"


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Validate a Wirescape shortcut package")
    parser.add_argument("--checkout", required=True)
    parser.add_argument("--package", required=True)
    args = parser.parse_args()
    try:
        print(shortcut_target(args.checkout, args.package))
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
