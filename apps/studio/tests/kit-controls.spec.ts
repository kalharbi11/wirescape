import { test, expect, type Page } from '@playwright/test';
import {
  boot, settle, state, patch, patchToKnob, knob, markerAngle, centre,
} from './helpers';

const loadMods = async (page: Page, mods: [number, string, number, number][], connections: any[] = []) => {
  await page.evaluate(({ mods, connections }) => {
    const positions: Record<string, [number, number]> = {};
    for (const [id, , x, y] of mods) positions[id] = [x, y];
    window.__wirescapeTest.load({
      format: 'wirescape.native.patch', version: 2,
      modules: mods.map(([id, module]) => ({ id, module })), connections,
      ui: { positions, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'Test');
  }, { mods, connections });
  await page.evaluate(() => window.__wirescapeTest.zoomTo(1));
  await settle(page);
};

const param = (page: Page, id: number, p: string) =>
  page.evaluate(({ id, p }) => window.__wirescapeTest.state().modules.find((m: any) => m.id === id).parameters[p],
    { id, p });

const cableMid = (page: Page) => page.evaluate(() => {
  const p = document.querySelector<SVGPathElement>('.cable-hit')!;
  const q = p.getPointAtLength(p.getTotalLength() / 2).matrixTransform(p.getScreenCTM()!);
  return { x: q.x, y: q.y };
});

const modAmount = async (page: Page) =>
  (await state(page)).connections.find((c: any) => c.kind === 'modulation').amount as number;

test.describe('kit controls', () => {
  test('1. knob tiers', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vcf', 100, 100], [2, 'essentials.chaos_lite', 700, 100]]);
    await expect(knob(page, 1, 'cutoff')).toHaveClass(/tier-L/);
    await expect(knob(page, 1, 'resonance')).toHaveClass(/tier-M/);
    await expect(knob(page, 2, 'pan_a')).toHaveClass(/tier-S/);
    expect(await knob(page, 1, 'cutoff').locator('svg text.tl').count()).toBeGreaterThan(0);
    expect(await knob(page, 2, 'pan_a').locator('svg text.tl').count()).toBe(0);
  });

  test('2. combined modulation shows one arc, count 2, live text moves', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vcf', 700, 100], [2, 'essentials.lfo', 100, 100],
      [3, 'essentials.lfo', 100, 500]]);
    await patchToKnob(page, { module: 2, port: 'control' }, { module: 1, param: 'cutoff' });
    await patchToKnob(page, { module: 3, port: 'control' }, { module: 1, param: 'cutoff' });
    const k = knob(page, 1, 'cutoff');
    await expect(k).toHaveAttribute('data-mod-count', '2');
    expect(await k.locator('path.mod').count()).toBe(1);
    await expect(k.locator('.knob-live')).toHaveText(/^→ /);
    const texts = new Set<string>(); const angles = new Set<number>();
    for (let i = 0; i < 12; i++) {
      texts.add((await k.locator('.knob-live').textContent()) ?? '');
      angles.add(Math.round((await markerAngle(page, 1, 'cutoff')) * 10));
      await page.waitForTimeout(150);
    }
    expect(texts.size).toBeGreaterThan(1);
    expect(angles.size).toBeGreaterThan(1);
  });

  test('3. modulation amount lives on the cable', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vcf', 700, 100], [2, 'essentials.lfo', 100, 100]]);
    await patchToKnob(page, { module: 2, port: 'control' }, { module: 1, param: 'cutoff' });
    const mid = await cableMid(page);
    await page.mouse.click(mid.x, mid.y);
    await settle(page);
    const amt = page.getByTestId('cable-amount');
    await expect(amt).toBeVisible();
    const a0 = await modAmount(page);
    const b = (await amt.boundingBox())!;
    const c = centre(b);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    await page.mouse.move(c.x + 25, c.y, { steps: 5 });
    await page.mouse.move(c.x + 50, c.y, { steps: 5 });
    await page.mouse.up();
    await settle(page);
    const a1 = await modAmount(page);
    expect(a1).toBeGreaterThan(a0);
    await amt.focus();
    await page.keyboard.press('ArrowLeft');
    await settle(page);
    expect(await modAmount(page)).toBeLessThan(a1);
  });

  test('4. cable colour persists through save/load', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vco', 100, 100], [2, 'essentials.vcf', 700, 100]]);
    await patch(page, { module: 1, port: 'audio' }, { module: 2, port: 'audio' });
    const mid = await cableMid(page);
    await page.mouse.click(mid.x, mid.y);
    await settle(page);
    await page.getByTestId('cable-color-rust').click();
    await settle(page);
    const colors = (await state(page)).ui.cableColors;
    expect(Object.values(colors)).toContain('rust');
    await expect(page.locator('.cable').first()).toHaveAttribute('stroke', 'var(--cable-rust)');
    await page.evaluate(() => {
      const t = window.__wirescapeTest; t.load(t.save());
    });
    await settle(page);
    expect(Object.values((await state(page)).ui.cableColors)).toContain('rust');
    await expect(page.locator('.cable').first()).toHaveAttribute('stroke', 'var(--cable-rust)');
  });

  test('5. cable is dotted while carrying signal', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vco', 100, 100], [2, 'essentials.output', 700, 100]]);
    await patch(page, { module: 1, port: 'audio' }, { module: 2, port: 'audio' });
    await page.evaluate(() => window.__wirescapeTest.noteOn(60));
    await expect(page.locator('.cable').first()).toHaveClass(/active/, { timeout: 2000 });
    await page.evaluate(() => window.__wirescapeTest.noteOff(60));
  });

  test('6. number box drags and types', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.clock', 200, 150]]);
    const nb = page.getByTestId('number-1-bpm');
    await expect(nb).toBeVisible();
    const b0 = await param(page, 1, 'bpm');
    const c = centre((await nb.boundingBox())!);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    await page.mouse.move(c.x + 20, c.y, { steps: 4 });
    await page.mouse.move(c.x + 60, c.y, { steps: 4 });
    await page.mouse.up();
    await settle(page);
    expect(await param(page, 1, 'bpm')).not.toBe(b0);
    await page.mouse.click(c.x, c.y);
    const input = page.getByTestId('readout-input-1-bpm');
    await expect(input).toBeVisible();
    await input.fill('100');
    await input.press('Enter');
    await settle(page);
    expect(await param(page, 1, 'bpm')).toBe(100);
  });

  test('7. fader drags and resets', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.mixer', 200, 100]]);
    await expect(page.getByTestId('fader-1-level_a')).toBeVisible();
    const thumb = page.getByTestId('fader-thumb-1-level_a');
    const start = await param(page, 1, 'level_a') ?? 0;
    const c = centre((await thumb.boundingBox())!);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    await page.mouse.move(c.x, c.y - 20, { steps: 4 });
    await page.mouse.move(c.x, c.y - 40, { steps: 4 });
    await page.mouse.up();
    await settle(page);
    const raised = await param(page, 1, 'level_a');
    expect(raised).toBeGreaterThan(start);
    const def = await page.evaluate(() => window.__wirescapeTest.effective(1, 'level_a'));
    await page.getByTestId('fader-1-level_a').locator('.fader-track').dblclick();
    await settle(page);
    const after = await page.evaluate(() => window.__wirescapeTest.effective(1, 'level_a'));
    expect(after).not.toBe(raised);
    expect(def).toBe(raised);
    const d = await page.evaluate(() => window.__wirescapeTest.registry.get('essentials.mixer')
      .parameters.find((p: any) => p.id === 'level_a').default);
    expect(after).toBeCloseTo(d, 6);
  });

  test('8. segmented control moves its indicator', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'host.keyboard', 200, 100]]);
    const seg = page.getByTestId('segmented-1-mode');
    await expect(seg).toBeVisible();
    const btns = seg.locator('button.seg');
    const n = await btns.count();
    expect(n).toBeGreaterThan(1);
    const onIdx = await btns.evaluateAll(bs => bs.findIndex(b => b.classList.contains('on')));
    const targetIdx = onIdx === 0 ? 1 : 0;
    const target = btns.nth(targetIdx);
    const value = await target.getAttribute('data-value');
    const ind = seg.locator('.seg-ind');
    const t0 = await ind.evaluate(e => getComputedStyle(e).transform);
    const x0 = (await ind.boundingBox())!.x;
    await page.getByTestId(`seg-1-mode-${value}`).click();
    await page.waitForTimeout(500);
    await expect(target).toHaveClass(/on/);
    await expect(target).toHaveAttribute('aria-checked', 'true');
    const t1 = await ind.evaluate(e => getComputedStyle(e).transform);
    const x1 = (await ind.boundingBox())!.x;
    expect(t1 !== t0 || x1 !== x0).toBe(true);
    expect(await param(page, 1, 'mode')).toBe(Number(value));
  });

  test('9. KNOBS/GRAPH switch keeps card height', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vcf', 200, 100]]);
    const card = page.getByTestId('module-1');
    const h0 = await card.evaluate(e => (e as HTMLElement).offsetHeight);
    await expect(knob(page, 1, 'cutoff')).toBeVisible();
    await page.getByTestId('view-1-graph').click();
    await settle(page);
    await expect(knob(page, 1, 'cutoff')).not.toBeVisible();
    await expect(page.getByTestId('filter-1')).toBeVisible();
    expect(await card.evaluate(e => (e as HTMLElement).offsetHeight)).toBe(h0);
    await page.getByTestId('view-1-knobs').click();
    await settle(page);
    await expect(knob(page, 1, 'cutoff')).toBeVisible();
    expect(await card.evaluate(e => (e as HTMLElement).offsetHeight)).toBe(h0);
  });

  test('10. parameter and module info boxes', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'essentials.vcf', 700, 100], [2, 'essentials.lfo', 100, 100]]);
    await patchToKnob(page, { module: 2, port: 'control' }, { module: 1, param: 'cutoff' });
    await page.mouse.move(5, 5);
    const k = knob(page, 1, 'cutoff');
    await k.hover();
    const btn = page.getByTestId('info-1-cutoff');
    await expect(btn).toBeVisible();
    await btn.hover();
    const box = page.getByTestId('info-1-cutoff-box');
    await expect(box).toBeVisible();
    await expect(box).toContainText(/lfo/i);
    await btn.click();
    await page.mouse.move(5, 5);
    await settle(page);
    await expect(box).toBeVisible();
    await page.keyboard.press('Escape');
    await expect(box).not.toBeVisible();

    await page.getByTestId('module-info-1').click();
    const mbox = page.getByTestId('module-info-1-box');
    await expect(mbox).toBeVisible();
    await expect(mbox).toContainText(/[←→]/);
  });

  test('11. ports sit on the side rails', async ({ page }) => {
    await boot(page);
    await loadMods(page, [[1, 'ports.macro_oscillator', 300, 100]]);
    const card = (await page.getByTestId('module-1').boundingBox())!;
    const ins = await page.locator('[data-testid^="port-1-input-"]').all();
    const outs = await page.locator('[data-testid^="port-1-output-"]').all();
    expect(ins.length).toBeGreaterThan(2);
    const ci: { x: number; y: number }[] = [];
    for (const p of ins) ci.push(centre((await p.locator('.port-jack').boundingBox())!));
    for (const c of ci) expect(Math.abs(c.x - card.x)).toBeLessThanOrEqual(2);
    ci.sort((a, b) => a.y - b.y);
    const gaps = ci.slice(1).map((c, i) => c.y - ci[i].y);
    for (const g of gaps) expect(Math.abs(g - gaps[0])).toBeLessThanOrEqual(2);
    expect(outs.length).toBeGreaterThan(0);
    for (const p of outs) {
      const c = centre((await p.locator('.port-jack').boundingBox())!);
      expect(Math.abs(c.x - (card.x + card.width))).toBeLessThanOrEqual(2);
    }
  });

  test('12. motion on add and delete', async ({ page }) => {
    await boot(page);
    await page.getByTestId('library-item-essentials.vco').click();
    const n = await page.evaluate(() => {
      const cards = document.querySelectorAll<HTMLElement>('[data-testid^="module-"]:not([data-testid^="module-header"]):not([data-testid^="module-title"]):not([data-testid^="module-info"])');
      return cards[cards.length - 1].getAnimations().length;
    });
    expect(n).toBeGreaterThan(0);
    await settle(page, 30);
    const id = (await state(page)).modules[0].id;
    const card = page.getByTestId(`module-${id}`);
    await card.locator('[data-testid^="module-header"]').click();
    await page.keyboard.press('Delete');
    await expect(card).toHaveCount(0, { timeout: 400 });
  });
});
