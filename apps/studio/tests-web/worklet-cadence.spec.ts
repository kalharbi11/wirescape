// SPDX-License-Identifier: GPL-3.0-only
// Deterministic sample-time test: no real audio devices, browser permissions or timers.
import { expect, test } from '@playwright/test';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { runInNewContext } from 'node:vm';

const worklet = readFileSync(resolve(dirname(test.info().file), '../public/web/ws-processor.js'), 'utf8');

for (const rate of [44100, 48000, 96000, 192000]) {
  test(`worklet telemetry at ${rate} Hz never exceeds 30 reports per audio second`, () => {
    const events: { kind: string }[] = [];
    const heap = new ArrayBuffer(65536);
    const bytes = new Uint8Array(heap);
    bytes.set([123, 125, 0], 256); // empty catalogue
    bytes.set([123, 125, 0], 512); // empty telemetry
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
    runInNewContext(worklet, {
      sampleRate: rate,
      currentTime: 0,
      AudioWorkletProcessor: FakeWorklet,
      registerProcessor: (name: string, type: any) => {
        expect(name).toBe('wirescape-engine');
        Processor = type;
      },
      WebAssembly: { Instance: FakeInstance, Module: { imports: () => [] } },
    });
    expect(Processor).not.toBeNull();
    const processor = new Processor({ processorOptions: { module: {} } });
    expect(events.filter(e => e.kind === 'ready')).toHaveLength(1);
    // Exactly one second of audio quanta. Telemetry cadence must follow frames,
    // including rates for which 12 quanta would exceed the 30-Hz protocol budget.
    const frames = 128;
    const renderedFrames = Math.ceil(rate / frames) * frames;
    for (let n = 0; n < renderedFrames; n += frames) {
      expect(processor.process([], [[new Float32Array(frames), new Float32Array(frames)]])).toBe(true);
    }
    const count = events.filter(e => e.kind === 'telemetry').length;
    expect(count).toBeGreaterThanOrEqual(25);
    expect(count).toBeLessThanOrEqual(30);
  });
}
