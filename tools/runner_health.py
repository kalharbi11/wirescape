#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Print a small, read-only Pi runner health snapshot; observations are not diagnosis."""
import json
import re
import shutil
import subprocess
from pathlib import Path


def command(argv, timeout=4):
    """Run an optional local command without exposing its error text."""
    if not shutil.which(argv[0]):
        return {"status": "unavailable"}
    try:
        result = subprocess.run(argv, capture_output=True, text=True, timeout=timeout,
                                check=False)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}
    except OSError:
        return {"status": "unavailable"}
    if result.returncode:
        return {"status": "unavailable"}
    return {"status": "ok", "output": result.stdout}


def read_text(path):
    try:
        return Path(path).read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def basic_stats():
    uptime = read_text("/proc/uptime")
    load = read_text("/proc/loadavg")
    mem = read_text("/proc/meminfo")
    disk = shutil.disk_usage("/")
    memory = {}
    if mem:
        wanted = {"MemTotal", "MemAvailable", "SwapTotal", "SwapFree"}
        for line in mem.splitlines():
            key, sep, value = line.partition(":")
            if sep and key in wanted:
                memory[key] = int(value.strip().split()[0])
    return {
        "uptime_seconds": float(uptime.split()[0]) if uptime else None,
        "load": load.split()[:3] if load else None,
        "memory_kib": memory,
        "disk_bytes": {"total": disk.total, "free": disk.free},
    }


def optional_reading(path, divisor=1):
    value = read_text(path)
    try:
        return int(value) // divisor if value is not None else None
    except ValueError:
        return None


def service_state():
    result = command(["systemctl", "show", "tailscaled", "--no-pager", "--property=ActiveState,SubState,Restart,NRestarts"])
    if result["status"] != "ok":
        return {"status": result["status"]}
    allowed = {"ActiveState", "SubState", "Restart", "NRestarts"}
    return {key: value for line in result["output"].splitlines() for key, sep, value in [line.partition("=")] if sep and key in allowed}


def wifi_state():
    result = command(["nmcli", "-t", "--fields", "TYPE,STATE", "device", "status"])
    if result["status"] != "ok":
        return {"status": result["status"]}
    rows = []
    for line in result["output"].splitlines():
        fields = line.split(":", 1)
        if len(fields) == 2 and fields[0] in {"wifi", "wifi-p2p"}:
            rows.append({"type": fields[0], "state": fields[1]})
    return {"status": "ok", "devices": rows}


def tailscale_state():
    result = command(["tailscale", "status", "--json"])
    if result["status"] != "ok":
        return {"status": result["status"]}
    try:
        data = json.loads(result["output"])
        return {"status": "ok", "backend": data.get("BackendState"), "self_online": (data.get("Self") or {}).get("Online"), "health_count": len(data.get("Health") or [])}
    except (ValueError, AttributeError, TypeError):
        return {"status": "unavailable"}


def throttled_state():
    result = command(["vcgencmd", "get_throttled"])
    if result["status"] != "ok":
        return {"status": result["status"]}
    try:
        return {"status": "ok", "flags": int(result["output"].strip().split("=", 1)[1], 16)}
    except (IndexError, ValueError):
        return {"status": "unavailable"}


def kernel_counts():
    result = command(["journalctl", "-k", "-b", "-n", "300", "--no-pager"])
    if result["status"] != "ok":
        return {"status": result["status"]}
    lines = result["output"].lower().splitlines()
    if not lines or any("-- no entries --" in line for line in lines):
        return {"status": "unavailable"}
    terms = {"oom": r"out of memory|oom[-_ ]kill|killed process", "undervoltage": r"under[- ]?voltage",
             "thermal": r"thermal throttl|overheat|critical temperature", "mmc": r"mmc.*(?:error|timeout|failed)"}
    return {"status": "ok", **{name: sum(bool(re.search(pattern, line)) for line in lines) for name, pattern in terms.items()}}


def report():
    return {"note": "read-only snapshot; observations do not identify an outage cause", "system": basic_stats(),
            "temperature_c": optional_reading("/sys/class/thermal/thermal_zone0/temp", 1000),
            "throttled": throttled_state(),
            "tailscaled": service_state(), "wifi": wifi_state(),
            "tailscale": tailscale_state(), "kernel_warnings": kernel_counts()}


def main():
    print(json.dumps(report(), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
