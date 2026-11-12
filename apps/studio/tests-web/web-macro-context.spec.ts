// SPDX-License-Identifier: GPL-3.0-only
// Real Web AudioWorklet/WASM: product 48 kHz and truthful unsupported-rate fallback.
import { expect, test, type Page } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';

type Reply = {
  type?: string; re?: string; ok?: boolean; code?: string; message?: string;
  audio?: { sampleRate: number };
};
const scene = CURATED_PATCHES.find(p => p.id === 'glass-cathedrals')!.patch;

async function boot(page: Page): Promise<void> {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect(page.getByTestId('boot-error')).toHaveCount(0);
}

const hello = (page: Page) => page.evaluate(() => new Promise<Reply>((resolve, reject) => {
  const bridge = (window as unknown as { __wirescapeWeb: {
    onMessage(cb: (m: Reply) => void): () => void;
  } }).__wirescapeWeb;
  const timer = setTimeout(() => { off(); reject(new Error('Web hello missing')); }, 5000);
  const off = bridge.onMessage(msg => {
    if (msg.type !== 'hello') return;
    clearTimeout(timer); off(); resolve(msg);
  });
}));

test('real Web macro scene uses a 48 kHz browser context, no test-rate override', async ({ page }, info) => {
  await boot(page);
  const observed = await hello(page);
  await info.attach('real-web-audio-rate', {
    body: JSON.stringify(observed.audio), contentType: 'application/json',
  });
  expect(observed.audio?.sampleRate).toBe(48000);
  const reply = await page.evaluate(patch => new Promise<Reply>((resolve, reject) => {
    const bridge = (window as unknown as { __wirescapeWeb: {
      onMessage(cb: (m: Reply) => void): () => void;
      send(msg: Record<string, unknown>): void;
    } }).__wirescapeWeb;
    const timer = setTimeout(() => { off(); reject(new Error('Macro reply missing')); }, 5000);
    const off = bridge.onMessage(msg => {
      if (msg.re !== 'macro-real-48k') return;
      clearTimeout(timer); off(); resolve(msg);
    });
    bridge.send({ v: 0, id: 'macro-real-48k', type: 'loadGraph', patch });
  }), scene);
  expect(reply.ok, JSON.stringify(reply)).toBe(true);
});

test('unsupported 48 kHz AudioContext constructor falls back and reports actual rate', async ({ page }) => {
  await page.addInitScript(() => {
    const Native = window.AudioContext;
    (window as any).__rateFallbackUsed = false;
    (window as any).__nativeFallbackRate = null;
    window.AudioContext = class extends Native {
      constructor(options?: AudioContextOptions) {
        if (options?.sampleRate === 48000) {
          (window as any).__rateFallbackUsed = true;
          throw new DOMException('Requested rate unsupported', 'NotSupportedError');
        }
        super(options);
        (window as any).__nativeFallbackRate = this.sampleRate;
      }
    };
  });
  await boot(page);
  const observed = await hello(page);
  const actual = await page.evaluate(() => ({
    rejected: (window as any).__rateFallbackUsed as boolean,
    rate: (window as any).__nativeFallbackRate as number,
  }));
  expect(actual.rejected).toBe(true);
  expect(Number.isFinite(actual.rate) && actual.rate > 0).toBe(true);
  expect(observed.audio?.sampleRate).toBe(actual.rate);
});
