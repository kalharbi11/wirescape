// SPDX-License-Identifier: GPL-3.0-only
// Independent #629 non-OSC patch matrix against the real Web AudioWorklet/WASM.
// This is not a FakeEngine test or an owner hardware/audio listening check.
import { expect, test, type Page } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';
import echo from '../patches/basics-03-echo-and-reverb.json';
import randomMelody from '../patches/basics-05-random-melody.json';

type Reply = { re?: string; ok?: boolean; code?: string };
const scene = (id: string) => CURATED_PATCHES.find(p => p.id === id)!.patch;

const scenarios = [
  { name: 'VCO, envelope, delay and reverb', patch: echo,
    required: ['essentials.vco', 'essentials.adsr', 'fx.delay', 'fx.reverb'], note: 60 },
  { name: 'clocked random, quantizer and resonator', patch: randomMelody,
    required: ['essentials.clock', 'example.random_source', 'essentials.quantizer', 'essentials.resonator'], note: null },
  { name: 'step sequencer, filter and envelope', patch: scene('witching-hour'),
    required: ['example.step_sequencer', 'essentials.vcf', 'essentials.adsr'], note: null },
  { name: 'macro oscillator and ladder', patch: scene('glass-cathedrals'),
    required: ['ports.macro_oscillator', 'essentials.ladder'], note: 60 },
  { name: 'two-voice CHAOS and stereo filtering', patch: scene('hollow-chapel'),
    required: ['essentials.chaos_lite', 'essentials.ladder', 'essentials.mixer'], note: null },
];

const boot = async (page: Page) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect(page.getByTestId('boot-error')).toHaveCount(0);
};

const command = (page: Page, message: Record<string, unknown>) =>
  page.evaluate(cmd => new Promise<Reply>(resolve => {
    const bridge = (window as unknown as { __wirescapeWeb: {
      onMessage(listener: (msg: Reply) => void): () => void;
      send(msg: Record<string, unknown>): void;
    } }).__wirescapeWeb;
    const off = bridge.onMessage(reply => {
      if (reply.re !== cmd.id) return;
      off();
      resolve(reply);
    });
    bridge.send(cmd);
  }), message);

const liveModules = (page: Page) => page.evaluate(() =>
  (window as unknown as { __wirescapeTest: { registry: { all: { id: string }[] } } })
    .__wirescapeTest.registry.all.map(m => m.id));

const peak = (page: Page) => page.evaluate(() =>
  (window as unknown as { __wirescapeWeb: { peak: number } }).__wirescapeWeb.peak);

for (const scenario of scenarios) {
  test(`real WASM patch matrix: ${scenario.name}`, async ({ page }) => {
    await boot(page);
    const catalogue = await liveModules(page);
    for (const id of scenario.required) expect(catalogue, `live Web module ${id}`).toContain(id);
    const moduleTypes = scenario.patch.modules.map(m => m.module);
    expect(moduleTypes.filter(id => !catalogue.includes(id)), 'fixture modules unavailable on Web').toEqual([]);

    const loaded = await command(page, {
      v: 0, id: 'matrix-load', type: 'loadGraph', patch: scenario.patch,
    });
    expect(loaded.ok, JSON.stringify(loaded)).toBe(true);
    if (scenario.note !== null) {
      const on = await command(page, {
        v: 0, id: 'matrix-note-on', type: 'noteOn', note: scenario.note, velocity: 1,
      });
      expect(on.ok, JSON.stringify(on)).toBe(true);
    }
    await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.00001);

    // Check Studio's actual document serialization, not a JSON clone in the test.
    await page.evaluate(({ patch, name }) =>
      (window as any).__wirescapeTest.load(patch, name), { patch: scenario.patch, name: scenario.name });
    await expect(page.getByTestId('patch-name')).toContainText(scenario.name);
    const saved = await page.evaluate(() =>
      (window as unknown as { __wirescapeTest: {
        save(): { modules: { module: string }[]; connections: unknown[] };
      } }).__wirescapeTest.save());
    expect(saved.modules.map(m => m.module).sort()).toEqual(moduleTypes.slice().sort());
    expect(saved.connections).toHaveLength(scenario.patch.connections.length);
    await page.evaluate(patch =>
      (window as any).__wirescapeTest.load(patch, 'Matrix reload'), saved);
    await expect(page.getByTestId('patch-name')).toContainText('Matrix reload');
    expect((await liveModules(page)).length).toBeGreaterThan(0);
  });
}

test('real WASM Chord event routing reports Notes-to-CV availability separately', async ({ page }) => {
  await boot(page);
  const catalogue = await liveModules(page);
  expect(catalogue).toContain('essentials.chord');
  const adapterAvailable = catalogue.includes('essentials.notes_to_cv');
  if (!adapterAvailable) {
    test.info().annotations.push({
      type: 'not-implemented',
      description: 'Notes-to-CV is not in this Web build; only Chord event-route acceptance is tested',
    });
  }
  const modules = [
    { id: 1, module: 'host.keyboard', version: 1 },
    { id: 2, module: 'essentials.chord', version: 1 },
    ...(adapterAvailable ? [
      { id: 3, module: 'essentials.notes_to_cv', version: 1 },
      { id: 4, module: 'essentials.vco', version: 2 },
      { id: 5, module: 'essentials.adsr', version: 2 },
      { id: 6, module: 'essentials.vca', version: 2 },
    ] : []),
    { id: 7, module: 'essentials.output', version: 2 },
  ];
  const connection = (kind: string, from: number, fromPort: string, to: number, toPort: string) => ({
    kind, from: { module: from, port: fromPort }, to: { module: to, port: toPort },
  });
  const connections = [
    connection('event', 1, 'notes', 2, 'notes'),
    ...(adapterAvailable ? [
      connection('event', 2, 'notes', 3, 'notes'),
      connection('signal', 3, 'pitch', 4, 'pitch'),
      connection('signal', 3, 'gate', 5, 'gate'),
      connection('signal', 4, 'audio', 6, 'audio'),
      connection('signal', 5, 'envelope', 6, 'gain_cv'),
      connection('signal', 6, 'audio', 7, 'audio'),
    ] : []),
  ];
  const patch = {
    format: 'wirescape.native.patch', version: 2, modules, connections,
    outputs: [{ module: 7, port: 'audio', bus_channel: 0 },
      { module: 7, port: 'audio', bus_channel: 1 }],
  };
  const loaded = await command(page, { v: 0, id: 'chord-route', type: 'loadGraph', patch });
  expect(loaded.ok, JSON.stringify(loaded)).toBe(true);
  const on = await command(page, { v: 0, id: 'chord-note', type: 'noteOn', note: 60, velocity: 1 });
  expect(on.ok, JSON.stringify(on)).toBe(true);
  if (adapterAvailable) {
    await expect.poll(() => peak(page), { timeout: 15_000 }).toBeGreaterThan(0.00001);
  }
  const off = await command(page, { v: 0, id: 'chord-off', type: 'noteOff', note: 60 });
  expect(off.ok, JSON.stringify(off)).toBe(true);
});
