// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const patch = (config?: unknown) => ({
  format: 'wirescape.native.patch' as const, version: 2 as const,
  ownerExtension: { survive: 1 },
  modules: [{ id: 1, module: 'osc.in', ownerNote: 'keep me', ...(config === undefined ? {} : { config }) }],
  connections: [],
  ui: { positions: { '1': [48, 30] as [number, number] }, view: { x: 0, y: 0, zoom: 1 } },
});

async function open(page: Page): Promise<void> {
  await page.getByTestId('osc-edit-1').click();
  await expect(page.getByTestId('osc-editor')).toBeVisible();
}

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
});

test('OSC In card shows actual route state, received counter and activity', async ({ page }, testInfo) => {
  await page.evaluate(() => { window.__wirescapeTest.engine.debug.oscTraffic = false; });
  await expect(page.getByTestId('osc-status-1')).toHaveText('Listening on 127.0.0.1:9000 · 8 of 8 rows active');
  await expect(page.getByTestId('osc-received-1')).toHaveText('Received: 0');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscReceived(1, 8));
  await expect(page.getByTestId('osc-received-1')).toHaveText('Received: 8');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscRouteState(1, 'port-busy'));
  await expect(page.getByTestId('osc-status-1')).toContainText('Port busy');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscRouteState(1, 'lan-not-confirmed'));
  await expect(page.getByTestId('osc-status-1')).toContainText('LAN not confirmed');
  await page.getByTestId('module-1').screenshot({ path: testInfo.outputPath('osc-in-card.png') });
  await testInfo.attach('OSC In card', { path: testInfo.outputPath('osc-in-card.png'), contentType: 'image/png' });
});

test('editor round-trips eight rows with stable module ID and patch extras', async ({ page }, testInfo) => {
  await open(page);
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/pico1/knob/8');
  await page.getByTestId('osc-editor').locator('.modal').screenshot({
    path: testInfo.outputPath('osc-in-editor.png'),
  });
  await testInfo.attach('OSC In editor', { path: testInfo.outputPath('osc-in-editor.png'), contentType: 'image/png' });
  await page.getByTestId('osc-row1-pattern').fill('/show/voice/*');
  await page.getByTestId('osc-row1-kind').selectOption('gate');
  await page.getByTestId('osc-row1-argument').fill('1');
  await page.getByTestId('osc-row1-input_max').fill('2');
  await page.getByTestId('osc-editor-port').fill('9001');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.modules[0]).toMatchObject({ id: 1, ownerNote: 'keep me' });
  expect(saved.ownerExtension).toEqual({ survive: 1 });
  expect(saved.modules[0].config).toMatchObject({ port: 9001, lan: false });
  expect(saved.modules[0].config.rows).toHaveLength(8);
  expect(saved.modules[0].config.rows[0]).toMatchObject({
    pattern: '/show/voice/*', kind: 'gate', argument: 1, input_max: 2,
  });
  await open(page);
  await expect(page.getByTestId('osc-row1-pattern')).toHaveValue('/show/voice/*');
  await expect(page.getByTestId('osc-editor-port')).toHaveValue('9001');
});

test('invalid pattern, range and port are refused without modifying authored config', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-row1-pattern').fill('bad/without/slash');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('address must start');
  expect((await state(page)).modules[0].config).toBeUndefined();
  await page.getByTestId('osc-row1-pattern').fill('/valid');
  await page.getByTestId('osc-row1-input_min').fill('5');
  await page.getByTestId('osc-row1-input_max').fill('4');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('input max');
  await page.getByTestId('osc-row1-input_max').fill('7');
  await page.getByTestId('osc-editor-port').fill('99');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('UDP port');
  expect((await state(page)).modules[0].config).toBeUndefined();
});

