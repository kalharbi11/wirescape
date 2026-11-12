// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { readFileSync } from 'node:fs';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { resolve } from 'node:path';
import {
  boot, settle, state, addModuleAt, patch, patchToKnob, port, knob, markerAngle, dragModuleTo,
} from './helpers';

/** The saved view drifts with the window; the patch itself must not. */
function withoutView(patch: any) {
  const { ui, ...rest } = patch;
  const { view: _view, ...uiRest } = ui ?? {};
  return { ...rest, ui: uiRest };
}

/**
 * The done-checklist session: build the demo patch by hand, play it, watch
 * modulation move, then save and reload and get the same patch back.
 */
async function buildDemoPatch(page: import('@playwright/test').Page) {
  await page.evaluate(() => window.__wirescapeTest.zoomTo(0.5));
  await settle(page);

  const keyboard = await addModuleAt(page, 'host.keyboard', 80, 260);
  const vco = await addModuleAt(page, 'essentials.vco', 620, 260);
  const vcf = await addModuleAt(page, 'essentials.vcf', 1160, 260);
  const vca = await addModuleAt(page, 'essentials.vca', 1700, 260);
  const output = await addModuleAt(page, 'essentials.output', 2240, 260);
  const adsr = await addModuleAt(page, 'essentials.adsr', 620, 760);
  const lfo = await addModuleAt(page, 'essentials.lfo', 1500, 760);

  await patch(page, { module: keyboard, port: 'pitch' }, { module: vco, port: 'pitch' });
  await patch(page, { module: keyboard, port: 'gate' }, { module: adsr, port: 'gate' });
  await patch(page, { module: vco, port: 'audio' }, { module: vcf, port: 'audio' });
  await patch(page, { module: vcf, port: 'low24' }, { module: vca, port: 'audio' });
  await patch(page, { module: adsr, port: 'envelope' }, { module: vca, port: 'gain_cv' });
  await patch(page, { module: vca, port: 'audio' }, { module: output, port: 'audio' });
  await patchToKnob(page, { module: lfo, port: 'control' }, { module: vcf, param: 'cutoff' });

  return { keyboard, vco, vcf, vca, output, adsr, lfo };
}

test.describe('a whole session', () => {
  test('builds the demo patch, plays it, and round-trips through save', async ({ page }) => {
    await boot(page);
    const ids = await buildDemoPatch(page);

    const s = await state(page);
    expect(s.modules).toHaveLength(7);
    expect(s.connections).toHaveLength(7);

    const mod = s.connections.find((c: any) => c.kind === 'modulation');
    expect(mod).toBeTruthy();
    expect(mod.from).toEqual({ module: ids.lfo, port: 'control' });
    expect(mod.to).toEqual({ module: ids.vcf, port: 'cutoff' });
    expect(Math.abs(mod.amount)).toBeGreaterThan(0);

    // The modulated knob must advertise itself as modulated.
    await expect(knob(page, ids.vcf, 'cutoff')).toHaveAttribute('data-mod-count', '1');

    // Play a note and watch the cutoff knob's live marker move.
    await page.evaluate(() => window.__wirescapeTest.load(window.__wirescapeTest.state()));
    await settle(page);
    await page.mouse.move(960, 540);
    await page.keyboard.down('a');
    const angles: number[] = [];
    for (let i = 0; i < 12; i++) {
      angles.push(await markerAngle(page, ids.vcf, 'cutoff'));
      await page.waitForTimeout(120);
    }
    await page.keyboard.up('a');
    const spread = Math.max(...angles) - Math.min(...angles);
    expect(angles.every(Number.isFinite)).toBe(true);
    expect(spread, `marker angles: ${angles.map(a => a.toFixed(1)).join(', ')}`).toBeGreaterThan(2);

    // Save, reload the page, and the restored patch must match.
    const saved = await state(page);
    await page.evaluate(() => window.__wirescapeTest.autosaveNow());
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    await settle(page);
    const restored = await state(page);
    expect(withoutView(restored)).toEqual(withoutView(saved));
  });

  for (const zoom of [0.5, 1, 1.85]) {
    test(`patching works at ${Math.round(zoom * 100)}% zoom`, async ({ page }) => {
      await boot(page);
      // Two narrow modules, so the pair still fits on screen at 185%.
      await page.evaluate(() => window.__wirescapeTest.zoomTo(0.5));
      const noise = await addModuleAt(page, 'essentials.noise', 60, 160);
      const vcf = await addModuleAt(page, 'essentials.vcf', 470, 160);

      await page.evaluate(z => {
        const t = window.__wirescapeTest;
        t.zoomTo(z);
        t.canvas.viewport.setView(20, 20, z);
      }, zoom);
      await settle(page);

      // The same audio output drives an input and modulates a knob: hints
      // set colour only, they never block a connection.
      await patch(page, { module: noise, port: 'audio' }, { module: vcf, port: 'audio' });
      await patchToKnob(page, { module: noise, port: 'audio' }, { module: vcf, param: 'cutoff' });

      const s = await state(page);
      expect(s.connections.filter((c: any) => c.kind === 'signal')).toHaveLength(1);
      expect(s.connections.filter((c: any) => c.kind === 'modulation')).toHaveLength(1);
      expect(Math.round(s.ui.view.zoom * 100)).toBe(Math.round(zoom * 100));
    });
  }
});

