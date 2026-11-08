// SPDX-License-Identifier: GPL-3.0-only
// Simulate one second of AudioWorklet render frames without opening an audio device.
import { expect, test } from '@playwright/test';

for (const rate of [44100, 48000, 96000, 192000]) {
  test(`worklet telemetry at ${rate} Hz never exceeds 30 reports per audio second`, async ({ page }) => {
    await page.goto('./');
    const result = await page.evaluate(async (sampleRate) => {
      const source = await (await fetch('./web/ws-processor.js')).text();
      const events: { kind: string }[] = [];
      const heap = new ArrayBuffer(65536);
      const bytes = new Uint8Array(heap);
      bytes.set([123, 125, 0], 256); // '{}': catalogue
      bytes.set([123, 125, 0], 512); // '{}': telemetry
      let Processor: any = null;
      class FakeInstance {
        exports = {
          memory: { buffer: heap },
          wsw_init: (_rate: number) => 1,
          wsw_catalogue: () => 256,
          wsw_render: (_frames: number) => 1024,
          wsw_right: () => 2048,
          wsw_telemetry: () => 512,
          wsw_peak: () => 0,
        };
      }
      class FakeWorklet {
        port = { postMessage: (message: { kind: string }) => { events.push(message); }, onmessage: null };
      }
      // Compile the exact served worklet with fake host globals in a test-only scope.
      const compile = new Function('sampleRate', 'currentTime', 'AudioWorkletProcessor',
        'registerProcessor', 'WebAssembly', source);
      compile(sampleRate, 0, FakeWorklet, (name: string, type: unknown) => {
        if (name !== 'wirescape-engine') throw new Error('unexpected worklet name');
        Processor = type;
      }, { Instance: FakeInstance, Module: { imports: () => [] } });
      if (!Processor) throw new Error('worklet was not registered');
      const processor = new Processor({ processorOptions: { module: {} } });
      const frames = 128;
      const rendered = Math.ceil(sampleRate / frames) * frames;
      for (let n = 0; n < rendered; n += frames) {
        if (!processor.process([], [[new Float32Array(frames), new Float32Array(frames)]])) {
          throw new Error('worklet stopped during simulated audio');
        }
      }
      return { ready: events.filter(e => e.kind === 'ready').length,
        count: events.filter(e => e.kind === 'telemetry').length };
    }, rate);
    expect(result.ready).toBe(1);
    expect(result.count).toBeGreaterThanOrEqual(25);
    expect(result.count).toBeLessThanOrEqual(30);
  });
}