test('presets fill all rows, while config unknowns only drop on editor save', async ({ page }) => {
  const original = patch({ port: 9011, futureOption: 123, rows: [{ pattern: '/old', futureField: true }] });
  await page.evaluate(p => window.__wirescapeTest.load(p), original);
  await settle(page);
  expect((await state(page)).modules[0].config.futureOption).toBe(123);
  await open(page);
  await expect(page.getByTestId('osc-unknown-warning')).toContainText('ignored by this engine: futureOption, rows[0].futureField');
  expect((await state(page)).modules[0].config.futureOption).toBe(123);
  await page.getByTestId('osc-preset-touchosc').click();
  await expect(page.getByTestId('osc-row1-pattern')).toHaveValue('/1/fader1');
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/1/fader8');
  await page.getByTestId('osc-preset-reset').click();
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/pico1/knob/8');
  await page.getByTestId('osc-preset-touchosc').click();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.modules[0].config.futureOption).toBeUndefined();
  expect(saved.modules[0].config.rows[0].futureField).toBeUndefined();
  expect(saved.modules[0].config.rows[7].pattern).toBe('/1/fader8');
  expect(saved.modules[0].ownerNote).toBe('keep me');
  expect(saved.ownerExtension).toEqual({ survive: 1 });
});

test('authored OSC config version is preserved and unsupported versions cannot be saved', async ({ page }) => {
  const authored = patch({ version: 2, port: 9011, futureOption: true, rows: [{ enabled: false }] });
  await page.evaluate(p => window.__wirescapeTest.load(p), authored);
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-unknown-warning')).toContainText('Unsupported OSC config version');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('Unsupported OSC config version');
  expect((await state(page)).modules[0].config).toMatchObject(authored.modules[0].config as Record<string, unknown>);
  await page.getByTestId('osc-editor').getByRole('button', { name: 'Cancel' }).click();
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await settle(page);
  expect((await state(page)).modules[0].config).toMatchObject(authored.modules[0].config as Record<string, unknown>);

  // Explicit v1 survives a genuine editor Save; omitted v1 stays omitted.
  await page.evaluate(p => window.__wirescapeTest.load(p), patch({ version: 1, port: 9011 }));
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-unknown-warning')).toBeHidden();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  expect((await state(page)).modules[0].config.version).toBe(1);
  await page.evaluate(p => window.__wirescapeTest.load(p), patch({ port: 9011 }));
  await settle(page);
  await open(page);
  await page.getByTestId('osc-editor-save').click();
  expect((await state(page)).modules[0].config.version).toBeUndefined();
});

test('Reset to Pico defaults restores UDP port and clears LAN request before save', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-editor-port').fill('9123');
  await page.getByTestId('osc-editor-lan').check();
  await page.getByTestId('osc-row1-pattern').fill('/changed');
  await page.getByTestId('osc-preset-reset').click();
  await expect(page.getByTestId('osc-editor-port')).toHaveValue('9000');
  await expect(page.getByTestId('osc-editor-lan')).not.toBeChecked();
  await expect(page.getByTestId('osc-row1-pattern')).toHaveValue('/pico1/knob/1');
  await expect(page.getByTestId('osc-send-example')).toContainText('127.0.0.1:9000');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.modules[0].config).toMatchObject({ port: 9000, lan: false });
  expect(saved.modules[0].config.rows[0].pattern).toBe('/pico1/knob/1');
});

test('copyable OSC example follows TouchOSC, custom rows and concrete wildcard addresses', async ({ page }) => {
  await open(page);
  await page.evaluate(() => {
    Object.defineProperty(navigator, 'clipboard', { configurable: true, value: {
      writeText: async (value: string) => {
        (window as Window & { __oscCopied?: string }).__oscCopied = value;
      },
    } });
  });
  await page.getByTestId('osc-preset-touchosc').click();
  await expect(page.getByTestId('osc-send-example')).toContainText('send /1/fader1 0.5');
  await page.getByTestId('osc-copy-example').click();
  expect(await page.evaluate(() => (window as Window & { __oscCopied?: string }).__oscCopied))
    .toContain('send /1/fader1 0.5');

  await page.getByTestId('osc-row1-pattern').fill('/show/voice/7');
  await expect(page.getByTestId('osc-send-example')).toContainText('send /show/voice/7 0.5');
  await page.getByTestId('osc-row1-pattern').fill('/show/voice/*');
  await page.getByTestId('osc-editor-port').fill('9012');
  await expect(page.getByTestId('osc-send-example'))
    .toContainText('send /show/voice/1 0.5 to 127.0.0.1:9012');
  await page.getByTestId('osc-copy-example').click();
  expect(await page.evaluate(() => (window as Window & { __oscCopied?: string }).__oscCopied))
    .toContain('send /show/voice/1 0.5 to 127.0.0.1:9012');
  await page.getByTestId('osc-editor-save').click();
  expect((await state(page)).modules[0].config.rows[0].pattern).toBe('/show/voice/*');
});