test.describe('cables', () => {
  test('refuses an event output into a signal input', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const keyboard = await addModuleAt(page, 'host.keyboard', 80, 200);
    const vco = await addModuleAt(page, 'essentials.vco', 800, 200);

    await patch(page, { module: keyboard, port: 'notes' }, { module: vco, port: 'pitch' });
    const s = await state(page);
    expect(s.connections).toHaveLength(0);
    await expect(page.getByTestId('toast')).toContainText(/event output cannot drive a signal input/i);
  });

  test('an event output does connect to an event input', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const keyboard = await addModuleAt(page, 'host.keyboard', 80, 200);
    const vco = await addModuleAt(page, 'essentials.vco', 800, 200);
    await patch(page, { module: keyboard, port: 'notes' }, { module: vco, port: 'notes' });
    const s = await state(page);
    expect(s.connections).toHaveLength(1);
    expect(s.connections[0].kind).toBe('event');
  });

  test('a second cable replaces the first, and shift-drop adds alongside', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.55));
    const a = await addModuleAt(page, 'essentials.vco', 60, 160);
    const b = await addModuleAt(page, 'essentials.noise', 60, 700);
    const mix = await addModuleAt(page, 'essentials.vcf', 900, 400);

    await patch(page, { module: a, port: 'audio' }, { module: mix, port: 'audio' });
    expect((await state(page)).connections).toHaveLength(1);

    await patch(page, { module: b, port: 'audio' }, { module: mix, port: 'audio' });
    let s = await state(page);
    expect(s.connections, 'the default is replace').toHaveLength(1);
    expect(s.connections[0].from.module).toBe(b);

    await patch(page, { module: a, port: 'audio' }, { module: mix, port: 'audio' }, { shift: true });
    s = await state(page);
    expect(s.connections, 'shift-drop adds').toHaveLength(2);
  });

  test('selecting a cable and pressing Delete removes it', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const vco = await addModuleAt(page, 'essentials.vco', 60, 200);
    const vcf = await addModuleAt(page, 'essentials.vcf', 900, 200);
    await patch(page, { module: vco, port: 'audio' }, { module: vcf, port: 'audio' });
    expect((await state(page)).connections).toHaveLength(1);

    // Click a point on the curve itself (the bounding-box centre of a sagging cable is off the path).
    const mid = await page.evaluate(() => {
      const p = document.querySelector<SVGPathElement>('.cable-hit')!;
      const q = p.getPointAtLength(p.getTotalLength() / 2).matrixTransform(p.getScreenCTM()!);
      return { x: q.x, y: q.y };
    });
    await page.mouse.click(mid.x, mid.y);
    await settle(page);
    await page.keyboard.press('Delete');
    await settle(page);
    expect((await state(page)).connections).toHaveLength(0);
  });

  test('a cable dropped on empty canvas opens a filtered quick-add', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.7));
    const vco = await addModuleAt(page, 'essentials.vco', 100, 200);

    const a = await port(page, vco, 'output', 'audio').boundingBox();
    await page.mouse.move(a!.x + a!.width / 2, a!.y + a!.height / 2);
    await page.mouse.down();
    await page.mouse.move(a!.x + 320, a!.y + 260, { steps: 8 });
    await page.mouse.up();
    await settle(page);

    const quick = page.getByTestId('quick-add');
    await expect(quick).toBeVisible();
    // Only modules with a signal input are offered; the LFO has no inputs.
    await expect(page.getByTestId('quick-add-essentials.lfo')).toHaveCount(0);
    await expect(page.getByTestId('quick-add-essentials.vcf')).toBeVisible();

    await page.getByTestId('quick-add-essentials.vcf').click();
    await settle(page);
    const s = await state(page);
    expect(s.modules).toHaveLength(2);
    expect(s.connections, 'the dropped cable completes itself').toHaveLength(1);
  });
});

