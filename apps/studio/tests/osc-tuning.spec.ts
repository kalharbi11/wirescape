// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';
import { midiToHz, hzToMidi, C0_HZ, G9_HZ, noteAndCents, formatVcoTune, parseVcoTune } from '../src/modules/musicalPitch';

test('equal-temperament reference and octave ratios', () => {
  expect(midiToHz(69)).toBe(440);
  expect(midiToHz(60)).toBeCloseTo(261.6255653, 6);
  expect(G9_HZ).toBeCloseTo(12543.85395, 3);
  expect(C0_HZ).toBeCloseTo(16.3515978, 5);
  for (const n of [12, 36, 60, 69, 95, 127]) {
    expect(hzToMidi(midiToHz(n))).toBeCloseTo(n, 9);
    expect(midiToHz(n + 12) / midiToHz(n)).toBeCloseTo(2, 10);
  }
  expect(noteAndCents(60)).toBe('C4 +0 ct');
  expect(noteAndCents(69.25)).toBe('A4 +25 ct');
  expect(noteAndCents(127)).toBe('G9 +0 ct');
  expect(formatVcoTune(220)).toBe('0 st 0 ct');
  expect(formatVcoTune(440)).toBe('+12 st 0 ct');
  expect(parseVcoTune('+12 st 0 ct')).toBeCloseTo(440, 9);
  expect(parseVcoTune('-12 st')).toBeCloseTo(110, 9);
});

async function vco(page: Page, frequency: number, connected: boolean): Promise<void> {
  await page.evaluate(({ frequency, connected }) => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'essentials.vco', parameters: { frequency } },
      { id: 2, module: 'host.keyboard', parameters: {} },
    ],
    connections: connected ? [{ kind: 'signal', from: { module: 2, port: 'pitch' }, to: { module: 1, port: 'pitch' } }] : [],
    ui: { positions: { 1: [40, 40], 2: [480, 40] }, view: { x: 0, y: 0, zoom: 1 } },
  }), { frequency, connected });
  await settle(page);
  const loaded = (await state(page)).connections;
  expect(loaded.some((c: any) => c.kind === 'signal' && c.to.module === 1 && c.to.port === 'pitch'))
    .toBe(connected);
}

test('VCO Hz note readout, musical endpoints and saved sub-audio range', async ({ page }) => {
  await boot(page);
  await vco(page, 261.6255653, false);
  const readout = page.getByTestId('readout-1-frequency');
  await expect(readout).toContainText('261.6 Hz · C4 +0 ct');
  const unit = page.getByTestId('knob-1-frequency');
  const dial = unit.locator('.knob');
  const labels = unit.locator('svg text.tl');
  await expect(labels.first()).toHaveText('C0');
  await expect(labels.last()).toHaveText('G9');
  await expect(dial).toHaveAttribute('aria-valuemin', String(C0_HZ));
  await expect(dial).toHaveAttribute('aria-valuemax', String(G9_HZ));
  await expect(dial).toHaveAttribute('aria-description', /C0 to G9.*0\.01 to 20,000 Hz/);
  await dial.press('Home');
  expect((await state(page)).modules[0].parameters.frequency).toBeCloseTo(C0_HZ, 1);
  expect(Number(await dial.getAttribute('aria-valuenow'))).toBeGreaterThanOrEqual(C0_HZ);
  await dial.press('End');
  expect((await state(page)).modules[0].parameters.frequency).toBeCloseTo(G9_HZ, 1);
  expect(Number(await dial.getAttribute('aria-valuenow'))).toBeLessThanOrEqual(G9_HZ);

  await vco(page, 0.01, false);
  expect((await state(page)).modules[0].parameters.frequency).toBe(.01);
  await expect(readout).toContainText('0.01 Hz');
  await expect(dial).toHaveAttribute('aria-valuenow', String(C0_HZ));
  await expect(dial).toHaveAttribute('aria-valuetext', /0\.01 Hz/);
  await readout.click();
  await page.getByTestId('readout-input-1-frequency').fill('A4');
  await page.getByTestId('readout-input-1-frequency').press('Enter');
  expect((await state(page)).modules[0].parameters.frequency).toBeCloseTo(440, 4);
  await vco(page, 20000, false);
  await expect(readout).toContainText('20 kHz');
  await expect(dial).toHaveAttribute('aria-valuenow', String(G9_HZ));
  await expect(dial).toHaveAttribute('aria-valuetext', /20 kHz/);
  expect((await state(page)).modules[0].parameters.frequency).toBe(20000);
});