test('LAN checkbox requests a route but cannot authorize a LAN socket', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-editor-lan').check();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-status-1')).toContainText('LAN not confirmed');
  expect((await state(page)).modules[0].config.lan).toBe(true);
});

test('row toggles round-trip, Clear disables all rows and old patches open with all rows on (#612)', async ({ page }) => {
  // An old patch without `enabled` keeps every row on.
  await page.evaluate(p => window.__wirescapeTest.load(p), patch({ rows: [{ pattern: '/legacy/1' }] }));
  await settle(page);
  await expect(page.getByTestId('osc-status-1')).toContainText('8 of 8 rows active');
  await open(page);
  for (let i = 1; i <= 8; i++) await expect(page.getByTestId(`osc-row${i}-enabled`)).toBeChecked();
  await expect(page.getByTestId('osc-editor-active')).toHaveText('8 of 8 rows active');
  await page.getByTestId('osc-row3-enabled').uncheck();
  await expect(page.getByTestId('osc-editor-active')).toHaveText('7 of 8 rows active');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  let saved = await state(page);
  const rows = saved.modules[0].config.rows as Record<string, unknown>[];
  // Writers emit `enabled` only when false.
  expect(rows[2]).toMatchObject({ pattern: '/pico1/knob/3', enabled: false });
  expect(rows.filter(r => 'enabled' in r)).toHaveLength(1);
  expect(rows[0].pattern).toBe('/legacy/1');
  await expect(page.getByTestId('osc-status-1')).toHaveText('Listening on 127.0.0.1:9000 · 7 of 8 rows active');

  // Clear keeps addresses; the engine then reports the module as not listening.
  await open(page);
  await expect(page.getByTestId('osc-row3-enabled')).not.toBeChecked();
  await page.getByTestId('osc-preset-clear').click();
  await expect(page.getByTestId('osc-editor-active')).toHaveText('0 of 8 rows active');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  saved = await state(page);
  const cleared = saved.modules[0].config.rows as Record<string, unknown>[];
  expect(cleared.every(r => r.enabled === false)).toBe(true);
  expect(cleared[0].pattern).toBe('/legacy/1');
  expect(cleared[7].pattern).toBe('/pico1/knob/8');
  await expect(page.getByTestId('osc-status-1')).toHaveText('All rows off · not listening');

  // Pico preset switches every row back on.
  await open(page);
  await page.getByTestId('osc-preset-pico').click();
  await expect(page.getByTestId('osc-editor-active')).toHaveText('8 of 8 rows active');
  await page.getByTestId('osc-editor-save').click();
  saved = await state(page);
  expect((saved.modules[0].config.rows as Record<string, unknown>[]).some(r => 'enabled' in r)).toBe(false);
});

test('unknown row keys on a disabled row survive unrelated edits and drop only on editor save (#612)', async ({ page }) => {
  await page.evaluate(p => window.__wirescapeTest.load(p), patch({
    rows: [{ pattern: '/a' }, { pattern: '/b', enabled: false, futureRowKey: 'x' }],
  }));
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-unknown-warning')).toContainText('ignored by this engine: rows[1].futureRowKey');
  await expect(page.getByTestId('osc-row2-enabled')).not.toBeChecked();
  await page.getByTestId('osc-editor').getByRole('button', { name: 'Cancel' }).click();
  // An unrelated document edit keeps the row's unknown key and its enabled flag.
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await settle(page);
  let row = (await state(page)).modules[0].config.rows[1];
  expect(row).toMatchObject({ pattern: '/b', enabled: false, futureRowKey: 'x' });
  await open(page);
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  row = (await state(page)).modules[0].config.rows[1];
  expect(row).toMatchObject({ pattern: '/b', enabled: false });
  expect(row.futureRowKey).toBeUndefined();
});
