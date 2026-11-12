// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { STARTERS, DEFAULT_STARTER } from '../src/app/starters';
import { readFileSync } from 'node:fs';
import { boot, state } from './helpers';

const KEY = 'wirescape.autosave.v2';

async function reload(page: Page): Promise<void> {
  await page.reload();
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
}

function withoutView(patch: any) {
  const { ui, ...rest } = patch;
  const { view: _view, ...uiRest } = ui;
  return { ...rest, ui: uiRest };
}

test('switching between clean starters recovers the last selected one', async ({ page }) => {
  await boot(page, { blank: false });
  const [first, last] = STARTERS.filter(s => s.id === 'keyboard-voice' || s.id === 'midi-keys-and-knobs');
  await page.getByTestId('starter-select').selectOption(first!.id);
  await page.getByTestId('starter-select').selectOption(last!.id);
  const selected = await state(page);
  await expect(page.getByTestId('dirty-dot')).not.toHaveClass(/on/);

  await reload(page);
  await expect(page.getByTestId('patch-name')).toHaveText(last!.name);
  expect(withoutView(await state(page))).toEqual(withoutView(selected));
  await expect(page.getByTestId('dirty-dot')).not.toHaveClass(/on/);
});

test('pagehide flushes an edit before the four-second interval', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.doc.addModule('essentials.noise', {}, 30, 40));
  await expect(page.getByTestId('dirty-dot')).toHaveClass(/on/);
  await reload(page);
  expect((await state(page)).modules.some((m: any) => m.module === 'essentials.noise')).toBe(true);
});

test('NEW replaces the prior recovery with a clean empty patch', async ({ page }) => {
  await boot(page, { blank: false });
  expect((await state(page)).modules.length).toBeGreaterThan(0);
  await page.getByTestId('new-patch').click();
  await expect(page.getByTestId('dirty-dot')).not.toHaveClass(/on/);
  await reload(page);
  expect((await state(page)).modules).toEqual([]);
  await expect(page.getByTestId('patch-name')).toHaveText('Untitled');
});

test('autosave never clears the dirty marker; explicit Save keeps recovery current', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.doc.addModule('essentials.noise', {}, 30, 40));
  await expect(page.getByTestId('dirty-dot')).toHaveClass(/on/);
  await page.evaluate(() => window.__wirescapeTest.autosaveNow());
  await expect(page.getByTestId('dirty-dot')).toHaveClass(/on/);

  page.once('dialog', dialog => void dialog.accept('Named after edit'));
  await page.getByTestId('patch-name').click();
  await page.getByTestId('save-patch').click();
  await expect(page.getByTestId('dirty-dot')).not.toHaveClass(/on/);

  await reload(page);
  await expect(page.getByTestId('patch-name')).toHaveText('Named after edit');
  expect((await state(page)).modules.some((m: any) => m.module === 'essentials.noise')).toBe(true);
});

test('a clean file import recovers its name and unknown native fields', async ({ page }) => {
  await boot(page);
  const original = {
    format: 'wirescape.native.patch', version: 2,
    vendorExtension: { author: 'outside', id: 22 },
    modules: [
      { id: 1, module: 'vendor.unknown', parameters: { odd: 0.5 }, futureField: ['keep'] },
    ],
    connections: [],
    ui: { positions: { '1': [20, 30] }, labels: {},
      view: { x: 0, y: 0, zoom: 1 }, swatch: 'rust' },
  };
  await page.getByTestId('file-input').evaluate((element, patch) => {
    const input = element as HTMLInputElement;
    const transfer = new DataTransfer();
    transfer.items.add(new File([JSON.stringify(patch)], 'preserved.wirescape.json', { type: 'application/json' }));
    input.files = transfer.files;
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }, original);
  await expect(page.getByTestId('patch-name')).toHaveText('preserved');
  await expect(page.getByTestId('dirty-dot')).not.toHaveClass(/on/);
  await reload(page);
  const recovered = await state(page);
  await expect(page.getByTestId('patch-name')).toHaveText('preserved');
  expect(recovered.vendorExtension).toEqual(original.vendorExtension);
  expect(recovered.ui.swatch).toBe('rust');
  expect(recovered.modules[0].futureField).toEqual(['keep']);
  expect(recovered.modules[0].module).toBe('vendor.unknown');
});

test('invalid stored recovery falls back to an editable starter', async ({ page }) => {
  await boot(page);
  await page.addInitScript(key => localStorage.setItem(key, '{"patch":{"modules":{}}}'), KEY);
  await reload(page);
  await expect(page.getByTestId('patch-name')).toHaveText(DEFAULT_STARTER.name);
  await page.getByTestId('new-patch').click();
  expect((await state(page)).modules).toEqual([]);
});

test('unavailable localStorage cannot block startup or editing', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.addInitScript(() => {
    Object.defineProperty(window, 'localStorage', {
      configurable: true,
      get() { throw new DOMException('Storage disabled', 'SecurityError'); },
    });
  });
  await page.goto('/');
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
  await page.getByTestId('new-patch').click();
  await page.evaluate(() => window.__wirescapeTest.doc.addModule('essentials.noise', {}, 30, 40));
  await expect(page.getByTestId('dirty-dot')).toHaveClass(/on/);
  expect(errors).toEqual([]);
});

test('quota-exceeded writes cannot block patch selection or editing', async ({ page }) => {
  await boot(page);
  await page.evaluate(key => {
    const write = Storage.prototype.setItem;
    Storage.prototype.setItem = function (name, value) {
      if (name === key) throw new DOMException('Storage full', 'QuotaExceededError');
      return write.call(this, name, value);
    };
  }, KEY);
  await page.getByTestId('new-patch').click();
  await page.evaluate(() => window.__wirescapeTest.doc.addModule('essentials.noise', {}, 30, 40));
  await expect(page.getByTestId('dirty-dot')).toHaveClass(/on/);
  expect((await state(page)).modules).toHaveLength(1);
});

test('real Studio codec migrates v1 Six Essentials and preserves every field on v2 reload', async ({ page }) => {
  await boot(page);
  const source = JSON.parse(readFileSync(new URL('../../../native/examples/Six_Essentials_Demo.json', import.meta.url), 'utf8'));
  const migrated = await page.evaluate(patch => {
    const doc = window.__wirescapeTest.doc;
    doc.replace(patch);
    return doc.toJSON();
  }, source);
  expect(migrated.version).toBe(2);
  expect(migrated.preserved).toEqual(source);
  expect(migrated.modules).toHaveLength(source.modules.length);
  expect(migrated.outputs).toEqual([
    { module: source.outputNode, port: 'audio', bus_channel: 0 },
    { module: source.outputNode, port: 'audio', bus_channel: 1 },
  ]);
  const reloaded = await page.evaluate(text => {
    const doc = window.__wirescapeTest.doc;
    doc.replace(JSON.parse(text));
    return doc.toJSON();
  }, JSON.stringify(migrated));
  expect(reloaded).toEqual(migrated);
});
