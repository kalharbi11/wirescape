# SPDX-License-Identifier: GPL-3.0-only
"""Assert same-build offline hashes; record cross-build hashes without comparing them."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import struct

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


def hash_streams(report):
    """Hash the renderer's canonical bytes with the same SHA256 used by Qt."""
    for case in report.get("cases", [report]):
        for field in ("first", "second", "changed"):
            key = field + "_bytes"
            if key in case:
                case[field] = hashlib.sha256(bytes.fromhex(case.pop(key))).hexdigest()
    return report


def serialization_oracle(report):
    # Independent frozen Qt qToLittleEndian/float-bit contract, including sign,
    # nontrivial high frame bits, all metadata, four UMP words and raw payload.
    samples = struct.pack("<4f", 0.0, -0.0, -0.5, 1.0)
    fields = [0x89abcdef, 0x01234567, 3, 7, 11, 13, 17, 19,
              0xffffffff, 0xfffffffe, 0xfffffffd, 23, 29, 31, 37, 4, 4]
    event = struct.pack("<17I2f4I", *fields, -0.5, 0.25, 41, 43, 47, 53) + bytes([0, 127, 128, 255])
    if bytes.fromhex(report["samples"]) != samples or bytes.fromhex(report["event"]) != event:
        raise ValueError("canonical Qt byte serialization changed")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "update", "event-self-test", "serialization-self-test"))
    parser.add_argument("--family", default=platform.machine())
    parser.add_argument("--renderer", type=Path)
    parser.add_argument("--compare-renderer", type=Path, help="Compare a frozen Qt renderer from the same source tree")
    parser.add_argument("--report", type=Path, default=ROOT / "build_reports/goldens/report.json")
    args = parser.parse_args()
    candidates = [ROOT / "build/native/golden_render", ROOT / "build/native/Release/golden_render.exe",
                  ROOT / "build/native/golden_render.exe"]
    renderer = args.renderer or next((p for p in candidates if p.is_file()), candidates[0])
    argument = "--" + args.command if args.command.endswith("self-test") else str(ROOT / "native/examples")
    result = subprocess.run([str(renderer.resolve()), argument], check=True, capture_output=True, text=True)
    raw = json.loads(result.stdout)
    if args.command == "serialization-self-test":
        serialization_oracle(raw)
        print("PASS frozen Qt sample/event byte contract")
        return 0
    report = hash_streams(raw)
    if args.command == "event-self-test":
        if report["first"] != report["second"] or report["first"] == report["changed"]:
            raise ValueError("event hash equality/sensitivity failed")
        print("PASS canonical event hashes: fresh instances match; changed event value differs")
        return 0
    report = validate(report)
    if args.compare_renderer:
        old = subprocess.run([str(args.compare_renderer.resolve()), str(ROOT / "native/examples")],
                             check=True, capture_output=True, text=True)
        previous = validate(json.loads(old.stdout))
        def key(case):
            return case["name"]
        if sorted(previous["cases"], key=key) != sorted(report["cases"], key=key):
            raise ValueError("frozen Qt and Qt-free render hashes differ")
        print("PASS frozen Qt vs Qt-free: every patch/module signal/event SHA256 matches")
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
