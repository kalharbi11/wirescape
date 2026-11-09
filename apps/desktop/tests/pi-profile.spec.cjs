// SPDX-License-Identifier: GPL-3.0-only
// Pi-only qualification in the existing Electron/Playwright real-sidecar suite.
// No physical devices or network; #386 owns semantic edit/reconnect cases.
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
const fs = require('node:fs');
const path = require('node:path');
const electronPath = require('electron');
const appMain = path.resolve(__dirname, '../build/main.js');

test.skip(process.platform !== 'linux' || process.env.WIRESCAPE_PI_APP_SMOKE !== '1',
  'Pi ARM64 qualification is executed by tools/pi/app_smoke.py, not hosted x86');
test('Pi ARM64: isolated Electron profile, real native round trip and clean shutdown', async () => {
  test.setTimeout(120000);
  const profile = process.env.WIRESCAPE_PI_PROFILE_DIR;
  const executable = process.env.WIRESCAPE_REAL_ENGINE;
  const backend = process.env.WIRESCAPE_PI_DISPLAY_BACKEND;
  expect(profile && path.isAbsolute(profile)).toBeTruthy();
  expect(executable && path.isAbsolute(executable)).toBeTruthy();
  expect(['x11', 'wayland']).toContain(backend);

  const instance = await electron.launch({
    executablePath: electronPath,
    args: ['--no-sandbox', '--ozone-platform=' + backend,
      appMain, '--engine', executable, '--null-audio', '--profile-dir', profile],
    env: { ...process.env, WIRESCAPE_TEST_STUB: '0' }
  });
  let enginePid;
  try {
    const page = await instance.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');

    const identity = await instance.evaluate(({ app }) => ({
      platform: process.platform, arch: process.arch,
      userData: app.getPath('userData'), sessionData: app.getPath('sessionData'),
      ozone: app.commandLine.getSwitchValue('ozone-platform')
    }));
    expect(identity.platform).toBe('linux');
    expect(identity.arch).toBe('arm64');
    expect(identity.userData).toBe(path.join(profile, 'userData'));
    expect(identity.sessionData).toBe(path.join(profile, 'sessionData'));
    expect(identity.ozone).toBe(backend);

    const hello = await page.evaluate(() => new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Missing native hello')), 10000);
      const off = window.wirescape.onMessage(m => {
        if (m.type !== 'hello') return;
        clearTimeout(timer); off(); resolve(m);
      });
    }));
    expect(hello.protocol).toBe(0);
    expect(hello.catalogue.modules.map(m => m.id)).toContain('essentials.vcf');

    const parent = instance.process().pid;
    const children = fs.readFileSync('/proc/' + parent + '/task/' + parent + '/children', 'utf8')
      .trim().split(/\s+/).filter(Boolean);
    const native = children.map(pid => {
      try {
        const argv = fs.readFileSync('/proc/' + pid + '/cmdline', 'utf8').split('\0').filter(Boolean);
        return { pid: Number(pid), argv };
      } catch { return null; }
    }).find(child => child?.argv[0] === executable);
    expect(native).toBeTruthy();
    enginePid = native.pid;
    expect(native.argv).toContain('--null-audio');
    expect(native.argv).not.toContain('--confirm-audio');
    expect(native.argv).not.toContain('--confirm-midi');
    const settingsIndex = native.argv.indexOf('--settings');
    expect(settingsIndex).toBeGreaterThan(0);
    expect(native.argv[settingsIndex + 1]).toBe(path.join(profile, 'native-settings.json'));

    const roundtrip = await page.evaluate(() => new Promise((resolve, reject) => {
      const timer = setTimeout(() => { off(); reject(new Error('Native command timed out')); }, 10000);
      const off = window.wirescape.onMessage(m => {
        if (m.re !== 'pi-panic-1') return;
        clearTimeout(timer); off(); resolve(m);
      });
      window.wirescape.send({ v: 0, id: 'pi-panic-1', type: 'panic' });
    }));
    expect(roundtrip.ok).toBe(true);

    // Main-process library and dialog paths must be underneath the new profile.
    const listed = await page.evaluate(() => window.wirescape.openPatch({ library: 'list' }));
    expect(JSON.parse(listed.text)).toEqual([]);
    expect(fs.statSync(path.join(profile, 'userData', 'patches')).isDirectory()).toBe(true);
    await instance.evaluate(({ dialog }) => {
      dialog.showSaveDialog = async (_win, options) => {
        globalThis.__piSaveDefault = options.defaultPath;
        return { canceled: false, filePath: options.defaultPath };
      };
    });
    const saved = await page.evaluate(() => window.wirescape.savePatch('{"pi":"isolated"}', { saveAs: true }));
    expect(saved.name).toBe('Untitled.json');
    const defaultPath = await instance.evaluate(() => globalThis.__piSaveDefault);
    expect(defaultPath).toBe(path.join(profile, 'userData', 'patches', 'Untitled.json'));
    expect(fs.readFileSync(defaultPath, 'utf8')).toBe('{"pi":"isolated"}');
    await instance.evaluate(({ BrowserWindow }) => {
      BrowserWindow.getAllWindows()[0].setBounds({ x: 0, y: 0, width: 900, height: 680 });
    });
    console.log('Pi functional display backend=' + identity.ozone);
  } finally {
    await instance.close().catch(() => {});
  }
  expect(fs.existsSync(path.join(profile, 'userData', 'window-state.json'))).toBe(true);
  if (enginePid) {
    await expect.poll(() => fs.existsSync('/proc/' + enginePid), { timeout: 8000 }).toBe(false);
  }
});
