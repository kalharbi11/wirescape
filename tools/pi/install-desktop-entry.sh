#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Owner-run, per-user installation; never sudo and never opens a device.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
python3 - "$root" <<'PY'
from pathlib import Path
import os
import shlex
import sys

root = Path(sys.argv[1])
desktop = root / 'apps/desktop'
electron = desktop / 'node_modules/.bin/electron'
main = desktop / 'build/main.js'
engine = root / 'build/native/wirescape-engine'
ui = root / 'apps/studio/dist/index.html'
for required in (electron, main, engine, ui):
    if not required.exists():
        raise SystemExit(f'Missing {required}: build the engine, npm ci and npm run build first')
home = Path.home()
launcher = home / '.local/bin/wirescape'
launcher.parent.mkdir(parents=True, exist_ok=True)
command = ' '.join(shlex.quote(str(p)) for p in (electron, main))
command += ' --engine ' + shlex.quote(str(engine))
command += ' --confirm-audio --confirm-midi "$@"'
launcher.write_text('#!/bin/sh\n# SPDX-License-Identifier: GPL-3.0-only\nexec ' + command + '\n')
launcher.chmod(0o755)
destination = home / '.local/share/applications/wirescape.desktop'
destination.parent.mkdir(parents=True, exist_ok=True)
quoted = str(launcher).replace('\\', '\\\\').replace('"', '\\"')
template = (root / 'tools/pi/wirescape.desktop.in').read_text()
destination.write_text(template.replace('@LAUNCHER@', quoted))
destination.chmod(0o644)
print(f'Installed {destination} (owner-run launch includes device confirmation flags)')
PY
