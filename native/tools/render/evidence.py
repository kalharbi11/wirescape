# SPDX-License-Identifier: GPL-3.0-only
"""Device-free behavioral evidence for wirescape_render; stdlib only."""
import argparse
import hashlib
import json
import math
import os
import platform
import struct
import subprocess
import sys
from pathlib import Path

VERSION = "render-evidence-v1"
ACTIVE_RMS = 0.01       # > -40 dBFS
SILENT_PEAK = 0.0001    # < -80 dBFS
FREQUENCY_TOLERANCE = 0.02


def sha(data):
    return hashlib.sha256(data).hexdigest()


def check(condition, message):
    if not condition:
        raise ValueError(message)


def wav_samples(path, rate, frames):
    data = path.read_bytes()
    check(len(data) == 44 + frames * 8 and len(data) <= 20 * 1024 * 1024, "WAV length/size")
    check(data[:4] == b"RIFF" and data[8:16] == b"WAVEfmt " and data[36:40] == b"data", "WAV header")
    check(struct.unpack_from("<I", data, 4)[0] == len(data) - 8, "RIFF size")
    check(struct.unpack_from("<IHHIIHH", data, 16) ==
          (16, 3, 2, rate, rate * 8, 8, 32), "WAV must be stereo float32")
    check(struct.unpack_from("<I", data, 40)[0] == frames * 8, "WAV data length")
    values = struct.unpack_from("<" + str(frames * 2) + "f", data, 44)
    left, right = values[::2], values[1::2]
    check(all(math.isfinite(x) for x in values), "non-finite audio")
    check(max(abs(a - b) for a, b in zip(left, right)) < 1e-5, "stereo bus mismatch")
    return left


def measure(samples, case):
    check(samples and all(math.isfinite(x) for x in samples), "non-finite audio")
    peak = max(abs(x) for x in samples)
    check(peak <= case["peak_limit"], "peak bound exceeded")
    results = [{"check": "finite", "passed": True},
               {"check": "peak", "value": peak, "maximum": case["peak_limit"], "unit": "FS"}]
    for window in case["windows"]:
        a, b = window["start"], window["end"]
        check(0 <= a < b <= len(samples), "invalid window bounds")
        region = samples[a:b]
        if window["kind"] == "active":
            rms = math.sqrt(sum(x * x for x in region) / len(region))
            check(rms > ACTIVE_RMS, f"active window {a}:{b} silent ({rms:.6g} RMS)")
            results.append({"check": "active", "start_frame": a, "end_frame": b, "rms_fs": rms,
                            "minimum_rms_fs": ACTIVE_RMS})
        elif window["kind"] == "silent":
            value = max(abs(x) for x in region)
            check(value < SILENT_PEAK, f"silent window {a}:{b} active ({value:.6g} peak)")
            results.append({"check": "silent", "start_frame": a, "end_frame": b, "peak_fs": value,
                            "maximum_peak_fs": SILENT_PEAK})
        else:
            raise ValueError("unknown window check")
    for window in case["frequencies"]:
        a, b, want = window["start"], window["end"], window["hz"]
        check(0 <= a < b <= len(samples) and want > 0, "invalid frequency window")
        region = samples[a:b]
        crossings = sum((x < 0 <= y) or (x >= 0 > y) for x, y in zip(region, region[1:]))
        observed = crossings * case["rate"] / (2 * (len(region) - 1))
        check(abs(observed / want - 1) <= FREQUENCY_TOLERANCE,
              f"frequency window {a}:{b}: {observed:.3f} Hz, expected {want} Hz")
        results.append({"check": "zero_crossing", "start_frame": a, "end_frame": b,
                        "measured_hz": observed, "expected_hz": want,
                        "tolerance_fraction": FREQUENCY_TOLERANCE})
    return results


def negative_fixtures():
    rate, n = 48000, 192000
    sine = [math.sin(2 * math.pi * 220 * i / rate) * 0.5 for i in range(n)]
    base = {"rate": rate, "peak_limit": 1.5, "windows": [], "frequencies": []}
    checks = [
        ("NaN", [float("nan")], base),
        ("clipping", [2.0], base),
        ("silence", [0.0] * n, dict(base, windows=[{"kind": "active", "start": 100, "end": 300}])),
        ("wrong silent window", sine, dict(base, windows=[{"kind": "silent", "start": 1000, "end": 3000}])),
        ("wrong frequency", sine, dict(base, frequencies=[{"start": 2000, "end": 48000, "hz": 440}]))
    ]
    for name, samples, case in checks:
        try:
            measure(samples, case)
        except ValueError:
            print("negative fixture rejected:", name)
        else:
            raise ValueError("negative fixture unexpectedly passed: " + name)


