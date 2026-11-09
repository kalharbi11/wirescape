// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';

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
  await expect(page.getByTestId('open-patch')).toBeVisible();
  await expect(page.getByTestId('share-patch')).toBeVisible();
  // The default starter is the keyboard voice: hold a note and the engine must be audible.
  await send(page, { v: 0, id: 'smoke-on', type: 'noteOn', note: 60, velocity: 1 });
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
  await send(page, { v: 0, id: 'smoke-off', type: 'noteOff', note: 60 });
  expect(errors).toEqual([]);
});


// The clock and sequencer are self-running: no computer key or MIDI event is needed.
test('web demo plays a curated step progression immediately after loading', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const patch = CURATED_PATCHES.find(s => s.id === 'witching-hour')!;
  await page.evaluate(({ patch, name }) =>
    (window as any).__wirescapeTest.load(patch, name), { patch: patch.patch, name: patch.name });
  await expect(page.getByTestId('patch-name')).toContainText('Witching Hour');
  await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.001);
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

test('web demo lists only starters and modules its engine can play', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const ids = await page.getByTestId('starter-select').locator('option').evaluateAll(
    os => os.map(o => (o as HTMLOptionElement).value).filter(Boolean));
  expect(ids).toContain('keyboard-voice');
  const catalogue = await page.evaluate(() =>
    (window as unknown as { __wirescapeTest: { registry: { all: { id: string }[] } } }).__wirescapeTest.registry.all.map(m => m.id));
  expect(catalogue.some(id => id.startsWith('midi.'))).toBe(false);
  const starters = await page.evaluate(() =>
    (window as unknown as { __wirescapeTest: { starters: { id: string; patch(): { modules: { module: string }[] } }[] } })
      .__wirescapeTest.starters.map(s => ({ id: s.id, modules: s.patch().modules.map(m => m.module) })));
  for (const s of starters) {
    const playable = s.modules.every(m => catalogue.includes(m));
    expect(ids.includes(s.id), s.id).toBe(playable);
  }
  expect(starters.some(s => s.modules.some(m => m.startsWith('midi.')))).toBe(true);
  await expect(page.getByTestId('library-item-midi.in')).toHaveCount(0);
  await expect(page.getByTestId('library-item-essentials.vco')).toHaveCount(1);
});

const modules = (page: Page) => page.evaluate(() =>
  (window as unknown as { __wirescapeTest: { save(): { modules: { id: number; module: string }[] } } })
    .__wirescapeTest.save().modules.map(m => `${m.id}:${m.module}`).sort());

test('SHARE puts the patch in a link that opens the same patch', async ({ page, context }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const sequence = CURATED_PATCHES.find(p => p.id === 'witching-hour')!;
  await page.evaluate(p => (window as any).__wirescapeTest.load(p.patch, p.name), sequence);
  await expect.poll(() => modules(page)).toContain('2:example.step_sequencer');
  const expected = await modules(page);
  await page.getByTestId('share-patch').click();
  await expect.poll(() => page.evaluate(() => location.hash)).toMatch(/^#p=/);
  const link = await page.evaluate(() => location.href);
  const other = await context.newPage();
  await other.goto(link);
  await other.getByTestId('web-start').click();
  await expect(other.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect.poll(() => modules(other)).toEqual(expected);
  await expect.poll(() => other.evaluate(() => location.hash)).toBe('');
  await expect(other.getByTestId('share-patch')).toBeVisible();
  // The shared sequencer must resume without a keyboard or MIDI event.
  await expect.poll(() => peak(other), { timeout: 15_000 }).toBeGreaterThan(0.001);
});

test('a malformed shared link falls back to the normal start', async ({ page }) => {
  await page.goto('./#p=not-a-patch');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect.poll(() => modules(page)).toContain('1:host.keyboard');
});

test('SAVE downloads the patch as JSON', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.evaluate(() => {
    const w = window as unknown as { __saved?: Promise<string> };
    const original = URL.createObjectURL.bind(URL);
    URL.createObjectURL = (o: Blob | MediaSource) => { if (o instanceof Blob) w.__saved = o.text(); return original(o); };
  });
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('save-patch').click()]);
  expect(download.suggestedFilename()).toBe('wirescape-patch.json');
  const patch = JSON.parse(await page.evaluate(() => (window as unknown as { __saved: Promise<string> }).__saved));
  expect(patch.format).toBe('wirescape.native.patch');
  expect(patch.modules.length).toBeGreaterThan(0);
});

test('live telemetry reaches the UI: cable signal values and the output scope', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.keyboard.down('a');
  const last = () => page.evaluate(() => {
    const t = (window as unknown as { __wirescapeTest: { ticker: { last?: {
      outputs?: Record<string, number>; scope: Record<string, ArrayLike<number>> } } } }).__wirescapeTest.ticker.last;
    return { outputs: Object.keys(t?.outputs ?? {}).length, scope: Object.values(t?.scope ?? {}).map(s => s.length) };
  });
  await expect.poll(async () => (await last()).outputs).toBeGreaterThan(0);
  await expect.poll(async () => (await last()).scope).toEqual([64]);
  await page.keyboard.up('a');
});


