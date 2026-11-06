import { test, expect } from '@playwright/test';
import { boot, settle, state } from './helpers';

const load = async (page: import('@playwright/test').Page) => {
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'essentials.noise', parameters: { level: 0.8 } },
      { id: 2, module: 'essentials.mixer', parameters: { level_a: 1, level_b: 0 } },
      { id: 3, module: 'essentials.output' },
    ],
    connections: [
      { kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'a' } },
      { kind: 'signal', from: { module: 2, port: 'audio' }, to: { module: 3, port: 'audio' } },
    ],
    ui: { positions: { '1': [30, 30], '2': [400, 30], '3': [850, 30] } },
  }, 'Channel test'));
  await page.evaluate(() => window.__wirescapeTest.fit());
  await settle(page);
};

test('mixer channel meters show each post-gain input and shared faders remain editable', async ({ page }) => {
  await boot(page); await load(page);
  const meter = page.getByTestId('meter-2');
  await expect(meter.locator('.meter-lab')).toHaveText(['A', 'B']);
  await expect.poll(async () => meter.locator('.meter-bar').first().evaluate(e => e.getAttribute('style'))).toMatch(/scaleX\(0\.8\)/);
  await expect(meter.locator('.meter-bar').nth(1)).toHaveAttribute('style', /scaleX\(0\)/);
  const fader = page.getByTestId('fader-thumb-2-level_a');
  await fader.focus(); await fader.press('Home');
  expect((await state(page)).modules.find((m: any) => m.id === 2).parameters.level_a).toBe(0);
  await expect.poll(async () => meter.locator('.meter-bar').first().evaluate(e => e.getAttribute('style')), { timeout: 3000 }).toMatch(/scaleX\(0\)/);
});

test('mixer and output displays stay inside their panel at mouse and touch density', async ({ page }) => {
  await boot(page); await load(page);
  for (const hit of ['mouse', 'touch']) {
    await page.evaluate(h => window.__wirescapeTest.setPref({ hit: h }), hit);
    await settle(page);
    for (const id of [2, 3]) {
      const overflow = await page.getByTestId(`module-${id}`).evaluate(card => {
        const panel = card.querySelector('.panel-core') as HTMLElement;
        const bounds = panel.getBoundingClientRect();
        return [...panel.querySelectorAll('.meter-pane, .scope-pane, .control-fader')].some(el => {
          const b = el.getBoundingClientRect();
          return b.left < bounds.left - 1 || b.right > bounds.right + 1;
        });
      });
      expect(overflow).toBe(false);
    }
  }
});