test.describe('the catalogue', () => {
  test('every module renders with working controls', async ({ page }) => {
    await boot(page);
    const report = await page.evaluate(async () => {
      const t = window.__wirescapeTest;
      const out: { id: string; knobs: number; controls: number; ports: number; descriptorPorts: number; params: number }[] = [];
      for (const m of t.registry.all) {
        t.load({
          format: 'wirescape.native.patch', version: 2,
          modules: [{ id: 1, module: m.id }], connections: [], ui: { positions: { 1: [0, 0] } },
        }, 'probe');
        await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
        const card = document.querySelector('.module');
        out.push({
          id: m.id,
          knobs: card?.querySelectorAll('.knob-unit').length ?? 0,
          controls: card?.querySelectorAll('.knob-unit,.segmented,.toggle,.input-select,.control-readout,.control-fader,.control-number,.groove-pane button').length ?? 0,
          ports: card?.querySelectorAll('.port').length ?? 0,
          descriptorPorts: m.ports.length,
          params: m.parameters.length,
        });
      }
      return out;
    });

    // Known IDs may only grow (#687); adding a module needs no test change.
    const catalogueIds: string[] = JSON.parse(readFileSync(resolve('public/wirescape_modules.json'), 'utf8'))
      .modules.map((m: { id: string }) => m.id);
    const knownIds: string[] = readFileSync(resolve('../../tools/foundation/fixtures/known_module_ids.txt'), 'utf8')
      .split('\n').map((line: string) => line.trim()).filter((line: string) => line && !line.startsWith('#'));
    const reportIds = report.map(r => r.id);
    expect(reportIds).toHaveLength(catalogueIds.length);
    expect(new Set(reportIds).size).toBe(reportIds.length);
    expect(knownIds.filter(id => !reportIds.includes(id))).toEqual([]);
    expect(report.some(r => r.id === 'essentials.notes_to_cv')).toBe(true);
    expect(report.some(r => r.id === 'essentials.chord')).toBe(true);
    expect(report.some(r => r.id === 'essentials.quantizer')).toBe(true);
    expect(report.find(r => r.id === 'essentials.quad_vca')).toMatchObject({ descriptorPorts: 13, params: 9 });
    expect(report.find(r => r.id === 'osc.in')).toMatchObject({ ports: 4, descriptorPorts: 9, params: 0 });
    expect(report.find(r => r.id === 'monitor')).toMatchObject({ ports: 3, descriptorPorts: 3, params: 0 });
    expect(report.some(r => r.id === 'sequencing.groove')).toBe(true);
    expect(report.some(r => r.id === 'fx.delay')).toBe(true);
    expect(report.some(r => r.id === 'fx.reverb')).toBe(true);
    for (const r of report) {
      expect(r.ports, `${r.id} should render its ports`).toBeGreaterThan(0);
      if (r.params > 0) {
        expect(r.controls, `${r.id} should render controls for its parameters`).toBeGreaterThan(0);
      }
    }
  });

  test('Notes→CV patch round-trips voices and poly connections', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.5));
    const keyboard = await addModuleAt(page, 'host.keyboard', 80, 220);
    const notes = await addModuleAt(page, 'essentials.notes_to_cv', 450, 220);
    const vco = await addModuleAt(page, 'essentials.vco', 820, 220);
    const adsr = await addModuleAt(page, 'essentials.adsr', 820, 700);
    const vca = await addModuleAt(page, 'essentials.vca', 1190, 220);
    const output = await addModuleAt(page, 'essentials.output', 1560, 220);

    await patch(page, { module: keyboard, port: 'notes' }, { module: notes, port: 'notes' });
    await patch(page, { module: notes, port: 'pitch' }, { module: vco, port: 'pitch' });
    await patch(page, { module: notes, port: 'gate' }, { module: adsr, port: 'gate' });
    await patch(page, { module: vco, port: 'audio' }, { module: vca, port: 'audio' });
    await patch(page, { module: adsr, port: 'envelope' }, { module: vca, port: 'gain_cv' });
    await patch(page, { module: vca, port: 'audio' }, { module: output, port: 'audio' });
    await page.evaluate(id => window.__wirescapeTest.commitParam(id, 'voices', 3), notes);
    await settle(page);

    const saved = await state(page);
    const savedModule = saved.modules.find((m: any) => m.id === notes);
    expect(savedModule).toMatchObject({ module: 'essentials.notes_to_cv', parameters: { voices: 3 } });
    const endpoints = saved.connections.map((c: any) =>
      [c.kind, c.from.module, c.from.port, c.to.module, c.to.port]);
    expect(endpoints).toEqual(expect.arrayContaining([
      ['event', keyboard, 'notes', notes, 'notes'],
      ['signal', notes, 'pitch', vco, 'pitch'],
      ['signal', notes, 'gate', adsr, 'gate'],
    ]));

    await page.evaluate(() => window.__wirescapeTest.autosaveNow());
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    await settle(page);
    const restored = await state(page);
    expect(restored.modules.find((m: any) => m.id === notes))
      .toMatchObject({ module: 'essentials.notes_to_cv', parameters: { voices: 3 } });
    expect(restored.connections).toEqual(saved.connections);
  });

  test('no module overflows its card, in either hit profile', async ({ page }) => {
    await boot(page);
    for (const hit of ['mouse', 'touch'] as const) {
      await page.evaluate(h => window.__wirescapeTest.setPref({ hit: h }), hit);
      const overflowing = await page.evaluate(async () => {
        const t = window.__wirescapeTest;
        const bad: string[] = [];
        for (const m of t.registry.all) {
          t.load({
            format: 'wirescape.native.patch', version: 2,
            modules: [{ id: 1, module: m.id }], connections: [], ui: { positions: { 1: [0, 0] } },
          }, 'probe');
          await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
          const card = document.querySelector('.module')!;
          const body = card.querySelector<HTMLElement>('.module-body')!;
          const core = card.querySelector<HTMLElement>('.panel-core')!;
          const ports = [...card.querySelectorAll<HTMLElement>('.port')];
          const lastPort = ports.length
            ? Math.max(...ports.map(e => e.offsetTop + e.offsetHeight)) : 0;
          const over = core.scrollHeight - body.clientHeight;
          const portOver = lastPort - body.clientHeight;
          if (over > 2 || portOver > 2) bad.push(`${m.id} core+${Math.round(over)} ports+${Math.round(portOver)}`);
        }
        return bad;
      });
      expect(overflowing, `overflow in the ${hit} profile`).toEqual([]);
    }
    await page.evaluate(() => window.__wirescapeTest.setPref({ hit: 'mouse' }));
  });

  test('an unknown module and unknown fields survive load then save', async ({ page }) => {
    await boot(page);
    const original = {
      format: 'wirescape.native.patch', version: 2,
      vendorExtension: { author: 'khalid', seed: 42 },
      modules: [
        { id: 1, module: 'essentials.vco', parameters: { frequency: 330 }, futureField: ['a', 'b'] },
        { id: 2, module: 'vendor.mystery_filter', parameters: { bite: 0.7 }, note: 'not in the catalogue' },
        { id: 3, module: 'essentials.output' },
      ],
      connections: [
        { kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' }, latencyHint: 3 },
        { kind: 'signal', from: { module: 2, port: 'audio' }, to: { module: 3, port: 'audio' } },
      ],
      ui: { positions: { 1: [40, 60], 2: [600, 60], 3: [1200, 60] }, labels: {}, view: { x: 0, y: 0, zoom: 1 }, swatch: 'rust' },
    };
    await page.evaluate(p => window.__wirescapeTest.load(p), original);
    await settle(page);

    await expect(page.getByTestId('module-2')).toHaveClass(/missing/);
    expect(await page.getByTestId('module-2').locator('.port').count(),
      'the missing card keeps the ports its cables reference').toBeGreaterThan(0);

    const saved = await state(page);
    expect(saved.vendorExtension).toEqual({ author: 'khalid', seed: 42 });
    expect(saved.ui.swatch).toBe('rust');
    expect(saved.modules.find((m: any) => m.id === 1).futureField).toEqual(['a', 'b']);
    expect(saved.modules.find((m: any) => m.id === 2)).toMatchObject({
      module: 'vendor.mystery_filter', parameters: { bite: 0.7 }, note: 'not in the catalogue',
    });
    expect(saved.connections[0].latencyHint).toBe(3);

    // The unknown module must survive a delete attempt of something else too.
    await page.getByTestId('module-1').click();
    await page.keyboard.press('Delete');
    await settle(page);
    expect((await state(page)).modules.find((m: any) => m.id === 2)).toBeTruthy();
  });
});

