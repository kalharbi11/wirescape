// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state, knob } from './helpers';

/** #287 MIDI Learn: arm from a knob, confirm a captured CC, Forget. FakeEngine, no devices. */

const load = async (page: Page, modules: any[], connections: any[] = [], positions: Record<string, [number, number]> = {}) => {
  await page.evaluate(({ modules, connections, positions }) => {
    window.__wirescapeTest.load({
      format: 'wirescape.native.patch', version: 2, modules, connections,
      ui: { positions, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'Test');
  }, { modules, connections, positions });
  await page.evaluate(() => window.__wirescapeTest.zoomTo(1));
  await settle(page);
};

const KEYSTEP = 'Arturia KeyStep|midi:0', LAUNCHPAD = 'Launchpad Mini|midi:1';
const ksIn = { endpoint: 'Arturia KeyStep', system_name: 'midi:0' };
const lpIn = { endpoint: 'Launchpad Mini', system_name: 'midi:1' };
const ev = (from: number, to: number) => ({ kind: 'event', from: { module: from, port: 'midi' }, to: { module: to, port: 'midi' } });
const VCF_POS = { '2': [900, 80] as [number, number] };
const vcf = [{ id: 2, module: 'essentials.vcf' }];

async function learn(page: Page, module: number, param: string, cc: { channel: number; controller: number },
  opts: { any?: boolean; device?: string; name?: string } = {}) {
  await knob(page, module, param).click({ button: 'right' });
  await page.getByTestId('context-midi').click();
  await expect(page.getByTestId('learn-bar')).toBeVisible();
  await expect(page.getByTestId('learn-confirm')).toBeDisabled();
  const device = opts.device ?? KEYSTEP;
  await page.evaluate(({ cc, device }) => window.__wirescapeTest.engine.emitMidiCc(device, cc.channel, cc.controller, 99), { cc, device });
  await expect(page.getByTestId('learn-message')).toHaveText(`CC ${cc.controller} · ch ${cc.channel} · ${opts.name ?? 'Arturia KeyStep'}`);
  if (opts.any) await page.getByTestId('learn-any-channel').check();
  await page.getByTestId('learn-confirm').click();
  await settle(page);
}

const armed = (page: Page) => page.evaluate(() => (window.__wirescapeTest.engine as any).status.midi.learn?.armed === true);
const undoAll = async (page: Page) => { await page.getByTestId('undo').click(); await settle(page); };
const mods = (s: any, type: string) => s.modules.filter((m: any) => m.module === type);
const modCables = (s: any) => s.connections.filter((c: any) => c.kind === 'modulation');

test.describe('MIDI Learn (#287)', () => {
  test('learn on a bare patch adds MIDI In + CC to CV, writes the row and cables the knob; one undo reverts', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    const before = JSON.stringify((await state(page)).modules);
    await learn(page, 2, 'resonance', { channel: 2, controller: 74 });
    expect(await armed(page)).toBe(false);
    await expect(page.getByTestId('learn-bar')).toBeHidden();

    const s = await state(page);
    const cc = mods(s, 'midi.cc_to_cv'), src = mods(s, 'midi.in');
    expect(cc).toHaveLength(1);
    expect(src).toHaveLength(1);
    // The new MIDI In listens to exactly the device that moved.
    expect(src[0].config).toEqual(ksIn);
    expect(cc[0].config).toEqual({ rows: [{ channel: 2, controller: 74 }] });
    expect(s.connections).toContainEqual({ kind: 'event', from: { module: src[0].id, port: 'midi' }, to: { module: cc[0].id, port: 'midi' } });
    expect(modCables(s)).toEqual([{ kind: 'modulation', from: { module: cc[0].id, port: 'cc1' },
      to: { module: 2, port: 'resonance' }, amount: 1 }]);
    expect(s.ui.ccMap[String(cc[0].id)].cc1).toBe(74);
    await expect(page.getByTestId(`port-${cc[0].id}-output-cc1`).locator('.cc-label')).toHaveText('CC 74');
    const fake = await page.evaluate(id => (window.__wirescapeTest.engine as any).ccRowsOf(id), cc[0].id);
    expect(fake).toEqual([{ channel: 2, controller: 74 }]);

    await undoAll(page);
    const after = await state(page);
    expect(JSON.stringify(after.modules)).toBe(before);
    expect(after.connections).toEqual([]);
  });

  test('any channel stores omni; the learned patch saves and reloads identically', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    await learn(page, 2, 'cutoff', { channel: 5, controller: 21 }, { any: true });
    const saved = await state(page);
    expect(mods(saved, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 0, controller: 21 }]);
    await page.evaluate(p => window.__wirescapeTest.load(JSON.parse(JSON.stringify(p)), 'Again'), saved);
    await settle(page);
    const { ui: a, ...rest } = await state(page);
    const { ui: b, ...restSaved } = saved;
    expect(rest).toEqual(restSaved);
    expect(a.ccMap).toEqual(b.ccMap);
  });

  test('a second knob takes the next free row of the same module; re-learning a knob updates its row only', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    await learn(page, 2, 'cutoff', { channel: 1, controller: 20 });
    await learn(page, 2, 'resonance', { channel: 1, controller: 21 });
    let s = await state(page);
    const cc = mods(s, 'midi.cc_to_cv');
    expect(cc).toHaveLength(1);
    expect(cc[0].config.rows).toEqual([{ channel: 1, controller: 20 }, { channel: 1, controller: 21 }]);
    expect(modCables(s).map((c: any) => `${c.from.port}>${c.to.port}`).sort()).toEqual(['cc1>cutoff', 'cc2>resonance']);

    await learn(page, 2, 'cutoff', { channel: 3, controller: 30 });
    s = await state(page);
    expect(mods(s, 'midi.cc_to_cv')).toHaveLength(1);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 3, controller: 30 }, { channel: 1, controller: 21 }]);
    expect(modCables(s)).toHaveLength(2);
  });

  test('a full CC module is left alone: Learn adds another and reuses the existing MIDI In', async ({ page }) => {
    await boot(page);
    const rows = Array.from({ length: 8 }, (_, i) => ({ channel: 0, controller: 10 + i, bipolar: i === 0 }));
    await load(page, [{ id: 1, module: 'midi.in' }, { id: 5, module: 'midi.cc_to_cv', config: { rows } }, ...vcf],
      [{ kind: 'event', from: { module: 1, port: 'midi' }, to: { module: 5, port: 'midi' } },
        { kind: 'modulation', from: { module: 5, port: 'cc1' }, to: { module: 2, port: 'resonance' }, amount: 0.1 }],
      { '1': [50, 80], '5': [400, 80], ...VCF_POS });
    // Output cc2..cc8 are uncabled but explicit rows: they are occupied, so a new module is needed.
    await learn(page, 2, 'cutoff', { channel: 1, controller: 74 });
    const s = await state(page);
    const cc = mods(s, 'midi.cc_to_cv');
    expect(cc).toHaveLength(2);
    expect(cc.find((m: any) => m.id === 5).config.rows).toEqual(rows);
    const added = cc.find((m: any) => m.id !== 5);
    expect(added.config.rows).toEqual([{ channel: 1, controller: 74 }]);
    expect(mods(s, 'midi.in')).toHaveLength(1);
    expect(s.connections.filter((c: any) => c.kind === 'event' && c.to.module === added.id)).toHaveLength(1);
  });

  test('a wired implicit default output keeps its meaning: holes are written as explicit defaults', async ({ page }) => {
    await boot(page);
    await load(page, [{ id: 5, module: 'midi.cc_to_cv' }, ...vcf],
      [{ kind: 'modulation', from: { module: 5, port: 'cc1' }, to: { module: 2, port: 'cutoff' }, amount: 0.5 }],
      { '5': [400, 80], ...VCF_POS });
    await learn(page, 2, 'resonance', { channel: 1, controller: 74 });
    const s = await state(page);
    expect(mods(s, 'midi.cc_to_cv')).toHaveLength(1);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 0, controller: 1 }, { channel: 1, controller: 74 }]);
    expect(modCables(s).map((c: any) => `${c.from.port}>${c.to.port}`).sort()).toEqual(['cc1>cutoff', 'cc2>resonance']);
    // An unfed converter gets its MIDI feed in the same edit: the learned row is never inert.
    const src = mods(s, 'midi.in');
    expect(src).toHaveLength(1);
    expect(src[0].config).toEqual(ksIn);
    expect(s.connections).toContainEqual(ev(src[0].id, 5));
    expect(await page.evaluate(() => (window.__wirescapeTest.engine as any).ccRowsOf(5))).toEqual(
      [{ channel: 0, controller: 1 }, { channel: 1, controller: 74 }]);
  });

  test('two device-specific feeds: a CC from the second device lands on the converter that hears it', async ({ page }) => {
    await boot(page);
    const modules = [{ id: 1, module: 'midi.in', config: ksIn }, { id: 3, module: 'midi.in', config: lpIn },
      { id: 5, module: 'midi.cc_to_cv' }, { id: 6, module: 'midi.cc_to_cv' }, ...vcf];
    await load(page, modules, [ev(1, 5), ev(3, 6)], { '1': [0, 0], '3': [0, 300], '5': [400, 0], '6': [400, 300], ...VCF_POS });
    await learn(page, 2, 'cutoff', { channel: 4, controller: 12 }, { device: LAUNCHPAD, name: 'Launchpad Mini' });
    const s = await state(page);
    expect(s.modules).toHaveLength(modules.length);
    expect(s.modules.find((m: any) => m.id === 5).config).toBeUndefined();
    expect(s.modules.find((m: any) => m.id === 6).config).toEqual({ rows: [{ channel: 4, controller: 12 }] });
    expect(modCables(s)).toEqual([{ kind: 'modulation', from: { module: 6, port: 'cc1' }, to: { module: 2, port: 'cutoff' }, amount: 1 }]);
  });

  test('a converter fed by another device is left alone: a new converter and matching MIDI In are added', async ({ page }) => {
    await boot(page);
    const routes = [ev(1, 5), { kind: 'modulation', from: { module: 5, port: 'cc1' }, to: { module: 2, port: 'resonance' }, amount: 0.3 }];
    await load(page, [{ id: 1, module: 'midi.in', config: ksIn }, { id: 5, module: 'midi.cc_to_cv' }, ...vcf], routes,
      { '1': [0, 0], '5': [400, 0], ...VCF_POS });
    await learn(page, 2, 'cutoff', { channel: 1, controller: 40 }, { device: LAUNCHPAD, name: 'Launchpad Mini' });
    const s = await state(page);
    for (const r of routes) expect(s.connections).toContainEqual(r);
    expect(s.modules.find((m: any) => m.id === 5).config).toBeUndefined();
    const ins = mods(s, 'midi.in');
    expect(ins.map((m: any) => m.config)).toEqual([ksIn, lpIn]);
    const added = mods(s, 'midi.cc_to_cv').find((m: any) => m.id !== 5);
    expect(added.config).toEqual({ rows: [{ channel: 1, controller: 40 }] });
    expect(s.connections).toContainEqual(ev(ins[1].id, added.id));
    expect(s.connections).toHaveLength(routes.length + 2);
  });

  test('an all-enabled feed hears every device: its converter takes the row', async ({ page }) => {
    await boot(page);
    await load(page, [{ id: 1, module: 'midi.in', config: { endpoint: 'all_enabled' } }, { id: 5, module: 'midi.cc_to_cv' }, ...vcf],
      [ev(1, 5)], { '1': [0, 0], '5': [400, 0], ...VCF_POS });
    await learn(page, 2, 'cutoff', { channel: 9, controller: 3 }, { device: LAUNCHPAD, name: 'Launchpad Mini' });
    const s = await state(page);
    expect(s.modules).toHaveLength(3);
    expect(s.modules.find((m: any) => m.id === 1).config).toEqual({ endpoint: 'all_enabled' });
    expect(s.modules.find((m: any) => m.id === 5).config).toEqual({ rows: [{ channel: 9, controller: 3 }] });
  });

  test('a device that went away before Confirm is refused visibly and the patch is untouched', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    const before = JSON.stringify(await state(page));
    await knob(page, 2, 'cutoff').click({ button: 'right' });
    await page.getByTestId('context-midi').click();
    await page.evaluate(d => window.__wirescapeTest.engine.emitMidiCc(d, 1, 7, 50), LAUNCHPAD);
    await expect(page.getByTestId('learn-message')).toHaveText('CC 7 · ch 1 · Launchpad Mini');
    await page.evaluate(d => window.__wirescapeTest.engine.setMidiEnabled(d, false), LAUNCHPAD);
    await page.getByTestId('learn-confirm').click();
    await expect(page.getByTestId('toast').last()).toContainText('no longer connected');
    expect(JSON.stringify(await state(page))).toBe(before);
  });

  test('re-learning a knob that shares a CC row forks to a new row; Forget removes only its own cable', async ({ page }) => {
    await boot(page);
    const shared = (port: string) => ({ kind: 'modulation', from: { module: 5, port: 'cc1' }, to: { module: 2, port }, amount: 1 });
    const rows = [{ channel: 1, controller: 20 }];
    await load(page, [{ id: 1, module: 'midi.in' }, { id: 5, module: 'midi.cc_to_cv', config: { rows } }, ...vcf],
      [ev(1, 5), shared('cutoff'), shared('resonance')], { '1': [0, 0], '5': [400, 0], ...VCF_POS });
    await learn(page, 2, 'cutoff', { channel: 2, controller: 30 });
    let s = await state(page);
    expect(s.modules.find((m: any) => m.id === 5).config.rows).toEqual([...rows, { channel: 2, controller: 30 }]);
    expect(modCables(s).map((c: any) => `${c.from.port}>${c.to.port}`).sort()).toEqual(['cc1>resonance', 'cc2>cutoff']);
    await knob(page, 2, 'resonance').click({ button: 'right' });
    await page.getByTestId('context-forget').click();
    await settle(page);
    s = await state(page);
    expect(modCables(s).map((c: any) => `${c.from.port}>${c.to.port}`)).toEqual(['cc2>cutoff']);
    expect(s.modules.find((m: any) => m.id === 5).config.rows).toEqual([...rows, { channel: 2, controller: 30 }]);
    expect(s.connections).toContainEqual(ev(1, 5));
  });

  test('Forget removes only the learned cable, drops the freed trailing row, keeps unrelated cables', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    await learn(page, 2, 'cutoff', { channel: 1, controller: 20 });
    await learn(page, 2, 'resonance', { channel: 1, controller: 21 });
    await knob(page, 2, 'resonance').click({ button: 'right' });
    await page.getByTestId('context-forget').click();
    await settle(page);
    let s = await state(page);
    expect(modCables(s).map((c: any) => `${c.from.port}>${c.to.port}`)).toEqual(['cc1>cutoff']);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 1, controller: 20 }]);
    expect(mods(s, 'midi.in')).toHaveLength(1);
    // Nothing learned on this knob any more: no Forget entry.
    await knob(page, 2, 'resonance').click({ button: 'right' });
    await expect(page.getByTestId('context-forget')).toHaveCount(0);
    await page.keyboard.press('Escape');
    // Forgetting the only remaining mapping keeps its single row.
    await knob(page, 2, 'cutoff').click({ button: 'right' });
    await page.getByTestId('context-forget').click();
    await settle(page);
    s = await state(page);
    expect(modCables(s)).toEqual([]);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 1, controller: 20 }]);
  });

  test('Forget trims the freed CC label and undo restores its saved hint', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    await learn(page, 2, 'cutoff', { channel: 1, controller: 20 });
    await learn(page, 2, 'resonance', { channel: 1, controller: 21 });
    let s = await state(page);
    const ccId = mods(s, 'midi.cc_to_cv')[0].id;
    expect(s.ui.ccMap[String(ccId)]).toEqual({ cc1: 20, cc2: 21 });
    await expect(page.getByTestId(`port-${ccId}-output-cc2`).locator('.cc-label')).toHaveText('CC 21');

    await knob(page, 2, 'resonance').click({ button: 'right' });
    await page.getByTestId('context-forget').click();
    await settle(page);
    s = await state(page);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([{ channel: 1, controller: 20 }]);
    expect(s.ui.ccMap[String(ccId)]).toEqual({ cc1: 20 });
    await expect(page.getByTestId(`port-${ccId}-output-cc2`).locator('.cc-label')).toHaveText('CC 2');

    await undoAll(page);
    s = await state(page);
    expect(mods(s, 'midi.cc_to_cv')[0].config.rows).toEqual([
      { channel: 1, controller: 20 }, { channel: 1, controller: 21 },
    ]);
    expect(s.ui.ccMap[String(ccId)]).toEqual({ cc1: 20, cc2: 21 });
    await expect(page.getByTestId(`port-${ccId}-output-cc2`).locator('.cc-label')).toHaveText('CC 21');
  });

  test('Esc disarms without touching the patch; later CCs are ignored', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    const before = JSON.stringify(await state(page));
    await knob(page, 2, 'cutoff').click({ button: 'right' });
    await page.getByTestId('context-midi').click();
    await expect.poll(() => armed(page)).toBe(true);
    await page.keyboard.press('Escape');
    await expect(page.getByTestId('learn-bar')).toBeHidden();
    expect(await armed(page)).toBe(false);
    await page.evaluate(() => window.__wirescapeTest.engine.emitMidiCc('Arturia KeyStep|midi:0', 1, 74, 10));
    expect(JSON.stringify(await state(page))).toBe(before);
  });

  test('the engine dropping the arm (timeout) closes the bar', async ({ page }) => {
    await boot(page);
    await load(page, vcf, [], VCF_POS);
    await knob(page, 2, 'cutoff').click({ button: 'right' });
    await page.getByTestId('context-midi').click();
    await expect(page.getByTestId('learn-bar')).toBeVisible();
    await page.evaluate(() => window.__wirescapeTest.engine.midiLearn(false));
    await expect(page.getByTestId('learn-bar')).toBeHidden();
  });
});
