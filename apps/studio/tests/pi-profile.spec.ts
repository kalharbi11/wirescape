// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot } from './helpers';

test('automatic Pi watchdog respects opt-out and switches only once per session', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.setPref({ profile: 'normal', autoProfile: false }));
  await page.keyboard.press('`');
  await page.getByTestId('debug-slowFrames').click();
  // Longer than the five-second watchdog window; opt-out must keep Normal.
  await page.waitForTimeout(6500);
  await expect(page.locator('html')).toHaveAttribute('data-profile', 'normal');
  await page.evaluate(() => window.__wirescapeTest.setPref({ autoProfile: true }));
  await expect(page.locator('html')).toHaveAttribute('data-profile', 'pi', { timeout: 9000 });
  // Explicit override after the automatic transition must remain respected.
  await page.evaluate(() => window.__wirescapeTest.setPref({ profile: 'normal' }));
  await page.waitForTimeout(6500);
  await expect(page.locator('html')).toHaveAttribute('data-profile', 'normal');
});
