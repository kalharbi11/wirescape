"""Offline ABI2 evidence collector. Build first with tools/dev.py build.

No devices, downloads, package installation or source mutation. Runs bounded
acceptance binaries and records failures, protected hashes and WAV statistics.
"""
from __future__ import annotations
import argparse
import ctypes
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build_reports/abi2"

def wav_stats(path: Path) -> dict:
    raw = path.read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(f"not WAV: {path}")
    fmt = None
    values = ()
    pos = 12
    while pos + 8 <= len(raw):
        name, length = struct.unpack_from("<4sI", raw, pos)
        data = raw[pos+8:pos+8+length]
        if name == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", data)
        if name == b"data":
            values = struct.unpack(f"<{length//4}f", data)
        pos += 8 + length + (length & 1)
    if not fmt or fmt[0] != 3 or fmt[5] != 32 or not values:
        raise ValueError(f"expected nonempty float32 WAV: {path}")
    finite = all(math.isfinite(x) for x in values)
    return {"path": str(path.relative_to(ROOT)), "sha256": hashlib.sha256(raw).hexdigest(),
            "frames": len(values)//fmt[1], "channels": fmt[1], "sample_rate": fmt[2],
            "finite": finite, "peak": max(map(abs, values)),
            "rms": math.sqrt(sum(x*x for x in values)/len(values))}

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--configuration", default="Release")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    binary = ROOT / "build/native" / args.configuration
    exe = ".exe" if sys.platform == "win32" else ""
    results = []
    commands = [
        ["ctest", "--test-dir", str(ROOT/"build/native"), "-C", args.configuration,
         "-R", "^abi2_", "--output-on-failure"],
        [str(ROOT/"build/native/Debug"/f"abi2_engine_tests{exe}")],
    ]
    for i, cmd in enumerate(commands):
        try:
            proc = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True, timeout=120)
            output = proc.stdout + proc.stderr
            code = proc.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            code, output = -1, str(error)
        log = OUT / f"acceptance-{i}.log"
        log.write_text(output, encoding="utf-8")
        results.append({"command": cmd, "returncode": code, "passed": code == 0,
                        "log": str(log.relative_to(ROOT))})
    baseline = json.loads((ROOT/"docs/history/MIGRATION_SOURCE_MANIFEST_2026-10-26.json").read_text(encoding="utf-8"))
    changed = [path for path, expected in baseline["protected_sources"].items()
               if not (ROOT/path).is_file() or hashlib.sha256((ROOT/path).read_bytes()).hexdigest() != expected]
    forbidden = re.compile(r"#\s*include\s*<(vector|string|thread|mutex|iostream|filesystem)>")
    bad_headers = [str(p.relative_to(ROOT)) for p in (ROOT/"native/modules2").glob("*/dsp.h")
                   if forbidden.search(p.read_text(encoding="utf-8"))]
    exports = []
    for path in sorted(binary.glob("ws2_*.dll")):
        try:
            library = ctypes.CDLL(str(path))
            library.ws_module2_abi_version.restype = ctypes.c_uint32
            library.ws_module2_entry.restype = ctypes.c_void_p
            ok = library.ws_module2_abi_version() == 2 and bool(library.ws_module2_entry())
            exports.append({"library": path.name, "passed": ok})
        except (OSError, AttributeError) as error:
            exports.append({"library": path.name, "passed": False, "error": str(error)})
    waves = [wav_stats(p) for p in sorted(OUT.glob("*.wav"))]
    fixture_dir = ROOT/"native/tests/fixtures/abi2"
    fixture_manifest = json.loads((fixture_dir/"manifest.json").read_text(encoding="utf-8"))
    fixture_changes = [name for name, expected in fixture_manifest["files"].items()
                       if not (OUT/name).is_file() or hashlib.sha256((OUT/name).read_bytes()).hexdigest() != expected]
    equivalence_path = ROOT/"build/native/abi2_equivalence.json"
    equivalence = json.loads(equivalence_path.read_text()) if equivalence_path.exists() else None
    benchmark_path = OUT/"benchmark.json"
    benchmark = json.loads(benchmark_path.read_text()) if benchmark_path.exists() else None
    passed = all(x["passed"] for x in results) and all(x["passed"] for x in exports) and not changed and not bad_headers and not fixture_changes and all(x["finite"] for x in waves)
    report = {"created_utc": datetime.now(timezone.utc).isoformat(), "functional_passed": passed,
              "physical_audio_opened": False, "commands": results,
              "protected_count": len(baseline["protected_sources"]), "protected_changed": changed,
              "forbidden_dsp_headers": bad_headers, "waves": waves,
              "abi2_dll_exports": exports,
              "frozen_fixture_changes": fixture_changes,
              "abi2_binary_hashes": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in sorted(binary.glob("*"))
                                    if p.suffix in (".exe", ".dll") and p.name.startswith(("abi2_", "ws2_"))},
              "equivalence": equivalence, "benchmark": benchmark,
              "mono_performance_target_passed": bool(benchmark and benchmark["mono_ratio"] <= 1.05),
              "limitations": ["No ARM compiler or hardware proof; Debug CRT is not ASan.",
                              "Allocation hook observes C++ new/new[] including aligned/nothrow, not arbitrary malloc inside third-party DLLs.",
                              "Worst timing is the largest observed block, not a hard real-time bound."]}
    (OUT/"summary.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(f"Functional evidence: {'PASS' if passed else 'FAIL'}; protected changes: {len(changed)}; "
          f"mono performance target: {'PASS' if report['mono_performance_target_passed'] else 'MISSED'}")
    print(OUT/"summary.json")
    return 0 if passed else 1

if __name__ == "__main__":
    raise SystemExit(main())
