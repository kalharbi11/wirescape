# SPDX-License-Identifier: GPL-3.0-only
"""Assert same-build offline hashes; record cross-build hashes without comparing them."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def validate(report):
    if any(report.get(k) != v for k, v in
           {"sample_rate": 48000, "frames": 48000, "block": 128, "seed": 42}.items()):
        raise ValueError("incorrect render settings")
    cases = report.get("cases", [])
    names = set()
    for case in cases:
        name, first, second = case["name"], case["first"], case["second"]
        if name in names or not re.fullmatch(r"[0-9a-f]{64}", first) or first != second:
            raise ValueError("invalid or mismatched same-build hash: " + name)
        names.add(name)
    if not any(n.startswith("patch/") for n in names) or not any(n.startswith("module/") for n in names):
        raise ValueError("missing patch or module coverage")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "update"))
    parser.add_argument("--family", default=platform.machine())
    parser.add_argument("--renderer", type=Path)
    parser.add_argument("--report", type=Path, default=ROOT / "build_reports/goldens/report.json")
    args = parser.parse_args()
    candidates = [ROOT / "build/native/golden_render", ROOT / "build/native/Release/golden_render.exe"]
    renderer = args.renderer or next((p for p in candidates if p.is_file()), candidates[0])
    result = subprocess.run([str(renderer.resolve()), str(ROOT / "native/examples")], check=True,
                            capture_output=True, text=True)
    report = validate(json.loads(result.stdout))
    report.update(family=args.family, platform=platform.platform(),
                  build_id=hashlib.sha256(renderer.read_bytes()).hexdigest())
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Same-build hashes passed: {len(report['cases'])}; recorded {args.report}")
    if args.command == "update":
        print("Cross-build hashes are report-only. Any future committed baseline update requires its own PR and reason.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