// The initial audio permission screen must remain usable even in Safari's
// shortest portrait/landscape visual viewport, before any WASM is loaded.
test('audio-start splash fits or scrolls on a narrow, short phone viewport', async ({ page }) => {
  await page.setViewportSize({ width: 320, height: 360 });
  await page.goto('./');
  const splash = page.getByTestId('web-splash');
  await expect(splash).toBeVisible();
  const start = page.getByTestId('web-start');
  await start.scrollIntoViewIfNeeded();
  await expect(start).toBeInViewport();
  const result = await page.evaluate(() => {
    const splash = document.querySelector('.web-splash') as HTMLElement;
    const panel = document.querySelector('.web-splash-card') as HTMLElement;
    const rect = panel.getBoundingClientRect();
    return { width: rect.width, left: rect.left, right: rect.right,
      scrollable: splash.scrollHeight >= splash.clientHeight };
  });
  expect(result.width).toBeLessThanOrEqual(320);
  expect(result.left).toBeGreaterThanOrEqual(0);
  expect(result.right).toBeLessThanOrEqual(320);
  expect(result.scrollable).toBe(true);
});

test('web audio resume is requested inside the Start tap, before async WASM loading', async ({ page }) => {
  await page.addInitScript(() => {
    const order: string[] = [];
    (window as unknown as { __webStartOrder: string[] }).__webStartOrder = order;
    const resume = AudioContext.prototype.resume;
    AudioContext.prototype.resume = function () { order.push('resume'); return resume.call(this); };
    const nativeFetch = window.fetch.bind(window);
    window.fetch = (...args) => {
      if (String(args[0]).includes('wirescape.wasm')) order.push('fetch-wasm');
      return nativeFetch(...args);
    };
  });
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const order = await page.evaluate(() =>
    (window as unknown as { __webStartOrder: string[] }).__webStartOrder);
  expect(order.indexOf('resume')).toBeGreaterThanOrEqual(0);
  expect(order.indexOf('fetch-wasm')).toBeGreaterThanOrEqual(0);
  expect(order.indexOf('resume')).toBeLessThan(order.indexOf('fetch-wasm'));
});

test('web bridge does not drop genuine engine animation telemetry when supplied', async ({ page }) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const telemetry = await page.evaluate(() => {
    const bridge = (window as unknown as { __wirescapeWeb: {
      onMessage(cb: (m: Record<string, any>) => void): () => void;
      receive(m: Record<string, unknown>): void;
      gen: string;
    } }).__wirescapeWeb;
    return new Promise<Record<string, any>>(resolve => {
      const off = bridge.onMessage(m => {
        if (m.type !== 'telemetry' || m.telemetry?.phase?.['9'] === undefined) return;
        off();
        resolve(m.telemetry);
      });
      bridge.receive({ kind: 'telemetry', gen: bridge.gen, telemetry: {
        effective: { 9: { cutoff: 1234 } }, phase: { 9: 0.25 },
        envelope: { 9: 0.5 }, eventActivity: { '9:gate': 0.8 },
        meters: {}, scope: {}, outputs: {},
      } });
    });
  });
  expect(telemetry.effective['9'].cutoff).toBe(1234);
  expect(telemetry.phase['9']).toBe(0.25);
  expect(telemetry.envelope['9']).toBe(0.5);
  expect(telemetry.eventActivity['9:gate']).toBe(0.8);
});

test.describe('iPhone-sized browser presentation', () => {
  test.use({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true, deviceScaleFactor: 3 });

  test('canvas, library, keyboard and chrome stay usable in portrait', async ({ page }, testInfo) => {
    await page.goto('./');
    await expect(page.getByTestId('web-splash')).toBeVisible();
    await testInfo.attach('web-iphone-start.png', {
      body: await page.screenshot({ animations: 'disabled' }), contentType: 'image/png',
    });
    await page.getByTestId('web-start').click();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await expect(page.getByTestId('patch-name')).toBeVisible();
    await expect(page.locator('html')).toHaveAttribute('data-hit', 'touch');
    await page.getByTestId('menu-toggle').click();
    await expect(page.getByTestId('library-backdrop')).toBeVisible();
    await page.mouse.click(375, 280);
    await expect(page.getByTestId('library-backdrop')).toBeHidden();

    const toolbar = page.locator('.topbar-right');
    await toolbar.evaluate(el => { el.scrollLeft = el.scrollWidth; });
    await expect(page.getByTestId('save-patch')).toBeInViewport();
    await page.getByTestId('toggle-keyboard').click();
    await expect(page.getByTestId('on-screen-keyboard')).toBeVisible();

    const bounds = await page.evaluate(() => ({
      top: document.querySelector('.topbar')!.getBoundingClientRect().top,
      foot: document.querySelector('.statusbar')!.getBoundingClientRect().bottom,
      width: document.documentElement.scrollWidth,
      height: window.innerHeight,
    }));
    expect(bounds.top).toBeGreaterThanOrEqual(-1);
    expect(bounds.foot).toBeLessThanOrEqual(bounds.height + 1);
    expect(bounds.width).toBeLessThanOrEqual(390);
    await testInfo.attach('web-iphone-canvas-and-keys.png', {
      body: await page.screenshot({ animations: 'disabled' }), contentType: 'image/png',
    });
  });
});