test.describe('controls', () => {
  test('a knob responds to drag, arrows, wheel and double-click', async ({ page }) => {
    await boot(page);
    const vcf = await addModuleAt(page, 'essentials.vcf', 200, 200);
    const dial = knob(page, vcf, 'cutoff').locator('.knob');
    const read = () => page.evaluate(
      id => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters.cutoff, vcf);

    const start = await read();
    const box = (await dial.boundingBox())!;
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2 - 60, { steps: 6 });
    await page.mouse.up();
    await settle(page);
    const dragged = await read();
    expect(dragged).toBeGreaterThan(start);

    await dial.focus();
    await page.keyboard.press('ArrowUp');
    await settle(page);
    expect(await read()).toBeGreaterThan(dragged);

    await dial.dblclick();
    await settle(page);
    expect(await read()).toBeCloseTo(1000, 3);
  });

  test('clicking the readout types a value', async ({ page }) => {
    await boot(page);
    const vcf = await addModuleAt(page, 'essentials.vcf', 200, 200);
    await page.getByTestId(`readout-${vcf}-cutoff`).click();
    const input = page.getByTestId(`readout-input-${vcf}-cutoff`);
    await expect(input).toBeVisible();
    await input.fill('4200');
    await input.press('Enter');
    await settle(page);
    const v = await page.evaluate(
      id => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters.cutoff, vcf);
    expect(v).toBeCloseTo(4200, 3);
  });

  test('a log-taper knob travels logarithmically', async ({ page }) => {
    await boot(page);
    const vcf = await addModuleAt(page, 'essentials.vcf', 200, 200);
    const dial = knob(page, vcf, 'cutoff').locator('.knob');
    await dial.focus();
    await page.keyboard.press('Home');            // 20 Hz, the minimum
    for (let i = 0; i < 5; i++) await page.keyboard.press('PageUp');  // +0.1 travel each
    await settle(page);
    const mid = await page.evaluate(
      id => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters.cutoff, vcf);
    // Half travel on a 20 Hz - 20 kHz log taper is ~632 Hz, not 10 kHz.
    expect(mid).toBeGreaterThan(400);
    expect(mid).toBeLessThan(900);
  });

  test('every interactive element carries a data-testid', async ({ page }) => {
    await boot(page);
    await addModuleAt(page, 'essentials.lfo', 120, 160);
    await addModuleAt(page, 'essentials.output', 760, 160);
    await settle(page);
    const missing = await page.evaluate(() => {
      const interactive = [...document.querySelectorAll<HTMLElement>(
        'button, input, select, [role="slider"], .port, .cable-hit')];
      return interactive
        .filter(e => !e.closest('[data-testid]') && !e.dataset.testid)
        .map(e => e.className || e.tagName);
    });
    expect(missing).toEqual([]);
  });
});

