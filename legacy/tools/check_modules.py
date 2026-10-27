"""Diagnose Wirescape folder plugins without starting audio or WebSocket server.

Run from the Wirescape root: py -3.12 tools\\check_modules.py
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from wirescape import extensions
from wirescape.schema import DEFINITIONS


def main():
    print('WIRESCAPE / MODULE DIAGNOSTIC')
    print('Project:', ROOT)
    print('Modules folder:', extensions.MODULES)
    found = extensions.scan()
    print('Discovered folder modules:', ', '.join(found) or '(none)')
    print('Enabled:', ', '.join(sorted(extensions.read_enabled())) or '(none)')
    extensions.load_enabled()
    for module in extensions.catalog():
        print(f"  {module['id']:16s} installed=yes active={module['active']} enabled={module['enabled']} version={module['version']}")
        if module['error']:
            print('    ERROR:', module['error'])
    for name, err in extensions.FAILED.items():
        if name not in found:
            print(f'  {name}: ERROR {err}')
    source = (ROOT/'web/client.js').read_text(encoding='utf8')
    ui = (ROOT/'web/index.html').read_text(encoding='utf8')
    manager_ok = "panel.classList.toggle('visible',!!visible)" in source and 'id="moduleManager"' in ui
    print('Module manager visibility code:', 'OK' if manager_ok else 'OUTDATED')
    seq = found.get('sequencer')
    seq_ready = bool(seq and 'sequencer' in DEFINITIONS and manager_ok)
    print('CHORD / SEQUENCER:', 'READY' if seq_ready else 'NOT ACTIVE — open MODULES ↻ to enable')
    return 0 if seq_ready else 1


if __name__ == '__main__':
    raise SystemExit(main())
