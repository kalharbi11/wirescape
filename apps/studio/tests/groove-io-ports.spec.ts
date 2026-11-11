// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';

const patch = {
  format: 'wirescape.native.patch', version: 2,
  modules: [
    { id: 1, module: 'sequencing.groove', version: 2,
      config: { version: 2, bpm: 120, tracks: [{
        length: 8, steps: [{ kind: 'note', key: 60, velocity: 0.9 }],
      }] } },
    { id: 2, module: 'essentials.output', version: 2 },
  ],
  connections: [],
  ui: { positions: { '1': [20, 20], '2': [1400, 20] } },
};

async function checkAnchor(page: Page) {
  const geometry = await page.evaluate(() => {
    const card = document.querySelector<HTMLElement>('[data-testid="module-1"]')!;
    const output = card.querySelector<HTMLElement>('[data-testid="port-1-output-t1_pitch"]')!;
    const c = card.getBoundingClientRect(), p = output.getBoundingClientRect();
    const canvas = (window as any).__wirescapeTest.canvas;
    const anchor = canvas.cardFor(1)?.anchor('output', 't1_pitch');
    return { width: card.offsetWidth, outputRight: p.right, cardRight: c.right,
      anchorX: anchor?.x, savedWidth: canvas.cardFor(1)?.width };
  });
  // Owner-approved full-width layout; phones pan/zoom rather than squeeze jacks.
  expect(geometry.width).toBe(1250);
  expect(geometry.savedWidth).toBe(geometry.width);
  expect(geometry.anchorX).toBe(geometry.width);
  expect(geometry.outputRight).toBeGreaterThan(geometry.cardRight - 28);
}

test('Groove keeps horizontal output jacks and stable anchors at phone and desktop widths', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto('/');
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.evaluate(p => (window as any).__wirescapeTest.load(p, 'Groove I/O'), patch);
  await expect(page.getByTestId('groove-1')).toBeVisible();
  for (const id of ['t1_pitch', 't1_gate', 't1_notes']) {
    await expect(page.getByTestId(`port-1-output-${id}`)).toHaveCSS('display', 'flex');
  }
  await checkAnchor(page);
  const more = page.getByTestId('ports-more-1-output');
  await expect(more).toBeVisible();
  await more.dispatchEvent('pointerdown');
  await expect(page.locator('[data-testid="module-1"] .port.output')).toHaveCount(5);

  await page.setViewportSize({ width: 1024, height: 768 });
  await expect(page.getByTestId('port-1-output-t1_pitch')).toBeVisible();
  await checkAnchor(page);
  await page.setViewportSize({ width: 1440, height: 900 });
  await expect(page.getByTestId('port-1-output-t1_pitch')).toBeVisible();
  await checkAnchor(page);
});