test.describe('canvas', () => {
  test('pans, zooms on the pointer, and fits', async ({ page }) => {
    await boot(page);
    await addModuleAt(page, 'essentials.vco', 300, 300);
    const before = await page.evaluate(() => window.__wirescapeTest.state().ui.view);

    await page.mouse.move(1500, 900);
    await page.mouse.down();
    await page.mouse.move(1300, 800, { steps: 6 });
    await page.mouse.up();
    await settle(page);
    const panned = await page.evaluate(() => window.__wirescapeTest.state().ui.view);
    expect(panned.x).not.toBe(before.x);

    await page.mouse.move(900, 500);
    await page.mouse.wheel(0, -240);
    await settle(page);
    const zoomed = await page.evaluate(() => window.__wirescapeTest.state().ui.view);
    expect(zoomed.zoom).toBeGreaterThan(panned.zoom);

    await page.getByTestId('zoom-fit').click();
    await settle(page);
    const fitted = await page.evaluate(() => window.__wirescapeTest.state().ui.view);
    expect(fitted.zoom).toBeGreaterThan(0.24);
    expect(fitted.zoom).toBeLessThanOrEqual(1.86);
  });

  test('shift-drag box-selects and Delete removes the group', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.5));
    await addModuleAt(page, 'essentials.vco', 120, 200);
    await addModuleAt(page, 'essentials.vcf', 120, 700);
    await settle(page);

    await page.keyboard.down('Shift');
    await page.mouse.move(300, 120);   // inside the canvas, clear of both cards
    await page.mouse.down();
    await page.mouse.move(1500, 950, { steps: 10 });
    await page.mouse.up();
    await page.keyboard.up('Shift');
    await settle(page);

    expect(await page.locator('.module.selected').count()).toBe(2);
    await page.keyboard.press('Delete');
    await settle(page);
    expect((await state(page)).modules).toHaveLength(0);
    await expect(page.getByTestId('empty-state')).toBeVisible();
  });

  test('double-clicking empty canvas opens quick-add', async ({ page }) => {
    await boot(page);
    await page.mouse.dblclick(1100, 600);
    await settle(page);
    await expect(page.getByTestId('quick-add')).toBeVisible();
    await page.getByTestId('quick-add-input').fill('ladder');
    await page.getByTestId('quick-add-essentials.ladder').click();
    await settle(page);
    expect((await state(page)).modules).toHaveLength(1);
  });

  test('dragging a module keeps its cables attached', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const vco = await addModuleAt(page, 'essentials.vco', 80, 200);
    const vcf = await addModuleAt(page, 'essentials.vcf', 900, 200);
    await patch(page, { module: vco, port: 'audio' }, { module: vcf, port: 'audio' });

    const pathBefore = await page.locator('.cable').first().getAttribute('d');
    await dragModuleTo(page, vcf, 1000, 520);
    const pathAfter = await page.locator('.cable').first().getAttribute('d');
    expect(pathAfter).not.toBe(pathBefore);
    expect((await state(page)).connections).toHaveLength(1);
  });
});

