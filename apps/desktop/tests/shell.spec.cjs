// SPDX-License-Identifier: GPL-3.0-only
const { test, expect, _electron: electron } = require('../../studio/node_modules/@playwright/test');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const electronExecutable = require('electron');
const appMain = path.resolve(__dirname, '../build/main.js');
const stub = path.resolve(__dirname, 'stub-engine.cjs');

async function launch(extraEnv = {}) {
  return electron.launch({
    executablePath: electronExecutable,
    args: ['--no-sandbox', appMain, '--engine', stub, '--null-audio'],
    env: { ...process.env, WIRESCAPE_TEST_STUB: '1', ...extraEnv }
  });
}
test('native window loads the real built UI with the isolated five-member bridge', async () => {
  const app = await launch();
  try {
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    expect(await page.evaluate(() => Object.keys(window.wirescape).sort()))
      .toEqual(['onMessage', 'openPatch', 'platform', 'savePatch', 'send']);
    const flags = await app.evaluate(({ BrowserWindow }) => {
      const w = BrowserWindow.getAllWindows()[0];
      return [w.isMinimizable(), w.isClosable()];
    });
    expect(flags).toEqual([true, true]);
    expect(await page.evaluate(() => typeof require)).toBe('undefined');
    expect(await page.evaluate(() => typeof process)).toBe('undefined');
  } finally { await app.close(); }
});
test('NDJSON replies, unexpected crash notification and bounded automatic restart', async () => {
  const app = await launch({ STUB_CRASH_ON_PANIC: '1' });
  try {
    const page = await app.firstWindow();
    const messages = await page.evaluate(async () => {
      return new Promise((resolve, reject) => {
        const seen = [];
        const timer = setTimeout(() => reject(new Error('Missing restart sequence')), 15000);
        const off = window.wirescape.onMessage(msg => {
          seen.push(msg);
          const hellos = seen.filter(v => v.type === 'hello');
          if (hellos.length >= 1 && !seen.some(v => v.re === 'p1')) {
            window.wirescape.send({ v: 0, id: 'p1', type: 'listAudioDevices' });
          }
          if (seen.some(v => v.re === 'p1') && !seen.some(v => v.type === 'app.engineExited')) {
            window.wirescape.send({ v: 0, id: 'crash1', type: 'panic' });
          }
          if (hellos.length >= 2 && seen.some(v => v.type === 'app.engineExited')) {
            clearTimeout(timer); off(); resolve(seen);
          }
        });
      });
    });
    expect(messages.some(m => m.re === 'p1' && m.ok === true)).toBe(true);
    expect(messages.some(m => m.type === 'app.engineExited' && m.expected === false)).toBe(true);
    const hellos = messages.filter(m => m.type === 'hello');
    expect(hellos[0].session).not.toBe(hellos[1].session);
  } finally { await app.close(); }
});
test('native File intents and open/save round-trip never expose a filesystem path', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-shell-'));
  const file = path.join(dir, 'patch.json');
  const app = await launch();
  try {
    const page = await app.firstWindow();
    await app.evaluate(({ dialog }, filepath) => {
      dialog.showSaveDialog = async () => ({ canceled: false, filePath: filepath });
      dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [filepath] });
    }, file);
    const saved = await page.evaluate(() => window.wirescape.savePatch('{"unknown":true}\n', { saveAs: true }));
    expect(saved).toEqual({ name: 'patch.json' });
    expect(fs.readFileSync(file, 'utf8')).toBe('{"unknown":true}\n');
    expect(await page.evaluate(() => window.wirescape.openPatch()))
      .toEqual({ text: '{"unknown":true}\n', name: 'patch.json' });
    await page.evaluate(() => {
      window.__intents = [];
      window.wirescape.onMessage(message => window.__intents.push(message));
    });
    await app.evaluate(({ Menu }) => {
      Menu.getApplicationMenu().getMenuItemById('open-patch').click();
      Menu.getApplicationMenu().getMenuItemById('save-patch').click();
      Menu.getApplicationMenu().getMenuItemById('save-as').click();
    });
    await expect.poll(() => page.evaluate(() => window.__intents.filter(v => v.type.startsWith('app.menu'))))
      .toEqual([
        { v: 0, type: 'app.menuOpenPatch' },
        { v: 0, type: 'app.menuSavePatch', saveAs: false },
        { v: 0, type: 'app.menuSavePatch', saveAs: true }
      ]);
  } finally {
    await app.close();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
test('Escape exits F11 fullscreen and Quit closes engine stdin', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-eof-'));
  const marker = path.join(dir, 'shutdown');
  const app = await launch({ STUB_EXIT_MARKER: marker });
  try {
    const page = await app.firstWindow();
    await app.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows()[0].setFullScreen(true));
    await expect.poll(() => app.evaluate(({ BrowserWindow }) =>
      BrowserWindow.getAllWindows()[0].isFullScreen())).toBe(true);
    await page.keyboard.press('Escape');
    await expect.poll(() => app.evaluate(({ BrowserWindow }) =>
      BrowserWindow.getAllWindows()[0].isFullScreen())).toBe(false);
    await app.evaluate(({ app }) => app.quit());
    await expect.poll(() => fs.existsSync(marker)).toBe(true);
  } finally {
    await app.close().catch(() => {});
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
