# SPDX-License-Identifier: GPL-3.0-only
"""Device-free play regression, including clean interrupted teardown on POSIX."""
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import time

runner, patch = map(str, sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix="play-", dir=Path.cwd()) as work:
    wav = Path(work) / "offline.wav"
    result = subprocess.run([runner, "play", patch, "--output", str(wav)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report["offline"] and report["loader"] == "runtime_patch"
    assert report["frames"] == 480000 and report["peak"] > 0 and report["rms"] > 0
    data = wav.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    assert struct.unpack_from("<HHIIHH", data, 20) == (1, 2, 48000, 192000, 4, 16)
    assert len(data) == 44 + 480000 * 4
    # Future service hooks fail before any endpoint is opened.
    refused = subprocess.run([runner, "play", patch, "--midi", "fake"], capture_output=True, timeout=10)
    assert refused.returncode != 0 and b"confirmation" in refused.stderr
    if os.name != "nt":
        interrupted = Path(work) / "interrupted.wav"
        process = subprocess.Popen([runner, "play", patch, "--pace", "--output", str(interrupted)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while (not interrupted.exists() or interrupted.stat().st_size < 4096) and time.monotonic() < deadline:
                time.sleep(0.02)
            assert interrupted.exists(), "offline renderer did not start"
            process.send_signal(signal.SIGINT)
            stdout, stderr = process.communicate(timeout=10)
            assert process.returncode == 0, stderr
            stopped = json.loads(stdout)
            assert 0 < stopped["frames"] < 480000
            assert interrupted.stat().st_size == 44 + stopped["frames"] * 4
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()
print("offline play WAV, device flags and teardown pass")
