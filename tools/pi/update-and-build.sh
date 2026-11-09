#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Owner-run source update and standalone app build. Never starts audio/MIDI.
set -eu

usage() {
    echo "Usage: tools/pi/update-and-build.sh <commit-sha|origin/branch|tag> [--jobs 1..4]" >&2
    exit 2
}
[ "$#" -eq 1 ] || { [ "$#" -eq 3 ] && [ "$2" = '--jobs' ]; } || usage
ref=$1
jobs=2
if [ "$#" -eq 3 ]; then jobs=$3; fi
case "$jobs" in 1|2|3|4) ;; *) usage ;; esac
# This is a Git revision, not a command-line option or rev-parse expression.
case "$ref" in
    ''|-*|*'..'*|*' '*|*'~'*|*'^'*|*':'*|*'@{'*|*'\'*|*'?'*|*'*'*|*'['*) usage ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
# Preserve the owner's edits and caches; preflight is read-only and offline.
if [ -n "$(git -C "$root" status --porcelain)" ]; then
    echo "Working tree is not clean; save or commit changes before updating." >&2
    exit 1
fi
python3 "$root/tools/pi/preflight.py" --check-only --root "$root" || {
    echo "Preflight blocked before fetch; fix the reported prerequisites and retry." >&2
    exit 1
}

echo "Fetching origin and resolving $ref..."
git -C "$root" fetch --prune origin
sha=$(git -C "$root" rev-parse --verify --end-of-options "$ref^{commit}")
git -C "$root" switch --detach "$sha"
actual=$(git -C "$root" rev-parse HEAD)
if [ "$actual" != "$sha" ]; then
    echo "Checked-out revision differs from requested revision." >&2
    exit 1
fi
# The selected revision may have different dependency pins and requirements.
python3 "$root/tools/pi/preflight.py" --check-only --root "$root" || {
    echo "Selected SHA $sha failed preflight; no build attempted." >&2
    exit 1
}

report="$root/build_reports/pi-source-build/$sha"
mkdir -p "$report"
export CMAKE_BUILD_PARALLEL_LEVEL="$jobs"
cd "$root"
echo "Building exact revision $sha with $jobs native jobs; logs in $report"

stage() {
    name=$1
    shift
    python3 "$root/tools/pi/preflight.py" --root "$root" --run-stage "$name" --report-dir "$report" -- "$@" || {
        rc=$?
        echo "Stage $name failed (exit $rc). See $report/$name.log and summary.json; repair and rerun this exact revision." >&2
        exit "$rc"
    }
}

stage native python3 tools/dev.py build --headless --config Release
if [ ! -x "$root/build/native/wirescape-engine" ]; then
    echo "Stage native failed: missing build/native/wirescape-engine after Release build; inspect $report/native.log" >&2
    exit 1
fi
stage studio-ci npm ci
stage studio-build npm run build
stage desktop-ci npm ci
stage desktop-build npm run build

if [ ! -f "$root/apps/studio/dist/index.html" ] ||
   [ ! -f "$root/apps/desktop/build/main.js" ]; then
    echo "Stage desktop-build failed: missing UI outputs; see $report/desktop-build.log" >&2
    exit 1
fi

echo "Built $sha (no audio/MIDI device opened). Report: $report/summary.json"
echo "To launch with the owner's device consent:"
python3 - "$root" <<'PY'
import shlex
import sys
from pathlib import Path

root = Path(sys.argv[1])
desktop = root / "apps/desktop"
cmd = [
    str(desktop / "node_modules/.bin/electron"), str(desktop / "build/main.js"),
    "--engine", str(root / "build/native/wirescape-engine"),
    "--confirm-audio", "--confirm-midi",
]
print("  cd " + shlex.quote(str(desktop)) + " && " + shlex.join(cmd))
PY
