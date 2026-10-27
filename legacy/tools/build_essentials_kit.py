"""Package the audited, source-only external Essentials kit without private data."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / "essentials_build_guide"
SUFFIXES = {".cpp", ".h", ".md", ".json", ".txt", ".py"}
EXCLUDE = {"__pycache__", "build", ".git", ".pytest_cache"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "deliverables" / "Wirescape_Essentials_Agent_Kit_2026-10-26.zip")
    args = parser.parse_args()
    mirrors = {
        "include/wirescape/module.h": "native/include/wirescape/module.h",
        "include/wirescape/engine.h": "native/include/wirescape/engine.h",
        "include/wirescape/linear_ramp.h": "native/include/wirescape/linear_ramp.h",
        "host/engine.cpp": "native/src/engine.cpp",
        "LICENSE": "LICENSE",
    }
    for module in ("scale_offset", "lfo"):
        for name in ("module.cpp", "module.json", "CMakeLists.txt"):
            mirrors[f"reference/{module}/{name}"] = f"native/modules/{module}/{name}"
    sources = {}
    for local, source in mirrors.items():
        data = (ROOT / source).read_bytes()
        if (KIT / local).read_bytes() != data:
            raise ValueError(f"Kit reference drift: {local}; refresh from {source} before packaging")
        sources[local] = {"source": source, "sha256": digest(data)}
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    provenance = {"snapshot_commit": revision, "working_tree_dirty": bool(dirty), "experimental_abi": 1, "sources": sources,
                  "scope": "source snapshot, not a stable SDK or hardware verification"}
    (KIT / "SOURCE_PROVENANCE.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    payload = {}
    for path in sorted(KIT.rglob("*")):
        relative = path.relative_to(KIT)
        if any(part in EXCLUDE for part in relative.parts):
            continue
        if path.is_symlink():
            raise ValueError(f"Symlink in kit: {relative}")
        if not path.is_file() or path.name == "KIT_MANIFEST.json":
            continue
        if path.suffix.lower() not in SUFFIXES and path.name != "LICENSE":
            raise ValueError(f"Unexpected file in source-only kit: {relative}")
        payload[relative.as_posix()] = path.read_bytes()
    required = {"README.md", "AGENT_START_HERE.md", "CONTRACT.md", "CMakeLists.txt", "RETURN_AND_IMPORT.md",
                "MODULE_STATUS_AND_OWNERSHIP.md", "submissions/example_gain/tests.cpp", "tools/run_checks.py"}
    if not required.issubset(payload):
        raise ValueError(f"Incomplete kit: {required - payload.keys()}")
    manifest = {"files": {name: digest(data) for name, data in payload.items()},
                "purpose": "current standalone Wirescape Essentials source authoring"}
    manifest_data = (json.dumps(manifest, indent=2) + "\n").encode()
    (KIT / "KIT_MANIFEST.json").write_bytes(manifest_data)
    payload["KIT_MANIFEST.json"] = manifest_data
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in payload.items():
            archive.writestr(f"Wirescape_Essentials_Agent_Kit/{name}", data)
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
        for name, data in payload.items():
            assert archive.read(f"Wirescape_Essentials_Agent_Kit/{name}") == data
    args.output.with_suffix(".zip.sha256").write_text(digest(args.output.read_bytes()) + "\n", encoding="ascii")
    print(f"PASS: {len(payload)} source/document files; ZIP CRC and contents verified\n{args.output}")


if __name__ == "__main__":
    main()
