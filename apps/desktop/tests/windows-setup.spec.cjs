// SPDX-License-Identifier: GPL-3.0-only
// W1: the actual Windows Electron/preload/sidecar path, never device authorization.
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
const { execFileSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

test.skip(process.platform !== 'win32', 'Executed by Windows W1 CI; not a Linux substitute');
test('Windows W1: isolated UI, correlated native roundtrip and graceful shutdown', async () => {
  const root = process.env.WIRESCAPE_SETUP_ROOT || path.resolve(__dirname, '../../..');
  const engine = process.env.WIRESCAPE_REAL_ENGINE;
  expect(engine && fs.statSync(engine).isFile()).toBeTruthy();
  const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-w1-'));
  const wrapper = path.join(scratch, 'main.cjs');
  fs.writeFileSync(wrapper, `const { app } = require('electron');
const fs = require('node:fs'), path = require('node:path');
for (const [key, suffix] of [['userData','user'],['sessionData','session']]) {
  const dir = path.join(${JSON.stringify(scratch)}, suffix);
  fs.mkdirSync(dir, {recursive:true}); app.setPath(key, dir);
}
require(${JSON.stringify(path.join(root, 'apps/desktop/build/main.js'))});\n`);
  const environment = { ...process.env, WIRESCAPE_TEST_STUB: '0' };
  delete environment.ELECTRON_RUN_AS_NODE;
  let app, nativePid;
  try {
    app = await electron.launch({ executablePath: require('electron'),
      args: [wrapper, '--engine', engine, '--null-audio', '--settings', path.join(scratch, 'settings.json')],
      cwd: root, env: environment });
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
    const appPaths = await app.evaluate(({ app }) => [app.getPath('userData'), app.getPath('sessionData')]);
    expect(appPaths).toEqual([path.join(scratch, 'user'), path.join(scratch, 'session')]);
    const children = execFileSync('powershell.exe', ['-NoProfile', '-Command',
      `Get-CimInstance Win32_Process -Filter 'ParentProcessId = ${app.process().pid}' | Where-Object { $_.ExecutablePath -eq $env:WIRESCAPE_REAL_ENGINE } | Select-Object -ExpandProperty ProcessId`],
      { encoding: 'utf8', timeout: 10000, env: { ...process.env, WIRESCAPE_REAL_ENGINE: engine } }).trim().split(/\s+/).filter(Boolean);
    expect(children).toHaveLength(1); nativePid = Number(children[0]);
    await page.evaluate(() => window.__wirescapeTest.load({ format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 1, module: 'essentials.output' }], connections: [],
      outputs: [{ module: 1, port: 'audio', bus_channel: 0 }], w1_unknown: { retained: true } }, 'W1 roundtrip'));
    const reply = await page.evaluate(() => new Promise((resolve, reject) => {
      const id = 'w1-audio-query';
      const timer = setTimeout(() => { off(); reject(new Error('Native roundtrip timed out')); }, 8000);
      const off = window.wirescape.onMessage(message => {
        if (message.re !== id) return;
        clearTimeout(timer); off(); resolve(message);
      });
      window.wirescape.send({ v: 0, id, type: 'listAudioDevices' });
    }));
    expect(reply.re).toBe('w1-audio-query'); expect(reply.ok).toBe(true);
    expect(Number(reply.gen)).toBeGreaterThan(0);
    expect(await page.evaluate(() => window.__wirescapeTest.state().w1_unknown)).toEqual({ retained: true });
    const electronProcess = app.process();
    await app.close(); app = null;
    expect(electronProcess.exitCode).toBe(0);
    execFileSync('powershell.exe', ['-NoProfile', '-Command',
      `if (Get-Process -Id ${nativePid} -ErrorAction SilentlyContinue) { throw 'Native child survived graceful shutdown' }`],
      { timeout: 10000 });
  } finally {
    if (app) await app.close();
    fs.rmSync(scratch, { recursive: true, force: true });
  }
});
