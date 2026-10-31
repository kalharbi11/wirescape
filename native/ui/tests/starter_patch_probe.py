# SPDX-License-Identifier: GPL-3.0-only
"""Offline CLI acceptance for the six bundled starters; no devices or user data."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EXAMPLES = ROOT / "native" / "examples"
NAMES = ("minimal", "lfo_filter", "midi", "poly", "chaos", "macro")


def cli_binary():
    candidates = (
        ROOT / "build/native/wirescape_cli",
        ROOT / "build/native/Release/wirescape_cli.exe",
        ROOT / "build/native/Debug/wirescape_cli.exe",
    )
    for path in candidates:
        if path.is_file():
            return path
    raise RuntimeError("wirescape_cli is missing; build headless first")


def main():
    binary = cli_binary()
    with tempfile.TemporaryDirectory(prefix="wirescape-starters-") as folder:
        for name in NAMES:
            patch = EXAMPLES / f"starter_{name}.json"
            if not patch.is_file():
                raise RuntimeError(f"missing starter: {patch}")
            output = Path(folder) / f"{name}.wav"
            command = (
                str(binary), "play", str(patch), "--modules", str(binary.parent),
                "--rate", "48000", "--block", "128", "--seconds", "1",
                "--output", str(output),
            )
            result = subprocess.run(
                command, cwd=ROOT, capture_output=True, text=True, timeout=60,
                check=False,
            )
            if result.returncode:
                raise RuntimeError(
                    f"{name} offline render failed: {result.stderr[-1200:]}"
                )
            report = json.loads(result.stdout.strip().splitlines()[-1])
            if (not report.get("offline") or report.get("loader") != "runtime_patch"
                    or report.get("frames") != 48000 or report.get("peak", 0) <= 0
                    or report.get("rms", 0) <= 0 or output.stat().st_size <= 44):
                raise RuntimeError(f"{name} has no valid non-silent offline render: {report}")
            print(f"{name}: offline peak {report['peak']}; rms {report['rms']}")
    print("Six device-free starter patches: 6/6 non-silent offline renders")


if __name__ == "__main__":
    main()