def git_head():
    root = Path(__file__).resolve().parents[3]
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                            capture_output=True, text=True, timeout=5, check=True)
    head = result.stdout.strip()
    check(len(head) == 40 and all(c in "0123456789abcdef" for c in head), "git HEAD unavailable")
    return head


def publish(out, info, results, error):
    info["status"] = "failed" if error else "passed"
    info["assertions"] = results
    if error:
        info["error"] = str(error)[:1200]
    (out / "measurements.json").write_text(json.dumps(info, indent=2, sort_keys=True) + "\n")
    lines = [f"# {info['case']} — {info['status']}", "",
             f"Source: \x60{info['source_sha']}\x60; harness: \x60{info['harness_sha256']}\x60",
             f"Patch SHA-256: \x60{info['patch_sha256']}\x60; script SHA-256: \x60{info['script_sha256']}\x60",
             f"Platform: {info['platform']} / {info['architecture']}; compiler: {info['compiler']}; configuration: {info['configuration']}",
             f"Rate: {info['rate']} Hz; block: {info['block']} frames; length: {info['frames']} frames; seed: {info['seed']}",
             "", "| Assertion | Measurement |", "|---|---|"]
    for item in results:
        value = item.get("measured_hz", item.get("rms_fs", item.get("peak_fs", item.get("value", "finite"))))
        lines.append(f"| {item['check']} | {value} |")
    if error:
        lines.extend(["", "FAIL: " + str(error)[:1200]])
    lines.extend(["", "Automated offline assertions only; not real-device or subjective-listening approval."])
    (out / "report.md").write_text("\n".join(lines) + "\n")


def run(args):
    case_path = Path(args.case)
    source = json.loads(case_path.read_text())
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    patch = (json.dumps(source["patch"], sort_keys=True, separators=(",", ":")) + "\n").encode()
    script = (json.dumps(source["events"], sort_keys=True, separators=(",", ":")) + "\n").encode()
    (out / "patch.json").write_bytes(patch)
    (out / "events.json").write_bytes(script)
    rate, block, seconds, seed = (source[key] for key in ("rate", "block", "seconds", "seed"))
    frames = round(rate * seconds)
    info = {"case": source["name"], "version": VERSION, "source_sha": git_head(),
            "source_kind": "pull_request_merge" if os.getenv("GITHUB_EVENT_NAME") == "pull_request" else "checkout_head",
            "workflow_run_url": (f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
                                 if os.getenv("GITHUB_REPOSITORY") and os.getenv("GITHUB_RUN_ID") else None),
            "harness_sha256": sha(Path(__file__).read_bytes()), "patch_sha256": sha(patch),
            "script_sha256": sha(script), "rate": rate, "block": block, "seconds": seconds,
            "frames": frames, "seed": seed, "events": source["events"],
            "platform": platform.system(), "architecture": platform.machine(),
            "compiler": args.compiler, "configuration": args.configuration,
            "render_command": ["wirescape_render", "--patch", "patch.json", "--script", "events.json",
                               "--out", "audio.wav", "--rate", str(rate), "--block", str(block),
                               "--seconds", str(seconds), "--seed", str(seed)]}
    results, error = [], None
    try:
        command = [args.render, "--patch", str(out / "patch.json"), "--script", str(out / "events.json"),
                   "--out", str(out / "audio.wav"), "--rate", str(rate), "--block", str(block),
                   "--seconds", str(seconds), "--seed", str(seed)]
        proc = subprocess.run(command, capture_output=True, text=True, timeout=60)
        check(proc.returncode == 0, f"renderer exit {proc.returncode}: {proc.stderr[-600:]}")
        samples = wav_samples(out / "audio.wav", rate, frames)
        results = measure(samples, source)
        size = sum(f.stat().st_size for f in out.iterdir() if f.is_file())
        check(size <= 7 * 1024 * 1024, "case artifacts exceed 7 MiB")
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        error = exc
    publish(out, info, results, error)
    if error:
        print("render evidence FAIL:", error, file=sys.stderr)
        return 1
    print("render evidence PASS:", source["name"], out)
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--render")
    parser.add_argument("--case")
    parser.add_argument("--out")
    parser.add_argument("--compiler", default="unavailable")
    parser.add_argument("--configuration", default="unavailable")
    options = parser.parse_args()
    if options.negative:
        negative_fixtures()
        sys.exit(0)
    parser.error("require --render, --case and --out") if not all(
        [options.render, options.case, options.out]) else None
    sys.exit(run(options))
