#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Owner-run per-user autostart; never uses sudo or edits system files.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
python3 - "$root" "${1:-}" <<'PY'
from pathlib import Path
import os
import sys

root = Path(sys.argv[1])
remove = sys.argv[2] == '--remove'
if sys.argv[2] not in ('', '--remove'):
    raise SystemExit('Usage: install-autostart.sh [--remove]')
home = Path.home()
destination = home / '.config/autostart/wirescape.desktop'
if remove:
    destination.unlink(missing_ok=True)
    print(f'Removed {destination}')
    raise SystemExit(0)

launcher = home / '.local/bin/wirescape'
if not launcher.is_file() or not os.access(launcher, os.X_OK):
    raise SystemExit(f'Missing executable {launcher}; install the Wirescape launcher first')
template = (root / 'tools/pi/wirescape.desktop.in').read_text(encoding='utf-8')
entry = template.replace('@LAUNCHER@', str(launcher))
if '@LAUNCHER@' in entry:
    raise SystemExit('Desktop template was not expanded')
destination.parent.mkdir(parents=True, exist_ok=True)
if not destination.exists() or destination.read_text(encoding='utf-8') != entry:
    temporary = destination.with_suffix('.desktop.tmp')
    temporary.write_text(entry, encoding='utf-8')
    temporary.chmod(0o644)
    os.replace(temporary, destination)
print(f'Installed {destination} (uses ~/.local/bin/wirescape)')
PY
