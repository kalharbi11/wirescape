#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Owner-run source update and standalone app build. Never starts audio/MIDI.
set -eu

usage() {
    echo "Usage: tools/pi/update-and-build.sh <commit-sha|origin/branch|tag>" >&2
    exit 2
}
[ "$#" -eq 1 ] || usage
ref=$1
# This is a Git revision, not a command-line option or rev-parse expression.
case "$ref" in
    ''|-*|*'..'*|*' '*|*'~'*|*'^'*|*':'*|*'@{'*|*'\'*|*'?'*|*'*'*|*'['*) usage ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
# Do not silently discard the owner's edits or switch a dirty worktree.
if [ -n "$(git -C "$root" status --porcelain)" ]; then
    echo "Working tree is not clean; save or commit changes before updating." >&2
    exit 1
fi

echo "Fetching origin and resolving $ref..."
git -C "$root" fetch --prune origin
sha=$(git -C "$root" rev-parse --verify --end-of-options "$ref^{commit}")
git -C "$root" switch --detach "$sha"
actual=$(git -C "$root" rev-parse HEAD)
if [ "$actual" != "$sha" ]; then
    echo "Checked-out revision differs from requested revision." >&2
    exit 1
fi
printf 'Building exact revision %s\n' "$sha"

cd "$root"
python3 tools/dev.py build --headless --config Release
if [ ! -x "$root/build/native/wirescape-engine" ]; then
    echo "Missing build/native/wirescape-engine after Release build." >&2
    exit 1
fi

(cd "$root/apps/studio" && npm ci && npm run build)
(cd "$root/apps/desktop" && npm ci && npm run build)

if [ ! -f "$root/apps/studio/dist/index.html" ] ||
   [ ! -f "$root/apps/desktop/build/main.js" ]; then
    echo "Desktop/UI build outputs are missing." >&2
    exit 1
fi

echo "Built $sha (no audio/MIDI device opened)."
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
