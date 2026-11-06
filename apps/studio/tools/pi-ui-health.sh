#!/usr/bin/env bash
# Read-only host observations; writes only the evidence report in this checkout.
set -euo pipefail

stage=${1:-}
case "$stage" in
  before|after) ;;
  *) printf 'Usage: bash tools/pi-ui-health.sh before|after\n' >&2; exit 2 ;;
esac
mkdir -p build_reports/pi-ui
report="build_reports/pi-ui/health-${stage}.txt"

{
  printf 'Stage: %s\nCaptured UTC: %s\n' "$stage" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  printf 'Checkout commit: '
  git rev-parse HEAD 2>/dev/null || printf 'unavailable\n'
  printf 'Kernel / architecture: '
  uname -srm || true
  printf 'Board model: '
  if [ -r /proc/device-tree/model ]; then
    tr -d '\0' < /proc/device-tree/model; printf '\n'
  else
    printf 'unavailable (physical Pi identity not established by this report)\n'
  fi
  if [ -r /etc/os-release ]; then
    awk '/^PRETTY_NAME=/' /etc/os-release
  fi
  printf '\nLoad / uptime\n'
  if [ -r /proc/loadavg ]; then
    awk '{printf "load_1m=%s load_5m=%s load_15m=%s runnable_total=%s\n", $1, $2, $3, $4}' /proc/loadavg
  else printf 'load: unavailable\n'; fi
  if [ -r /proc/uptime ]; then
    awk '{printf "uptime_seconds=%s\n", $1}' /proc/uptime
  else printf 'uptime: unavailable\n'; fi
  printf '\nMemory\n'
  if [ -r /proc/meminfo ]; then
    awk '/^(MemTotal|MemAvailable|MemFree|SwapTotal|SwapFree):/ {print}' /proc/meminfo
  else printf 'memory: unavailable\n'; fi
  printf '\nCheckout filesystem (KiB)\n'
  if command -v df >/dev/null 2>&1; then
    df -Pk . | awk 'NR == 2 {printf "total_kib=%s used_kib=%s available_kib=%s used_percent=%s\n", $2, $3, $4, $5}' || printf 'disk: unavailable\n'
  else printf 'disk: unavailable (df missing)\n'; fi
  printf '\nThermal zones (millidegrees Celsius)\n'
  zones_found=0
  for zone in /sys/class/thermal/thermal_zone*; do
    [ -r "$zone/temp" ] || continue
    zones_found=1
    printf '%s type=' "${zone##*/}"
    if [ -r "$zone/type" ]; then cat "$zone/type" || true; else printf 'unknown\n'; fi
    printf 'temperature_mC='
    cat "$zone/temp" || printf 'unavailable\n'
  done
  if [ "$zones_found" -eq 0 ]; then printf 'temperature: unavailable (no readable thermal zone)\n'; fi
  printf '\nPi firmware health\n'
  if command -v vcgencmd >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1; then
    # Bounded, read-only queries; no sudo, installation or hardware configuration.
    if temperature=$(timeout 5s vcgencmd measure_temp 2>/dev/null); then
      printf '%s\n' "$temperature"
    else printf 'firmware temperature: unavailable (query failed or timed out)\n'; fi
    if throttled=$(timeout 5s vcgencmd get_throttled 2>/dev/null); then
      printf '%s\n' "$throttled"
      if [[ "$throttled" =~ ^throttled=0x([0-9a-fA-F]{1,8})$ ]]; then
        flags=$((16#${BASH_REMATCH[1]}))
        printf 'Current: undervoltage=%d frequency_capped=%d throttled=%d soft_temperature_limit=%d\n' \
          "$((flags & 1))" "$(((flags >> 1) & 1))" "$(((flags >> 2) & 1))" "$(((flags >> 3) & 1))"
        printf 'Occurred since boot: undervoltage=%d frequency_capped=%d throttled=%d soft_temperature_limit=%d\n' \
          "$(((flags >> 16) & 1))" "$(((flags >> 17) & 1))" "$(((flags >> 18) & 1))" "$(((flags >> 19) & 1))"
        printf 'Historical bits may predate this test; compare before/after and uptime. They are not cleared.\n'
      else printf 'Throttle decoding: unavailable (unexpected firmware response)\n'; fi
    else printf 'throttle flags: unavailable (query failed or timed out)\n'; fi
  else
    printf 'firmware temperature / throttle flags: unavailable (vcgencmd or timeout missing)\n'
  fi
  printf '\nContext only: before/after samples do not establish peak temperature or sustained thermal stability.\n'
} | tee "$report"
