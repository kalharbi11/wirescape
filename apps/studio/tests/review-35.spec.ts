import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state, port, knob, patchToKnob, centre } from './helpers';

/** Owner review of phase 3.5: parameter nodes, mapping, collapsed rails, cable leads. */

const load = async (page: Page, mods: [number, string, number, number][], connections: any[] = []) => {
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

const paramPort = (page: Page, module: number, param: string) => page.getByTestId(`param-port-${module}-${param}`);

async function drag(page: Page, a: { x: number; y: number }, b: { x: number; y: number }) {
  await page.mouse.move(a.x, a.y);
  await page.mouse.down();
  await page.mouse.move((a.x + b.x) / 2, (a.y + b.y) / 2 - 30, { steps: 8 });
  await page.mouse.move(b.x, b.y, { steps: 8 });
  await page.mouse.up();
  await settle(page);
}

test.describe('3.5 review', () => {
  test('essential parameters have nodes; a cable dropped on one is a modulation', async ({ page }) => {
    await boot(page);
    await load(page, [[1, 'essentials.lfo', 60, 80], [2, 'essentials.vcf', 700, 80]]);
    await expect(paramPort(page, 2, 'cutoff')).toBeVisible();
    await expect(paramPort(page, 2, 'resonance')).toBeVisible();
    const a = await port(page, 1, 'output', 'control').boundingBox();
    const b = await paramPort(page, 2, 'cutoff').locator('.port-jack').boundingBox();
    await drag(page, centre(a!), centre(b!));
    const mods = (await state(page)).connections.filter((c: any) => c.kind === 'modulation');
    expect(mods).toHaveLength(1);
    expect(mods[0].to).toEqual({ module: 2, port: 'cutoff' });
  });

  test('a parameter gets a node when mapped and loses it with its last cable', async ({ page }) => {
    await boot(page);
    await load(page, [[1, 'essentials.lfo', 60, 80], [2, 'essentials.lfo', 700, 80]]);
    await expect(paramPort(page, 2, 'offset')).toHaveCount(0);
    await patchToKnob(page, { module: 1, port: 'control' }, { module: 2, param: 'offset' });
    await expect(paramPort(page, 2, 'offset')).toBeVisible();
    // The cable now lands on the node, not on the knob.
    const end = await page.evaluate(() => {
      const d = document.querySelector('.cable')!.getAttribute('d')!;
      const n = d.match(/-?\d+(\.\d+)?/g)!.map(Number);
      return { x: n[n.length - 2], y: n[n.length - 1] };
    });
    const node = await page.evaluate(() => {
      const card = document.querySelector<HTMLElement>('[data-testid="module-2"]')!;
      const el = document.querySelector<HTMLElement>('[data-testid="param-port-2-offset"]')!;
      return { x: card.offsetLeft, y: card.offsetTop + 36 + parseFloat(el.style.top) };
    });
    expect(Math.abs(end.x - node.x)).toBeLessThan(1.5);
    expect(Math.abs(end.y - node.y)).toBeLessThan(1.5);
    await page.evaluate(() => {
      const s = window.__wirescapeTest.state();
      window.__wirescapeTest.load({ ...s, connections: [] }, 'Test');
    });
    await settle(page);
    await expect(paramPort(page, 2, 'offset')).toHaveCount(0);
  });

  test('right-click an output, Map, click a knob', async ({ page }) => {
    await boot(page);
    await load(page, [[1, 'essentials.lfo', 60, 80], [2, 'essentials.vcf', 700, 80]]);
    await port(page, 1, 'output', 'control').click({ button: 'right' });
    await page.getByTestId('context-map').click();
    await expect(knob(page, 2, 'resonance')).toHaveClass(/map-target/);
    await knob(page, 2, 'resonance').locator('.dial-wrap').click();
    const mods = (await state(page)).connections.filter((c: any) => c.kind === 'modulation');
    expect(mods.map((c: any) => c.to.port)).toEqual(['resonance']);
    await expect(page.locator('.map-target')).toHaveCount(0);
  });

  test('a busy rail collapses to four with + / −, and CC numbers are typed', async ({ page }) => {
    await boot(page);
    await load(page, [[1, 'midi.cc_to_cv', 100, 80]]);
    await expect(page.locator('[data-testid^="port-1-output-"]')).toHaveCount(4);
    await page.getByTestId('ports-more-1-output').dispatchEvent('pointerdown');
    await expect(page.locator('[data-testid^="port-1-output-"]')).toHaveCount(5);
    const lab = port(page, 1, 'output', 'cc1').locator('.cc-label');
    await lab.dispatchEvent('pointerdown');
    await page.locator('.cc-input').fill('74');
    await page.keyboard.press('Enter');
    await expect(lab).toHaveText('CC 74');
    const ui = (await state(page)).ui;
    expect(ui.ccMap['1'].cc1).toBe(74);
    expect(ui.portsShown['1'].output).toBe(5);
  });

  test('cables leave and enter their nodes near level, as one smooth curve', async ({ page }) => {
    await boot(page);
    await load(page, [[1, 'essentials.vco', 600, 400], [2, 'essentials.vcf', 80, 60]],
      [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }]);
    const d = await page.locator('.cable').first().getAttribute('d');
    const n = d!.match(/-?\d+(\.\d+)?/g)!.map(Number);
    // M ax,ay C c1x,c1y c2x,c2y bx,by — one smooth cubic, near level at both nodes.
    expect(n).toHaveLength(8);
    const [ax, ay, c1x, c1y, c2x, c2y, bx, by] = n as [number, number, number, number, number, number, number, number];
    expect(c1x).toBeGreaterThan(ax);   // leaves the output to the right
    expect(c2x).toBeLessThan(bx);      // enters the input from the left
    const deg = (x: number, y: number) => Math.abs(Math.atan2(y, x) * 180 / Math.PI);
    expect(deg(c1x - ax, c1y - ay)).toBeLessThan(42);
    expect(deg(bx - c2x, by - c2y)).toBeLessThan(42);
  });
});
