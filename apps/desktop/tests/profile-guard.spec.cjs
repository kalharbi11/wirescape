// SPDX-License-Identifier: GPL-3.0-only
// Portable startup regression for the real compiled Electron main-process code.
// The VM stops before window/engine creation: no device, display or Pi required.
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');

const compiled = path.resolve(__dirname, '../build/main.js');

function startup(args) {
  const source = fs.readFileSync(compiled, 'utf8');
  const end = source.indexOf('let window = null;');
  if (end < 0) throw new Error('Compiled desktop startup boundary changed');
  const paths = {};
  const fakeElectron = {
    protocol: { registerSchemesAsPrivileged() {} },
    app: { setPath(name, location) { paths[name] = location; } }
  };
  const context = {
    exports: {}, __dirname: path.dirname(compiled), console,
    process: { argv: ['electron', compiled, ...args] },
    require(id) { return id === 'electron' ? fakeElectron : require(id); }
  };
  vm.runInNewContext(source.slice(0, end) + '\nglobalThis.capturedEngineArgs = engineArgs;',
    context, { timeout: 3000, filename: compiled });
  return { paths, engineArgs: Array.from(context.capturedEngineArgs) };
}

test('default launch leaves existing app paths and native settings untouched', () => {
  const result = startup(['--null-audio']);
  expect(result.paths).toEqual({});
  expect(result.engineArgs).toEqual(['--null-audio']);
});

test('isolated profile sets both Electron paths and the native settings argument', () => {
  const base = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape pi profile '));
  try {
    const profile = path.join(base, 'profile');
    const result = startup(['--profile-dir', profile, '--null-audio']);
    expect(result.paths).toEqual({
      userData: path.join(profile, 'userData'),
      sessionData: path.join(profile, 'sessionData')
    });
    expect(result.engineArgs).toEqual(['--null-audio', '--settings',
      path.join(profile, 'native-settings.json')]);
    expect(fs.statSync(result.paths.userData).isDirectory()).toBe(true);
    expect(fs.statSync(result.paths.sessionData).isDirectory()).toBe(true);
    const nested = path.join(profile, 'nested', 'owner.json');
    const explicit = startup(['--profile-dir', profile, '--settings', nested]);
    expect(explicit.engineArgs).toEqual(['--settings', nested]);
  } finally {
    fs.rmSync(base, { recursive: true, force: true });
  }
});

test('duplicate, outside, relative or missing settings never reach the engine', () => {
  const base = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape pi flags '));
  try {
    const profile = path.join(base, 'profile');
    const inside = path.join(profile, 'native.json');
    const outside = path.join(base, 'old-owner.json');
    expect(() => startup(['--profile-dir', profile,
      '--settings', inside, '--settings', outside])).toThrow(/Duplicate --settings/);
    expect(() => startup(['--profile-dir', profile, '--settings', outside]))
      .toThrow(/must stay within/);
    expect(() => startup(['--profile-dir', profile, '--settings', 'settings.json']))
      .toThrow(/absolute file path/);
    expect(() => startup(['--profile-dir', profile, '--settings']))
      .toThrow(/absolute file path/);
    expect(() => startup(['--profile-dir', profile, '--profile-dir', profile]))
      .toThrow(/only once/);
    expect(fs.existsSync(outside)).toBe(false);
  } finally {
    fs.rmSync(base, { recursive: true, force: true });
  }
});

test('symlinked settings parents and files cannot escape the isolated profile', () => {
  const base = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape pi links '));
  try {
    const profile = path.join(base, 'profile');
    const outside = path.join(base, 'owner-old-data');
    fs.mkdirSync(profile);
    fs.mkdirSync(outside);
    const link = path.join(profile, 'redirect');
    try {
      fs.symlinkSync(outside, link, process.platform === 'win32' ? 'junction' : 'dir');
    } catch (err) {
      if (['EPERM', 'EACCES', 'ENOTSUP'].includes(err.code)) {
        test.skip(true, 'Host does not permit creating temporary symlinks');
        return;
      }
      throw err;
    }
    expect(() => startup(['--profile-dir', profile, '--settings',
      path.join(link, 'new', 'native.json')])).toThrow(/parent escapes/);
    const existing = path.join(outside, 'old.json');
    fs.writeFileSync(existing, 'preserve me');
    const leaf = path.join(profile, 'native.json');
    fs.symlinkSync(existing, leaf, 'file');
    expect(() => startup(['--profile-dir', profile, '--settings', leaf]))
      .toThrow(/must not be a symlink/);
    expect(fs.readFileSync(existing, 'utf8')).toBe('preserve me');
  } finally {
    fs.rmSync(base, { recursive: true, force: true });
  }
});
