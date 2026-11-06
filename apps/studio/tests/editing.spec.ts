import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state, knob, centre } from './helpers';

/** Phase 4: undo/redo, copy/paste, typed values, and breaking it on purpose. */

const api = <T>(page: Page, fn: string, ...args: unknown[]) =>
  page.evaluate(({ fn, args }) => (window.__wirescapeTest as any)[fn](...args), { fn, args }) as Promise<T>;

/** The patch without the view (what undo is meant to restore). */
const body = async (page: Page) => {
  const s = await state(page);
  const { view: _v, ...ui } = s.ui ?? {};
  return JSON.stringify({ ...s, ui });
};

const flush = (page: Page) => api(page, 'flushHistory');
const ids = async (page: Page) => (await state(page)).modules.map((m: any) => m.id) as number[];
const param = async (page: Page, id: number, p: string) =>
  (await state(page)).modules.find((m: any) => m.id === id).parameters[p] as number;

async function buildVoice(page: Page) {
  const steps: string[] = [await body(page)];
  for (const t of ['essentials.vco', 'essentials.vcf', 'essentials.output']) {
    await api(page, 'addModule', t); await settle(page); await flush(page);
    steps.push(await body(page));
  }
  const [vco, vcf, out] = await ids(page);
  await page.evaluate(({ vco, vcf, out }) => {
    const d = (window.__wirescapeTest as any).doc;
    d.setPosition(vco, 60, 60); d.setPosition(vcf, 520, 60); d.setPosition(out, 1000, 60);
  }, { vco, vcf, out });
  await api(page, 'zoomTo', 1);
  await settle(page);
  await api(page, 'connect', { kind: 'signal', from: { module: vco, port: 'audio' }, to: { module: vcf, port: 'audio' } });
  await api(page, 'connect', { kind: 'signal', from: { module: vcf, port: 'low24' }, to: { module: out, port: 'audio' } });
  await flush(page);
  steps.push(await body(page));
  return { steps, vco: vco!, vcf: vcf!, out: out! };
}

