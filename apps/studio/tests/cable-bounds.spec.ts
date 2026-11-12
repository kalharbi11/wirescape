// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot } from './helpers';

// Owner report (2026-11-12, desktop, Basics 2 at 100 %): cables ended mid-canvas. The cable SVG was
// 1x1 with overflow: visible, so Chromium's GPU rasteriser repainted cables only near cards.
// The SVG must now cover every cable in world units, at any zoom.
test('the cable layer covers every cable, before and after zooming in', async ({ page }) => {
  await boot(page);
  await page.getByTestId('starter-select').selectOption('basics-02-fm-filter-lfo');
  const uncovered = () => page.evaluate(() => {
    const svg = document.querySelector<SVGSVGElement>('svg.cable-layer')!;
    const vb = svg.viewBox.baseVal;
    if (!vb || vb.width <= 1 || vb.height <= 1) return ['no viewBox'];
    const left = parseFloat(svg.style.left), top = parseFloat(svg.style.top);
    const out: string[] = [];
    if (left !== vb.x || top !== vb.y || parseFloat(svg.style.width) !== vb.width || parseFloat(svg.style.height) !== vb.height)
      out.push('style and viewBox disagree');
    for (const p of svg.querySelectorAll<SVGPathElement>('path.cable')) {
      if (!p.getAttribute('d')) continue;
      const b = p.getBBox();
      if (b.x < vb.x || b.y < vb.y || b.x + b.width > vb.x + vb.width || b.y + b.height > vb.y + vb.height)
        out.push(p.closest('g')?.getAttribute('data-key') ?? '?');
    }
    return out;
  });
  await expect.poll(uncovered).toEqual([]);
  expect(await page.locator('svg.cable-layer path.cable').count()).toBeGreaterThan(8);
  for (let i = 0; i < 9; i++) await page.getByTestId('zoom-in').click();
  await expect.poll(uncovered).toEqual([]);
});
