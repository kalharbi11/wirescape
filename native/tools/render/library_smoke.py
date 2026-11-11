# SPDX-License-Identifier: GPL-3.0-only
"""Device-free production-engine smoke for every shared Studio patch.

Uses the existing wirescape_render binary and the evidence harness's
already-established SILENT_PEAK, without adding a new audio threshold.
Finite/non-silent is a minimum regression gate, not an audible quality verdict.
"""
import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from evidence import SILENT_PEAK

RATE = 48000
FRAMES = RATE * 2
BLOCK = 128
METRIC_FIELDS = ("faults", "late_events", "dropped_events", "ring_full_events",
                 "stale_events", "arena_full_events")


def analyze_wav(path):
    raw = path.read_bytes()
    if (len(raw) != 44 + FRAMES * 8 or raw[:4] != b"RIFF" or
            raw[8:16] != b"WAVEfmt " or raw[36:40] != b"data" or
            struct.unpack_from("<IHHIIHH", raw, 16) !=
            (16, 3, 2, RATE, RATE * 8, 8, 32)):
        raise ValueError("invalid stereo 48-kHz float32 WAV")
    peaks = [0.0, 0.0]
    energies = [0.0, 0.0]
    for left, right in struct.iter_unpack("<ff", memoryview(raw)[44:]):
        for channel, sample in enumerate((left, right)):
            if not math.isfinite(sample):
                raise ValueError("non-finite audio")
            peaks[channel] = max(peaks[channel], abs(sample))
            energies[channel] += sample * sample
    if max(peaks) <= SILENT_PEAK:
        raise ValueError(f"silent patch: L={peaks[0]:.7g}, R={peaks[1]:.7g} peak")
    return peaks, [math.sqrt(energy / FRAMES) for energy in energies]


def smoke(renderer, patch_path, directory):
    patch = json.loads(patch_path.read_text(encoding="utf-8"))
    keyboard_nodes = [m["id"] for m in patch["modules"] if m["module"] == "host.keyboard"]
    # The public folder has no hardware MIDI. A scripted note makes keyboard
    # patches audible without a device, while autoplay patches run untouched.
    events = []
    for node in keyboard_nodes:
        events.extend((
            {"frame": 0, "type": "noteOn", "node": node, "key": 60},
            {"frame": RATE, "type": "noteOff", "node": node, "key": 60},
        ))
    events.sort(key=lambda event: event["frame"])
    patch_file = directory / "patch.json"
    script_file = directory / "events.json"
    wav_file = directory / "audio.wav"
    patch_file.write_text(json.dumps(patch), encoding="utf-8")
    script_file.write_text(json.dumps(events), encoding="utf-8")
    seed = patch.get("settings", {}).get("seed", 42)
    command = [
        str(renderer), "--patch", str(patch_file), "--script", str(script_file),
        "--out", str(wav_file), "--rate", str(RATE), "--block", str(BLOCK),
        "--seconds", "2", "--seed", str(seed),
    ]
    result = subprocess.run(command, capture_output=True, text=True,
                            timeout=60, check=False)
    if result.returncode:
        raise ValueError(f"renderer exit {result.returncode}: {result.stderr[-500:]}")
    lines = [line[len("render_metrics "):] for line in result.stdout.splitlines()
             if line.startswith("render_metrics ")]
    if len(lines) != 1:
        raise ValueError("expected one render_metrics line")
    metrics = json.loads(lines[0])
    if any(type(metrics.get(key)) is not int or metrics[key] != 0
           for key in METRIC_FIELDS):
        raise ValueError(f"render fault/event counters: {metrics}")
    return analyze_wav(wav_file)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--render", type=Path, required=True)
    parser.add_argument("--patches", type=Path, required=True)
    args = parser.parse_args()
    if not args.render.is_file():
        parser.error("production wirescape_render binary is missing")
    patches = sorted(args.patches.glob("*.json"))
    if not patches:
        parser.error("shared patch folder is empty")
    failed = []
    for patch in patches:
        try:
            with tempfile.TemporaryDirectory(prefix="ws-patch-smoke-") as tmp:
                peaks, rms = smoke(args.render.resolve(), patch, Path(tmp))
            print(f"PASS {patch.name}: L/R peak={peaks[0]:.6f}/{peaks[1]:.6f} "
                  f"RMS={rms[0]:.6f}/{rms[1]:.6f}", flush=True)
        except (OSError, ValueError, KeyError, TypeError,
                subprocess.TimeoutExpired) as exc:
            failed.append(patch.name)
            print(f"FAIL {patch.name}: {exc}", file=sys.stderr, flush=True)
    print(f"Library offline smoke: {len(patches) - len(failed)}/{len(patches)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