test.describe('the engine', () => {
  test('rolls an edit back and explains when the engine refuses', async ({ page }) => {
    await boot(page);
    const vcf = await addModuleAt(page, 'essentials.vcf', 200, 200);
    await page.evaluate(() => { window.__wirescapeTest.engine.debug.refuseEdits = true; });

    const before = await page.evaluate(
      id => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters.cutoff, vcf);
    await knob(page, vcf, 'cutoff').locator('.knob').focus();
    await page.keyboard.press('ArrowUp');
    await settle(page);

    await expect(page.getByTestId('toast')).toContainText(/refused/i);
    const after = await page.evaluate(
      id => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters.cutoff, vcf);
    expect(after).toBe(before);
  });

  test('reports audio status and reacts to an unplugged device', async ({ page }) => {
    await boot(page);
    await expect(page.getByTestId('status-audio')).toHaveText(/RUNNING/, { timeout: 8000 });
    await page.evaluate(() => { window.__wirescapeTest.engine.debug.unplugAudio = true; });
    await expect(page.getByTestId('status-audio')).toHaveText(/DEVICE LOST|NO DEVICE/, { timeout: 8000 });
    await page.evaluate(() => { window.__wirescapeTest.engine.debug.unplugAudio = false; });
    await expect(page.getByTestId('status-audio')).toHaveText(/RUNNING|RECONNECTING/, { timeout: 8000 });
  });

  test('meters and the scope move while a note is held', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => {
      const t = window.__wirescapeTest;
      t.load({
        format: 'wirescape.native.patch', version: 2,
        modules: [
          { id: 1, module: 'essentials.vco' },
          { id: 2, module: 'essentials.output' },
        ],
        connections: [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }],
        ui: { positions: { 1: [60, 120], 2: [700, 120] } },
      }, 'meters');
    });
    await settle(page);
    const samples = await page.evaluate(async () => {
      const t = window.__wirescapeTest;
      const seen: number[] = [];
      await new Promise<void>(resolve => {
        const off = t.engine.onTelemetry((tel: any) => {
          const m = tel.meters[2];
          if (m) seen.push(m[0]);
          if (seen.length >= 10) { off(); resolve(); }
        });
      });
      return seen;
    });
    expect(samples.length).toBeGreaterThanOrEqual(10);
    expect(Math.max(...samples)).toBeGreaterThan(0);
  });
});
