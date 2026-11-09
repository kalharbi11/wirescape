// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, settle } from './helpers';
import { STARTERS } from '../src/app/starters';

test.use({ video: 'on' });
test('review recording, screenshots and frame samples', async ({ page, browser }, info) => {
  const piRun = info.project.metadata.piRun === true;
  test.setTimeout(piRun ? 300_000 : 90_000);
  const environment: Record<string, unknown> = { piRun, headed: info.project.use.headless === false, browser: browser.version() };
  try {
    const session = await browser.newBrowserCDPSession();
    environment.gpu = (await session.send('SystemInfo.getInfo')).gpu;
    await session.detach();
  } catch { environment.gpu = 'unavailable'; }
  const errors: string[] = [];
  page.on('pageerror', e => errors.push(e.message));
  await boot(page);
  environment.display = await page.evaluate(() => ({ width: screen.width, height: screen.height, dpr: devicePixelRatio, viewport: [innerWidth, innerHeight], userAgent: navigator.userAgent }));
  for (const id of ['witching-hour', 'vowel-apparitions', 'dust-reactor']) {
    const starter = STARTERS.find(s => s.id === id)!;
    await page.evaluate(({ patch, name }) => {
      const a = window.__wirescapeTest;
      a.load(patch, name); a.fit(); a.noteOn(60);
    }, { patch: starter.patch(), name: starter.name });
    await settle(page, 20);
    await page.screenshot({ path: info.outputPath(`${id}.png`) });
    // Exercise each signature control in the recording.
    if (id === 'witching-hour') await page.getByTestId('step-gate-2-2').click();
    if (id === 'vowel-apparitions') {
      const bank = page.locator('.model-bank');
      await bank.getByRole('radio').nth(7).click();
    }
    if (id === 'dust-reactor') await page.getByTestId('fader-thumb-3-level_a').press('ArrowUp');
    await settle(page, 20);
  }
  const samples = [];
  for (const workload of ['stress-20-40', 'signal-drone'] as const) {
    if (workload === 'signal-drone') {
      const starter = STARTERS.find(s => s.id === 'dust-reactor')!;
      await page.evaluate(({ patch, name }) => {
        const a = window.__wirescapeTest;
        a.load(patch, name); a.noteOn(60);
      }, { patch: starter.patch(), name: starter.name });
    } else {
      // Structural stress: 20 modules / 40 cables, moving modulation. This
      // intentionally has no Output; the drone separately exercises signal displays.
      await page.evaluate(() => {
        const a = window.__wirescapeTest;
        const modules = [], connections = [], positions: Record<number, [number, number]> = {};
        for (let i = 0; i < 10; i++) {
          const lfo = i + 1, filter = i + 11;
          modules.push({ id: lfo, module: 'essentials.lfo', parameters: { rate: 0.4 + i / 10 } },
            { id: filter, module: 'essentials.vcf', parameters: { cutoff: 800, resonance: 0.3 } });
          positions[lfo] = [(i % 5) * 600, Math.floor(i / 5) * 1200];
          positions[filter] = [(i % 5) * 600, Math.floor(i / 5) * 1200 + 600];
          connections.push(
            { kind: 'signal', from: { module: filter, port: 'low12' }, to: { module: 11 + (i + 1) % 10, port: 'audio' } },
            { kind: 'modulation', from: { module: lfo, port: 'control' }, to: { module: filter, port: 'cutoff' }, amount: 0.3 },
            { kind: 'modulation', from: { module: lfo, port: 'control' }, to: { module: filter, port: 'resonance' }, amount: 0.1 },
            { kind: 'modulation', from: { module: 1 + (i + 1) % 10, port: 'control' }, to: { module: filter, port: 'cutoff' }, amount: 0.1 },
          );
        }
        a.load({ format: 'wirescape.native.patch', version: 2, modules, connections, ui: { positions } });
        a.noteOn(60);
      });
    }
    for (const profile of ['normal', 'pi']) for (const zoom of ['fit', '100%']) {
      await page.evaluate(({ profile, zoom }) => {
        const a = window.__wirescapeTest;
        a.setPref({ profile, autoProfile: false });
        if (zoom === 'fit') a.fit(); else a.zoomTo(1);
      }, { profile, zoom });
      // Let layout, cable springs and telemetry settle before every condition.
      await page.waitForTimeout(2000);
      const sample = await page.evaluate(async ({ piRun, signalBearing }) => {
        const a = window.__wirescapeTest;
        const intervals: number[] = [];
        let sum = 0, allVisible = document.visibilityState === 'visible', allFocused = document.hasFocus();
        let visibilityInterruptions = 0, focusInterruptions = 0;
        let activeMeterFrames = 0, activeScopeFrames = 0, scopeUpdates = 0;
        let lastScopeSignature: number | undefined;
        const onVisibility = () => {
          if (document.visibilityState !== 'visible') { allVisible = false; visibilityInterruptions++; }
        };
        const onBlur = () => { allFocused = false; focusInterruptions++; };
        document.addEventListener('visibilitychange', onVisibility);
        window.addEventListener('blur', onBlur);
        let previous = performance.now();
        const start = previous;
        try {
          // Real-device evidence samples elapsed time, not a small fixed frame
          // count. Hosted CI keeps the original 120-frame smoke measurement.
          while (piRun ? previous - start < 15_000 : intervals.length < 120) {
            await new Promise<void>(resolve => requestAnimationFrame(() => resolve()));
            const now = performance.now();
            intervals.push(now - previous); previous = now;
            sum += a.ticker.applyMs;
            allVisible = allVisible && document.visibilityState === 'visible';
            allFocused = allFocused && document.hasFocus();
            if (signalBearing) {
              const telemetry = a.ticker.last;
              const meters = telemetry?.meters[6] as [number, number] | undefined;
              const scope = telemetry?.scope[6] as Float32Array | undefined;
              if (meters?.some(value => value > 0.001)) activeMeterFrames++;
              if (scope?.some(value => Math.abs(value) > 0.001)) activeScopeFrames++;
              // Count changed waveform buffers, not repeated browser frames
              // displaying the same 30 Hz telemetry packet.
              if (scope?.length) {
                const signature = scope[0]! + scope[Math.floor(scope.length / 3)]!;
                if (lastScopeSignature !== undefined && signature !== lastScopeSignature) scopeUpdates++;
                lastScopeSignature = signature;
              }
            }
          }
        } finally {
          document.removeEventListener('visibilitychange', onVisibility);
          window.removeEventListener('blur', onBlur);
        }
        const durationMs = previous - start;
        const sorted = [...intervals].sort((a, b) => a - b);
        const percentile = (p: number) => sorted[Math.max(0, Math.ceil(sorted.length * p) - 1)]!;
        const state = a.state();
        return {
          durationMs, frames: intervals.length, frameFps: intervals.length * 1000 / durationMs,
          p95FrameMs: percentile(0.95), p99FrameMs: percentile(0.99), maxFrameMs: sorted[sorted.length - 1]!,
          over33_3Ms: intervals.filter(ms => ms > 33.3).length,
          over50Ms: intervals.filter(ms => ms > 50).length,
          over100Ms: intervals.filter(ms => ms > 100).length,
          over250Ms: intervals.filter(ms => ms > 250).length,
          allVisible, allFocused, visibilityInterruptions, focusInterruptions,
          applyMs: sum / intervals.length, fps: a.fps(),
          modules: state.modules.length, cables: state.connections.length,
          outputModule: signalBearing ? state.modules.find((m: { id: number }) => m.id === 6)?.module : undefined,
          activeMeterFrames, activeScopeFrames, scopeUpdates,
        };
      }, { piRun, signalBearing: workload === 'signal-drone' });
      samples.push({ workload, profile, zoom, ...sample });
    }
  }
  await info.attach('frame-samples', { body: JSON.stringify({
    environment,
    note: piRun ? 'Pi runner, headed Chromium; 2 s warmup + at least 15 s per condition; fake engine only' : 'Cloud Chromium; 2 s warmup + 120 frames per condition; not Pi hardware measurements',
    samples,
  }, null, 2), contentType: 'application/json' });
  console.log('FRAME_SAMPLES', JSON.stringify({ environment, samples }));
  expect(samples).toHaveLength(8);
  for (const sample of samples) {
    const condition = `${sample.workload} / ${sample.profile} / ${sample.zoom}`;
    expect([sample.modules, sample.cables], condition).toEqual(sample.workload === 'stress-20-40' ? [20, 40] : [9, 9]);
    if (sample.workload === 'signal-drone') {
      expect(sample.outputModule, condition).toBe('essentials.output');
      expect(sample.activeMeterFrames, `${condition}: Output meters carry signal`).toBeGreaterThan(0);
      expect(sample.activeScopeFrames, `${condition}: Output scope carries signal`).toBeGreaterThan(0);
      expect(sample.scopeUpdates, `${condition}: Output scope keeps updating`).toBeGreaterThan(1);
    }
    if (piRun) {
      expect(sample.durationMs, condition).toBeGreaterThanOrEqual(15_000);
      expect(sample.allVisible && sample.allFocused, `${condition}: visible and focused throughout sampling`).toBe(true);
      expect(sample.visibilityInterruptions + sample.focusInterruptions, condition).toBe(0);
      expect(sample.frameFps, `${condition}: Pi target >=30 displayed frames/second`).toBeGreaterThanOrEqual(30);
      expect(sample.p95FrameMs, `${condition}: p95 pacing <=50 ms`).toBeLessThanOrEqual(50);
      expect(sample.p99FrameMs, `${condition}: p99 pacing <=100 ms`).toBeLessThanOrEqual(100);
      expect(sample.over250Ms, `${condition}: no stalls over 250 ms`).toBe(0);
    }
  }
  if (piRun) expect(environment.headed).toBe(true);
  expect(errors).toEqual([]);
});
