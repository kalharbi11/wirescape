# SPDX-License-Identifier: GPL-3.0-only
"""Generate native identities plus UI presentation; refuse identity mismatches."""
import argparse
import copy
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def generate(root=ROOT):
    presentation = json.loads((root / 'apps/studio/public/catalogue-presentation.json').read_text())
    paths = sorted((root / 'native/modules2').glob('*/module.json'))
    paths += sorted((root / 'native/sdk/examples').glob('*/module.json'))
    manifests = {}
    for path in paths:
        manifest = json.loads(path.read_text())
        identity = manifest['id']
        if identity in manifests:
            raise ValueError(f'duplicate native module ID: {identity}')
        # Manifests contain ABI/schema versions, not the module's saved-data version.
        info = re.search(r'ws::Info\s+info\s*\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*(\d+)\s*}',
                         path.with_name('dsp.h').read_text())
        if not info or info[1] != identity or int(info[2]) < 1:
            raise ValueError(f'{path}: descriptor identity/version missing or mismatched')
        manifests[identity] = (manifest, int(info[2]))
    modules = presentation['modules']
    if len({m['id'] for m in modules}) != len(modules) or {m['id'] for m in modules} != set(manifests):
        raise ValueError('module ID mismatch between native manifests and presentation sidecar')
    result = []
    for original in modules:
        m = copy.deepcopy(original)
        native, version = manifests[m['id']]
        ports = {(p['direction'], p['id']) for p in m['ports']}
        expected = {(direction, key) for direction, field in [('input', 'inputs'), ('output', 'outputs')]
                    for key in native.get(field, {})}
        params = native.get('parameters', {})
        if (ports != expected or len(ports) != len(m['ports'])
                or {p['id'] for p in m['parameters']} != set(params)
                or len(m['parameters']) != len(params)):
            raise ValueError(f"{m['id']}: port/parameter ID mismatch; refusing to rename IDs")
        m['id'], m['name'] = native['id'], native['name']
        m['version'] = version
        m['legacy_parameters'] = params
        for port in m['ports']:
            port['kind'] = native['inputs' if port['direction'] == 'input' else 'outputs'][port['id']]
        result.append(m)
    return json.dumps({'note': presentation['note'], 'modules': result}, indent=1) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    target = ROOT / 'apps/studio/public/wirescape_modules.json'
    try:
        content = generate()
    except (ValueError, KeyError) as error:
        parser.exit(1, f'{error}\n')
    if args.check:
        if target.read_text() != content:
            parser.exit(1, 'UI catalogue drift; run python tools/gen_ui_catalogue.py\n')
        print('UI catalogue matches native manifests and presentation sidecar')
    else:
        target.write_text(content)


if __name__ == '__main__':
    main()