test('VCO pitch cable relabels ratio as Tune and double-click resets to 220', async ({ page }) => {
  await boot(page);
  await vco(page, 440, true);
  const unit = page.getByTestId('knob-1-frequency');
  await expect(unit.locator('.control-label')).toHaveText('Tune');
  await expect(page.getByTestId('readout-1-frequency')).toHaveText('+12 st 0 ct');
  await page.getByTestId('readout-1-frequency').click();
  const edit = page.getByTestId('readout-input-1-frequency');
  await expect(edit).toHaveValue('+12 st 0 ct');
  await edit.fill('0 st 0 ct');
  await edit.press('Enter');
  expect((await state(page)).modules[0].parameters.frequency).toBeCloseTo(220, 4);
  await vco(page, 440, true);
  await unit.locator('.knob').dblclick();
  expect((await state(page)).modules[0].parameters.frequency).toBe(220);
  await vco(page, 220, false);
  await expect(unit.locator('.control-label')).toHaveText('Frequency');
  await expect(page.getByTestId('readout-1-frequency')).toContainText('220 Hz · A3 +0 ct');
});

test('VCO pitch cable removal and reconnect refresh the mounted Tune dial', async ({ page }) => {
  await boot(page);
  await vco(page, 440, true);
  const unit = page.getByTestId('knob-1-frequency');
  const dial = unit.locator('.knob');
  const mountedDial = await dial.elementHandle();
  expect(mountedDial).not.toBeNull();
  await expect(unit.locator('.control-label')).toHaveText('Tune');
  await expect(dial).toHaveAttribute('aria-label', 'Tune');
  await expect(page.getByTestId('readout-1-frequency')).toHaveText('+12 st 0 ct');

  const cable = (await state(page)).connections[0];
  expect(cable).toMatchObject({
    kind: 'signal', from: { module: 2, port: 'pitch' }, to: { module: 1, port: 'pitch' },
  });
  // Use the real app deletion path, without load/rebuild or changing frequency.
  await page.evaluate(c => {
    window.__wirescapeTest.select({ kind: 'cable', c });
    window.__wirescapeTest.deleteSelection();
  }, cable);
  await settle(page);
  expect((await state(page)).connections).toHaveLength(0);
  await expect(unit.locator('.control-label')).toHaveText('Frequency');
  await expect(dial).toHaveAttribute('aria-label', 'Frequency');
  await expect(page.getByTestId('readout-1-frequency')).toHaveText('440 Hz · A4 +0 ct');
  expect(await mountedDial!.evaluate(el => el.isConnected)).toBe(true);

  // Reconnect in the live document; the same knob must resume Tune reporting.
  await page.evaluate(c => window.__wirescapeTest.connect(c), cable);
  await settle(page);
  expect((await state(page)).connections).toHaveLength(1);
  await expect(unit.locator('.control-label')).toHaveText('Tune');
  await expect(dial).toHaveAttribute('aria-label', 'Tune');
  await expect(page.getByTestId('readout-1-frequency')).toHaveText('+12 st 0 ct');
  expect(await mountedDial!.evaluate(el => el.isConnected)).toBe(true);
  expect((await state(page)).modules[0].parameters.frequency).toBe(440);
});

test('nominal pitch readout follows FakeEngine observations and clears replaced snapshots', async ({ page }) => {
  await boot(page);
  await vco(page, 440, false);
  const live = page.getByTestId('pitch-1');
  await expect(live).toHaveText('A4 +0 ct · 440 Hz');
  await expect(page.getByTestId('knob-1-frequency').locator('.knob-pitch-caption'))
    .toHaveText('NOMINAL BASE · FM NOT SHOWN');
  await page.evaluate(() => window.__wirescapeTest.noteOn(72));
  // No note cable: the native oscillator stays at its free-running base.
  await expect(live).toHaveText('A4 +0 ct · 440 Hz');
  await vco(page, 220, true);
  await page.evaluate(() => window.__wirescapeTest.noteOn(69));
  await expect(live).toHaveText('A4 +0 ct · 440 Hz');
  await vco(page, 220, false);
  await expect(live).toHaveText('A3 +0 ct · 220 Hz');
  // The complete-map contract evicts an old observation even when the card stays mounted.
  await page.evaluate(() => window.__wirescapeTest.ticker.push({
    tick: 999, time: 0, effective: {}, effective_pitch: {}, phase: {},
    envelope: {}, meters: {}, scope: {}, eventActivity: {}, heldNotes: [],
  }));
  await expect(live).toHaveText('—');
});

test('Macro coarse displays C4-relative notes without writing new parameters', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'ports.macro_oscillator', parameters: { coarse: 9 } }],
    connections: [], ui: { positions: { 1: [40, 40] }, view: { x: 0, y: 0, zoom: 1 } },
  }));
  await settle(page);
  await expect(page.getByTestId('readout-1-coarse')).toHaveText('A4 +0 ct');
  await page.getByTestId('readout-1-coarse').click();
  await page.getByTestId('readout-input-1-coarse').fill('0');
  await page.getByTestId('readout-input-1-coarse').press('Enter');
  expect((await state(page)).modules[0].parameters.coarse).toBe(0);
  await expect(page.getByTestId('readout-1-coarse')).toHaveText('C4 +0 ct');
});
