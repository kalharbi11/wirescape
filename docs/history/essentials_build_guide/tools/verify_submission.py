#!/usr/bin/env python3
"""Structural offline submission check; this is not a security certification."""
import argparse
import json
import re
import sys
from pathlib import Path

REQUIRED = ("module.cpp", "module.json", "CMakeLists.txt", "tests.cpp")
FORBIDDEN = {".dll", ".pyd", ".exe", ".ttf", ".zip", ".a", ".lib", ".so", ".dylib", ".o", ".obj"}
ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")


def verify(path: Path):
    problems = []
    if not path.exists() or not path.is_dir():
        return ["submission path must be an existing directory"]
    root = path.resolve()
    for item in path.rglob("*"):
        try:
            resolved = item.resolve(strict=True)
        except (OSError, RuntimeError):
            problems.append(f"broken or looping symlink: {item.relative_to(path)}")
            continue
        if item.is_symlink():
            problems.append(f"symlinks are not accepted: {item.relative_to(path)}")
        try:
            resolved.relative_to(root)
        except ValueError:
            problems.append(f"path escapes submission directory: {item.relative_to(path)}")
        if item.is_file() and item.suffix.lower() in FORBIDDEN:
            problems.append(f"prebuilt or bundled file is not accepted: {item.relative_to(path)}")
        if item.is_dir() and item.name.lower() in {"build", "dist", "__pycache__", ".git"}:
            problems.append(f"generated or repository directory is not accepted: {item.relative_to(path)}")
    for name in REQUIRED:
        candidate = path / name
        if not candidate.is_file() or candidate.is_symlink():
            problems.append(f"required regular file is missing: {name}")
    manifest_path = path / "module.json"
    if manifest_path.is_file() and not manifest_path.is_symlink():
        try:
            data = json.loads(manifest_path.read_text(encoding="utf-8"))
            if not isinstance(data, dict):
                raise ValueError("top level must be an object")
            if data.get("schema_version") != 1 or data.get("abi_version") != 1:
                problems.append("schema_version and abi_version must both be 1")
            if not isinstance(data.get("id"), str) or not ID_RE.fullmatch(data["id"]):
                problems.append("id must use lowercase dotted namespace form, for example team.effect")
            if not isinstance(data.get("name"), str) or not data["name"].strip():
                problems.append("name must be a non-empty string")
            if not isinstance(data.get("library"), str) or not re.fullmatch(r"ws_[a-z0-9_]+", data["library"]):
                problems.append("library must be a ws_ prefixed target name")
            for field in ("inputs", "outputs"):
                ports = data.get(field)
                if not isinstance(ports, dict) or any(not isinstance(k, str) or not isinstance(v, str) or v not in {"audio", "control"} for k, v in ports.items()):
                    problems.append(f"{field} must map port names to audio or control")
            params = data.get("parameters")
            if not isinstance(params, dict) or any(not isinstance(k, str) or type(v) is not int or v < 0 for k, v in params.items()):
                problems.append("parameters must map names to non-negative integer IDs")
            elif len(set(params.values())) != len(params):
                problems.append("parameter IDs must be unique")
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
            problems.append(f"module.json is invalid: {exc}")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("submission", type=Path)
    args = parser.parse_args()
    errors = verify(args.submission)
    if errors:
        for issue in errors:
            print(f"FAIL: {issue}")
        return 1
    print("PASS: required files and basic manifest structure are present")
    print("This offline structural check is not a security certification or proof of DSP correctness.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
