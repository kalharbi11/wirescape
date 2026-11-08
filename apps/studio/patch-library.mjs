// SPDX-License-Identifier: GPL-3.0-only
import { readdirSync, readFileSync, lstatSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const defaultDirectory = fileURLToPath(new URL('./patches/', import.meta.url));
export function patchIndex(directory = defaultDirectory) {
  const names = readdirSync(directory).filter(name => name.endsWith('.json')).sort();
  if (names.length > 128) throw new Error('Shared library exceeds 128 patches');
  let total = 0;
  const patches = names.map(filename => {
    if (filename.length > 120 || !/^[a-z0-9][a-z0-9-]*\.json$/.test(filename)) throw new Error('Use lowercase-hyphen patch filenames');
    if (filename === 'keyboard-voice.json' || filename.startsWith('qt-')) throw new Error('Reserved built-in patch ID');
    const path = directory + '/' + filename;
    const stat = lstatSync(path);
    if (!stat.isFile() || stat.isSymbolicLink() || stat.size > 2 * 1024 * 1024)
      throw new Error('Shared patches must be regular JSON files <=2 MiB');
    total += stat.size;
    const patch = JSON.parse(readFileSync(path, 'utf8'));
    if (patch && 'name' in patch && (typeof patch.name !== 'string' || !patch.name.length || patch.name.length > 240))
      throw new Error('Patch name must be 1–240 characters: ' + filename);
    if (patch?.format !== 'wirescape.native.patch' || patch.version !== 2 ||
        !Array.isArray(patch.modules) || !Array.isArray(patch.connections) ||
        !Array.isArray(patch.outputs) || patch.modules.length > 32 ||
        !patch.modules.every(m => m && typeof m.module === 'string'))
      throw new Error('Invalid shared patch: ' + filename);
    return { id: filename.slice(0, -5), name: typeof patch.name === 'string' ? patch.name : filename, patch };
  });
  if (total > 8 * 1024 * 1024) throw new Error('Shared library exceeds 8 MiB');
  const source = JSON.stringify({ patches });
  if (Buffer.byteLength(source) > 8 * 1024 * 1024) throw new Error('Shared index exceeds 8 MiB');
  return source;
}
export function patchLibraryPlugin() {
  return {
    name: 'wirescape-shared-patches',
    generateBundle() {
      this.emitFile({ type: 'asset', fileName: 'patches/index.json', source: patchIndex() });
    },
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url?.split('?')[0]?.endsWith('/patches/index.json')) {
          try {
            res.setHeader('Content-Type', 'application/json');
            res.end(patchIndex());
          } catch (error) { res.statusCode = 500; res.end(String(error)); }
        } else next();
      });
    },
  };
}