test.describe('editing', () => {
  test('undo walks back to an empty canvas and redo rebuilds the patch exactly', async ({ page }) => {
    await boot(page);
    const { steps } = await buildVoice(page);
    for (let i = steps.length - 2; i >= 0; i--) {
      await api(page, 'undo'); await settle(page);
      expect(await body(page)).toBe(steps[i]);
    }
    expect(await ids(page)).toHaveLength(0);
    await expect(page.getByTestId('undo')).toBeDisabled();
    for (let i = 1; i < steps.length; i++) {
      await api(page, 'redo'); await settle(page);
      expect(await body(page)).toBe(steps[i]);
    }
    await expect(page.getByTestId('redo')).toBeDisabled();
  });

  test('one knob drag is one undo step', async ({ page }) => {
    await boot(page);
    const { vcf } = await buildVoice(page);
    const before = await param(page, vcf, 'cutoff');
    const depth = (await api<{ past: number }>(page, 'historyDepth')).past;
    const b = await knob(page, vcf, 'cutoff').locator('.dial-wrap').boundingBox();
    const c = centre(b!);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    for (let i = 1; i <= 12; i++) await page.mouse.move(c.x, c.y - i * 6);
    await page.mouse.up();
    await settle(page);
    await flush(page);
    expect(await param(page, vcf, 'cutoff')).not.toBeCloseTo(before, 3);
    expect((await api<{ past: number }>(page, 'historyDepth')).past).toBe(depth + 1);
    await page.keyboard.press('Control+z');
    await settle(page);
    expect(await param(page, vcf, 'cutoff')).toBeCloseTo(before, 6);
    await page.keyboard.press('Control+Shift+z');
    await settle(page);
    expect(await param(page, vcf, 'cutoff')).not.toBeCloseTo(before, 3);
  });

  test('rapid undo and redo never corrupt the patch', async ({ page }) => {
    await boot(page);
    const { steps } = await buildVoice(page);
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    for (let i = 0; i < 25; i++) await page.keyboard.press('Control+z');
    await settle(page);
    expect(await ids(page)).toHaveLength(0);
    for (let i = 0; i < 25; i++) await page.keyboard.press('Control+y');
    await settle(page);
    expect(await body(page)).toBe(steps.at(-1));
    for (let i = 0; i < 10; i++) { await page.keyboard.press('Control+z'); await page.keyboard.press('Control+Shift+z'); }
    await settle(page);
    expect(await body(page)).toBe(steps.at(-1));
    expect(errors).toEqual([]);
    // The canvas still matches the document.
    expect(await page.locator('[data-testid^="module-header-"]').count()).toBe(3);
    expect(await page.locator('.cable-group').count()).toBe(2);
  });

  test('copy and paste keeps inner cables, works across zoom, and pastes into another patch', async ({ page }) => {
    await boot(page);
    const { vco, vcf } = await buildVoice(page);
    await api(page, 'select', { kind: 'modules', ids: [vco, vcf] });
    await page.keyboard.press('Control+c');
    await api(page, 'zoomTo', 0.5); await settle(page);
    await page.keyboard.press('Control+v');
    await settle(page);
    let s = await state(page);
    expect(s.modules).toHaveLength(5);
    const fresh = s.modules.map((m: any) => m.id).filter((i: number) => ![1, 2, 3].includes(i));
    expect(fresh).toHaveLength(2);
    const inner = s.connections.filter((c: any) => fresh.includes(c.from.module) && fresh.includes(c.to.module));
    expect(inner).toHaveLength(1); // VCO→VCF came along; VCF→OUT did not
    // One undo removes the whole paste.
    await flush(page);
    await page.keyboard.press('Control+z'); await settle(page);
    expect((await state(page)).modules).toHaveLength(3);
    // Paste into a new, empty patch.
    await page.getByTestId('new-patch').click(); await settle(page);
    await page.keyboard.press('Control+v'); await settle(page);
    s = await state(page);
    expect(s.modules.map((m: any) => m.module).sort()).toEqual(['essentials.vcf', 'essentials.vco']);
    expect(s.connections).toHaveLength(1);
  });

  test('duplicate cascades instead of stacking', async ({ page }) => {
    await boot(page);
    const { vco } = await buildVoice(page);
    await api(page, 'select', { kind: 'modules', ids: [vco] });
    await page.keyboard.press('Control+d'); await settle(page);
    await page.keyboard.press('Control+d'); await settle(page);
    const s = await state(page);
    const pos = (id: number) => s.ui.positions[String(id)];
    const vcos = s.modules.filter((m: any) => m.module === 'essentials.vco').map((m: any) => m.id);
    expect(vcos).toHaveLength(3);
    const ps = vcos.map(pos);
    expect(new Set(ps.map((p: number[]) => p.join(','))).size).toBe(3);
  });

  test('typed values understand units, notes and percentages', async ({ page }) => {
    await boot(page);
    await api(page, 'load', {
      format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 1, module: 'essentials.vcf' }, { id: 2, module: 'essentials.mixer' }], connections: [],
      ui: { positions: { 1: [80, 80], 2: [600, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'T');
    await settle(page);
    const type = async (mod: number, p: string, text: string) => {
      await page.getByTestId(`readout-${mod}-${p}`).click();
      const input = page.getByTestId(`readout-input-${mod}-${p}`);
      await input.fill(text);
      await input.press('Enter');
      await settle(page);
    };
    await type(1, 'cutoff', '2k');
    expect(await param(page, 1, 'cutoff')).toBeCloseTo(2000, 3);
    await type(1, 'cutoff', 'A4');
    expect(await param(page, 1, 'cutoff')).toBeCloseTo(440, 3);
    await type(1, 'cutoff', '350');
    expect(await param(page, 1, 'cutoff')).toBeCloseTo(350, 3);
    await type(1, 'resonance', '40');
    expect(await param(page, 1, 'resonance')).toBeCloseTo(0.4, 6);
    await type(1, 'resonance', 'nonsense');
    expect(await param(page, 1, 'resonance')).toBeCloseTo(0.4, 6);
  });

  test('a fader value can be typed (OPEN-8)', async ({ page }) => {
    await boot(page);
    await api(page, 'load', {
      format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 1, module: 'essentials.mixer' }], connections: [],
      ui: { positions: { 1: [80, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'T');
    await settle(page);
    const faders = page.locator('.control-fader');
    if (await faders.count() === 0) test.skip(true, 'no fader in this catalogue');
    const id = await faders.first().locator('.fader-value').getAttribute('data-testid');
    const p = id!.split('-').slice(2).join('-');
    await page.getByTestId(id!).click();
    const input = page.getByTestId(`readout-input-1-${p}`);
    await input.fill('70');
    await input.press('Enter');
    await settle(page);
    expect(await param(page, 1, p)).toBeCloseTo(0.7, 6);
  });

  test('a reload in the middle of a drag restores a valid patch', async ({ page }) => {
    await boot(page);
    const { vcf } = await buildVoice(page);
    const b = await knob(page, vcf, 'cutoff').locator('.dial-wrap').boundingBox();
    const c = centre(b!);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    for (let i = 1; i <= 6; i++) await page.mouse.move(c.x, c.y - i * 8);
    await api(page, 'autosaveNow');
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    await settle(page);
    const s = await state(page);
    expect(s.modules).toHaveLength(3);
    expect(s.connections).toHaveLength(2);
    const v = s.modules.find((m: any) => m.id === vcf).parameters.cutoff;
    expect(v).toBeGreaterThanOrEqual(20);
    expect(v).toBeLessThanOrEqual(20000);
    // History starts fresh after a restore: nothing to undo yet.
    await expect(page.getByTestId('undo')).toBeDisabled();
  });
});
