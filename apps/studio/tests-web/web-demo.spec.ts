// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';

const peak = (page: Page) => page.evaluate(() => (window as unknown as { __wirescapeWeb: { peak: number } }).__wirescapeWeb.peak);
const send = (page: Page, cmd: Record<string, unknown>) => page.evaluate(c =>
  (window as unknown as { __wirescapeWeb: { send(m: Record<string, unknown>): void } }).__wirescapeWeb.send(c), cmd);

test('web demo starts the wasm engine, loads the starter and makes sound', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.goto('./');
  await expect(page.getByTestId('web-splash')).toBeVisible();
  await page.getByTestId('web-start').click();
  await expect(page.getByTestId('web-splash')).toHaveCount(0);
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect(page.getByTestId('boot-error')).toHaveCount(0);
  await expect(page.getByTestId('open-patch')).toBeHidden();
  // The default starter is the keyboard voice: hold a note and the engine must be audible.
  await send(page, { v: 0, id: 'smoke-on', type: 'noteOn', note: 60, velocity: 1 });
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
  await send(page, { v: 0, id: 'smoke-off', type: 'noteOff', note: 60 });
  expect(errors).toEqual([]);
});

test('web demo refuses MIDI modules without breaking the live patch', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const reply = await page.evaluate(() => new Promise<Record<string, unknown>>(resolve => {
    const w = (window as unknown as { __wirescapeWeb: {
      send(m: Record<string, unknown>): void; onMessage(cb: (m: Record<string, unknown>) => void): () => void } }).__wirescapeWeb;
    const off = w.onMessage(m => { if (m.re === 'midi-add') { off(); resolve(m); } });
    w.send({ v: 0, id: 'midi-add', type: 'addModule', moduleId: 9000, moduleType: 'midi.in' });
  }));
  expect(reply.ok).toBe(false);
  expect(reply.code).toBe('unknown-module');
  await send(page, { v: 0, id: 'on2', type: 'noteOn', note: 64, velocity: 1 });
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
});

const held = (page: Page) => page.evaluate(() =>
  (window as unknown as { __wirescapeTest: { ticker: { last?: { heldNotes: number[] } } } }).__wirescapeTest.ticker.last?.heldNotes ?? []);

test('computer keys and on-screen keys play through the wasm engine and release', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.keyboard.down('a');
  await expect.poll(() => held(page)).toEqual([48]);
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
  await page.keyboard.up('a');
  await expect.poll(() => held(page)).toEqual([]);
  await page.getByTestId('toggle-keyboard').click();
  await page.getByTestId('piano-key-4').hover();
  await page.mouse.down();
  await expect.poll(() => held(page)).toEqual([52]);
  await page.mouse.up();
  await expect.poll(() => held(page)).toEqual([]);
});

// Worklet module fetches bypass page routing, so test processors are injected as blob URLs.
const withProcessor = (page: Page, source: string, readyTimeoutMs = 10_000) => page.addInitScript(([src, ms]) => {
  (window as unknown as { __wirescapeWebTest: unknown }).__wirescapeWebTest = {
    readyTimeoutMs: ms, processorUrl: URL.createObjectURL(new Blob([src as string], { type: 'text/javascript' })) };
}, [source, readyTimeoutMs] as const);

test('a processor that throws reports an error and leaves Try again usable', async ({ page }) => {
  await withProcessor(page, "registerProcessor('wirescape-engine', class extends AudioWorkletProcessor {" +
    " constructor() { super(); throw new Error('boom'); } process() { return true; } });");
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.getByTestId('web-error')).toContainText('stopped while starting');
  await expect(page.getByTestId('web-start')).toBeEnabled();
  await expect(page.getByTestId('web-start')).toHaveText('Try again');
});

test('a silent processor times out instead of hanging on Starting', async ({ page }) => {
  await withProcessor(page, "registerProcessor('wirescape-engine', class extends AudioWorkletProcessor { process() { return true; } });", 1500);
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.getByTestId('web-error')).toContainText('did not answer in time');
  await expect(page.getByTestId('web-start')).toBeEnabled();
});

test('a failed start followed by Try again starts the engine', async ({ page }) => {
  let fail = true;
  await page.route('**/web/wirescape.wasm', r => fail
    ? r.fulfill({ status: 503, contentType: 'application/wasm', body: '' }) : r.continue());
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.getByTestId('web-error')).toBeVisible();
  fail = false;
  await page.getByTestId('web-start').click();
  await expect(page.getByTestId('web-splash')).toHaveCount(0);
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.keyboard.down('a');
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
  await page.keyboard.up('a');
});
