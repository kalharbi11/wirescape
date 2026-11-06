// SPDX-License-Identifier: GPL-3.0-only
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
const path = require('node:path');
const electronExecutable = require('electron');
const appMain = path.resolve(__dirname, '../build/main.js');
const stub = path.resolve(__dirname, 'stub-engine.cjs');

test('stderr reports elapsed time to first hello and running audio status', async () => {
  const app = await electron.launch({
    executablePath: electronExecutable,
    args: ['--no-sandbox', appMain, '--engine', stub, '--null-audio'],
    env: { ...process.env, WIRESCAPE_TEST_STUB: '1', STUB_STARTUP_DELAY_MS: '1500', STUB_AUDIO_RUNNING: '1' }
  });
  try {
    const stderr = [];
    app.process().stderr.on('data', chunk => stderr.push(chunk.toString('utf8')));
    await app.firstWindow();
    await expect.poll(() => stderr.join('')).toMatch(/boot-to-first-hello-ms=\d+/);
    await expect.poll(() => stderr.join('')).toMatch(/boot-to-audio-running-ms=\d+/);
  } finally { await app.close(); }
});
