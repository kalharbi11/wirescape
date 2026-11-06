#!/usr/bin/env bash
set -euo pipefail
mkdir -p build_reports/pi-ui
report=build_reports/pi-ui/preflight.txt
{
  printf 'Commit: %s\nRunner: %s\n' "${GITHUB_SHA:-local}" "${RUNNER_NAME:-local}"
  uname -srmo
  if [ -r /proc/device-tree/model ]; then tr -d '\0' < /proc/device-tree/model; printf '\n'; fi
  if [ -r /etc/os-release ]; then awk '/^PRETTY_NAME=/' /etc/os-release || true; fi
  awk '/MemTotal/ {print}' /proc/meminfo
} | tee "$report"
if [ "$(uname -m)" != aarch64 ]; then
  printf 'Expected the ARM64 Pi runner.\n' | tee -a "$report"; exit 1
fi
# The runner user must already have access to the logged-in desktop.
# These optional repo variables select that existing session; they do not grant access.
export DISPLAY="${PI_UI_DISPLAY:-${DISPLAY:-}}"
export WAYLAND_DISPLAY="${PI_UI_WAYLAND_DISPLAY:-${WAYLAND_DISPLAY:-}}"
export XDG_RUNTIME_DIR="${PI_UI_XDG_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-}}"
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
  printf 'No desktop session: set DISPLAY or WAYLAND_DISPLAY and XDG_RUNTIME_DIR in the runner service, or PI_UI_* repository variables. Headless/Xvfb results do not close OPEN-5.\n' | tee -a "$report"; exit 1
fi
browser="${WIRESCAPE_CHROMIUM:-}"
if [ -z "$browser" ]; then
  browser="$(command -v chromium || command -v chromium-browser || true)"
fi
if [ -z "$browser" ] || [ ! -x "$browser" ]; then
  printf 'System Chromium missing: install Chromium on the Pi or set WIRESCAPE_CHROMIUM.\n' | tee -a "$report"; exit 1
fi
"$browser" --version | tee -a "$report"
printf 'Desktop: DISPLAY=%s WAYLAND_DISPLAY=%s\n' "$DISPLAY" "$WAYLAND_DISPLAY" | tee -a "$report"
printf 'WIRESCAPE_CHROMIUM=%s\nDISPLAY=%s\nWAYLAND_DISPLAY=%s\nXDG_RUNTIME_DIR=%s\n' "$browser" "$DISPLAY" "$WAYLAND_DISPLAY" "$XDG_RUNTIME_DIR" >> "$GITHUB_ENV"
# Chromium needs explicit Wayland selection when no X11/XWayland display exists.
if [ -z "$DISPLAY" ] && [ -n "$WAYLAND_DISPLAY" ]; then
  printf 'WIRESCAPE_WAYLAND=1\n' >> "$GITHUB_ENV"
fi
