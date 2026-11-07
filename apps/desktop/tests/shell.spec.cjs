// SPDX-License-Identifier: GPL-3.0-only
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
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
    expect(new Set(hellos.map(h => h.session)).size).toBe(hellos.length);
  } finally { await app.close(); }
});
test('four crashes within 30s stop at the failure dialog, even after renderer restart', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-crash-loop-'));
  const countFile = path.join(dir, 'spawns');
  const app = await launch({ STUB_CRASH_COUNT_FILE: countFile });
  try {
    const page = await app.firstWindow();
    await app.evaluate(({ dialog }) => {
      globalThis.__failureDialogCount = 0;
      dialog.showMessageBox = async () => {
        globalThis.__failureDialogCount++;
        return new Promise(() => {}); // Hold the dialog: no user-requested reset.
      };
    });
    await page.evaluate(() => {
      window.__unexpectedExits = 0;
      window.__rendererRestartSent = false;
      window.wirescape.onMessage(message => {
        if (message.type !== 'app.engineExited' || message.expected !== false) return;
        window.__unexpectedExits++;
        if (!window.__rendererRestartSent) {
          window.__rendererRestartSent = true;
          window.wirescape.send({ v: 0, id: 'renderer-restart', type: 'app.restartEngine' });
        }
      });
    });
    await expect.poll(() => page.evaluate(() => window.__rendererRestartSent)).toBe(true);
    await expect.poll(() => app.evaluate(() => globalThis.__failureDialogCount), { timeout: 15000 }).toBe(1);
    expect(await page.evaluate(() => window.__unexpectedExits)).toBe(4);
    expect(Number(fs.readFileSync(countFile, 'utf8'))).toBe(4);
    await page.waitForTimeout(650); // A fifth spawn would have appeared by now.
    expect(Number(fs.readFileSync(countFile, 'utf8'))).toBe(4);
  } finally {
    await app.close().catch(() => {});
    fs.rmSync(dir, { recursive: true, force: true });
  }
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
test('patch dialogs create the user patch folder lazily and retain the selected path', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-patches-'));
  const folder = path.join(dir, 'patches');
  const selected = path.join(dir, 'selected.json');
  const app = await launch();
  try {
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await app.evaluate(({ app, dialog }, directory) => {
      app.setPath('userData', directory);
      globalThis.__patchDefaults = [];
      dialog.showOpenDialog = async (_window, options) => {
        globalThis.__patchDefaults.push(options.defaultPath);
        return { canceled: true, filePaths: [] };
      };
      dialog.showSaveDialog = async (_window, options) => {
        globalThis.__patchDefaults.push(options.defaultPath);
        return { canceled: true };
      };
    }, dir);
    expect(fs.existsSync(folder)).toBe(false);
    expect(await page.evaluate(() => window.wirescape.openPatch())).toBeNull();
    expect(fs.statSync(folder).isDirectory()).toBe(true);
    const preserved = path.join(folder, 'existing.json');
    fs.writeFileSync(preserved, '{"unknown":"keep me"}\n');
    expect(await page.evaluate(() => window.wirescape.savePatch('{}', { saveAs: true }))).toBeNull();
    expect(await app.evaluate(() => globalThis.__patchDefaults))
      .toEqual([folder, path.join(folder, 'Untitled.json')]);
    expect(fs.readdirSync(folder)).toEqual(['existing.json']);

    await app.evaluate(({ dialog }, filename) => {
      dialog.showSaveDialog = async () => ({ canceled: false, filePath: filename });
    }, selected);
    expect(await page.evaluate(() => window.wirescape.savePatch('{"unknown":42}\n', { saveAs: true })))
      .toEqual({ name: 'selected.json' });
    expect(fs.readFileSync(selected, 'utf8')).toBe('{"unknown":42}\n');
    await app.evaluate(({ dialog }) => {
      dialog.showSaveDialog = async (_window, options) => {
        globalThis.__patchDefaults.push(options.defaultPath);
        return { canceled: true };
      };
    });
    expect(await page.evaluate(() => window.wirescape.openPatch())).toBeNull();
    expect(await page.evaluate(() => window.wirescape.savePatch('{}', { saveAs: true }))).toBeNull();
    expect(await app.evaluate(() => globalThis.__patchDefaults.slice(-2))).toEqual([selected, selected]);
    // Cancelled Open/Save As must not redirect a subsequent plain Save.
    await page.evaluate(() => window.wirescape.savePatch('{"unknown":43}\n'));
    expect(fs.readFileSync(selected, 'utf8')).toBe('{"unknown":43}\n');
    expect(fs.readFileSync(preserved, 'utf8')).toBe('{"unknown":"keep me"}\n');
  } finally {
    await app.close();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
test('an unavailable patch folder rejects file operations while the window remains usable', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-patch-error-'));
  fs.writeFileSync(path.join(dir, 'patches'), 'existing file');
  const app = await launch();
  try {
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await app.evaluate(({ app, dialog }, directory) => {
      app.setPath('userData', directory);
      dialog.showOpenDialog = dialog.showSaveDialog = async () => {
        throw new Error('Dialog must not open without a patch folder');
      };
    }, dir);
    await expect(page.evaluate(() => window.wirescape.openPatch())).rejects.toThrow('Could not create patch folder');
    await expect(page.evaluate(() => window.wirescape.savePatch('{}'))).rejects.toThrow('Could not create patch folder');
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    expect(await page.evaluate(() => Object.keys(window.wirescape).sort()))
      .toEqual(['onMessage', 'openPatch', 'platform', 'savePatch', 'send']);
    expect(fs.readFileSync(path.join(dir, 'patches'), 'utf8')).toBe('existing file');
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

test('patch library keeps paths private, bounds reads and detaches bundled save targets', async () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-library-'));
  const patches = path.join(root, 'patches');
  fs.mkdirSync(patches);
  fs.writeFileSync(path.join(patches, 'Song.json'), '{"future":42}');
  fs.writeFileSync(path.join(patches, 'Huge.json'), 'x'.repeat(2097153));
  fs.writeFileSync(path.join(patches, 'Bad.json'), Buffer.from([255]));
  let app;
  try {
    app = await launch();
    // Playwright's Electron main-process evaluate runs without CommonJS require.
    // Create fixtures in this Node test process and pass only serializable paths.
    await app.evaluate(({ app }, directory) => app.setPath('userData', directory), root);
    const page = await app.firstWindow();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    const listed = await page.evaluate(() => window.wirescape.openPatch({ library: 'list' }));
    expect(JSON.parse(listed.text)).toContain('Song.json');
    expect(listed.text).not.toContain(root);
    expect(await page.evaluate(() => window.wirescape.openPatch({ library: 'read', name: 'Song.json' })))
      .toEqual({ name: 'Song.json', text: '{"future":42}' });
    for (const name of ['../Song.json', 'Huge.json', 'Bad.json']) {
      expect(await page.evaluate(async name => {
        try { await window.wirescape.openPatch({ library: 'read', name }); return false; } catch { return true; }
      }, name)).toBe(true);
    }
    if (process.platform !== 'win32') {
      fs.writeFileSync(path.join(root, 'Outside.json'), '{}');
      fs.symlinkSync(path.join(root, 'Outside.json'), path.join(patches, 'Escape.json'));
      expect(await page.evaluate(async () => {
        try { await window.wirescape.openPatch({ library: 'read', name: 'Escape.json' }); return false; } catch { return true; }
      })).toBe(true);
    }
    await page.evaluate(() => window.wirescape.openPatch({ library: 'reset' }));
    const output = path.join(patches, 'Copy.json');
    const expectedDefault = path.join(patches, 'Untitled.json');
    await app.evaluate(({ dialog }, paths) => {
      dialog.showSaveDialog = async (_window, options) => {
        if (options.defaultPath !== paths.expectedDefault) throw new Error('Wrong default folder');
        return { canceled: false, filePath: paths.output };
      };
    }, { output, expectedDefault });
    await page.evaluate(() => window.wirescape.savePatch('{"copy":true}', { saveAs: true }));
    expect(fs.readFileSync(output, 'utf8')).toBe('{"copy":true}');
    expect(fs.readFileSync(path.join(patches, 'Song.json'), 'utf8')).toBe('{"future":42}');
  } finally {
    if (app) await app.close().catch(() => {});
    fs.rmSync(root, { recursive: true, force: true });
  }
});
