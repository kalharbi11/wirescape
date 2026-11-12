// SPDX-License-Identifier: GPL-3.0-only
// Observe the product context policy; override rates only for explicit diagnostic cases.
import { expect, test } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';

for (const requestedRate of [undefined, 44100, 48000]) {
  test(`macro rate diagnostic: ${requestedRate ?? 'browser default'}`, async ({ page }, info) => {
    // Explicit diagnostic contexts only. The default case and original matrix stay untouched.
    if (requestedRate !== undefined) {
      await page.addInitScript(rate => {
        const NativeAudioContext = window.AudioContext;
        window.AudioContext = class extends NativeAudioContext {
          constructor(options?: AudioContextOptions) { super({ ...options, sampleRate: rate }); }
        };
      }, requestedRate);
    }
    await page.goto('./');
    await page.getByTestId('web-start').click();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    const patch = CURATED_PATCHES.find(p => p.id === 'glass-cathedrals')!.patch;
    const evidence = await page.evaluate(async patch => {
      type Reply = { type?: string; re?: string; ok?: boolean; code?: string;
        message?: string; gen?: string; audio?: { sampleRate: number } };
      const bridge = (window as unknown as { __wirescapeWeb: {
        onMessage(fn: (msg: Reply) => void): () => void;
        send(msg: Record<string, unknown>): void;
      } }).__wirescapeWeb;
      const hello = await new Promise<Reply>((resolve, reject) => {
        const timer = setTimeout(() => { off(); reject(new Error('Missing Web hello')); }, 5000);
        const off = bridge.onMessage(msg => {
          if (msg.type !== 'hello') return;
          clearTimeout(timer); off(); resolve(msg);
        });
      });
      const load = (id: string, candidate: unknown) => new Promise<Reply>((resolve, reject) => {
        const timer = setTimeout(() => { off(); reject(new Error('Missing macro reply')); }, 5000);
        const off = bridge.onMessage(msg => {
          if (msg.re !== id) return;
          clearTimeout(timer); off(); resolve(msg);
        });
        bridge.send({ v: 0, id, type: 'loadGraph', patch: candidate });
      });
      const before = await load('macro-rate-baseline', {
        format: 'wirescape.native.patch', version: 2, settings: {},
        modules: [{ id: 1, module: 'essentials.output' }], connections: [],
      });
      const reply = await load('macro-rate-diagnostic', patch);
      return { sampleRate: hello.audio?.sampleRate, before, reply };
    }, patch);
    console.log('MACRO_RATE_DIAGNOSTIC', JSON.stringify(evidence));
    await info.attach('macro-rate-diagnostic', {
      body: JSON.stringify(evidence), contentType: 'application/json',
    });
    expect(Number.isFinite(evidence.sampleRate)).toBe(true);
    if (requestedRate !== undefined) expect(evidence.sampleRate).toBe(requestedRate);
    expect(evidence.before.ok).toBe(true);
    if (evidence.sampleRate === 48000) expect(evidence.reply.ok, JSON.stringify(evidence)).toBe(true);
    if (evidence.reply.ok) {
      expect(evidence.reply.gen).not.toBe(evidence.before.gen);
    } else {
      expect(evidence.reply.gen).toBe(evidence.before.gen);
      expect(evidence.reply.code).toBe('refused');
      expect(evidence.reply.message).toContain(String(evidence.sampleRate));
      expect(evidence.reply.message).toContain('prepare');
      expect(evidence.reply.message).toContain('ports.macro_oscillator requires 48000 Hz');
      expect(new TextEncoder().encode(evidence.reply.message).length).toBeLessThanOrEqual(320);
      await expect.poll(() => page.evaluate(() =>
        (window as unknown as { __wirescapeWeb: { peak: number } }).__wirescapeWeb.peak)).toBe(0);
    }
    // The original matrix still requires success; this diagnostic does not waive it.
  });
}
