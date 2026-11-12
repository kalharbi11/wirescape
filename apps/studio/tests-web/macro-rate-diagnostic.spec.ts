// SPDX-License-Identifier: GPL-3.0-only
// Observe the default AudioContext; never force a rate to make the matrix pass.
import { expect, test } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';

test('macro refusal records the actual default browser rate and loader reason', async ({ page }, info) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  const patch = CURATED_PATCHES.find(p => p.id === 'glass-cathedrals')!.patch;
  const evidence = await page.evaluate(async patch => {
    type Reply = { type?: string; re?: string; ok?: boolean; code?: string;
      message?: string; audio?: { sampleRate: number } };
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
    const reply = await new Promise<Reply>((resolve, reject) => {
      const timer = setTimeout(() => { off(); reject(new Error('Missing macro reply')); }, 5000);
      const off = bridge.onMessage(msg => {
        if (msg.re !== 'macro-rate-diagnostic') return;
        clearTimeout(timer); off(); resolve(msg);
      });
      bridge.send({ v: 0, id: 'macro-rate-diagnostic', type: 'loadGraph', patch });
    });
    return { sampleRate: hello.audio?.sampleRate, reply };
  }, patch);
  console.log('MACRO_RATE_DIAGNOSTIC', JSON.stringify(evidence));
  await info.attach('macro-rate-diagnostic', {
    body: JSON.stringify(evidence), contentType: 'application/json',
  });
  expect(Number.isFinite(evidence.sampleRate)).toBe(true);
  if (evidence.sampleRate === 48000) {
    expect(evidence.reply.ok, JSON.stringify(evidence)).toBe(true);
  } else {
    expect(evidence.reply.ok, JSON.stringify(evidence)).toBe(false);
    expect(evidence.reply.code).toBe('refused');
    expect(evidence.reply.message).toContain(String(evidence.sampleRate));
    expect(evidence.reply.message).toContain('prepare');
  }
  // The original matrix still requires success; this diagnostic does not waive it.
});
