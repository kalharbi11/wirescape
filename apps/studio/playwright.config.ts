// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig, devices } from '@playwright/test';

/**
 * The container ships a Chromium that does not match this Playwright's
 * pinned build, so every project points at it explicitly.
 */
const executablePath = process.env.WIRESCAPE_CHROMIUM ?? '/opt/pw-browsers/chromium';

export default defineConfig({
  testDir: './tests',
  timeout: 60_000,
  expect: { timeout: 10_000 },
  fullyParallel: false,
  workers: 1,
  reporter: process.env.WIRESCAPE_PI_RUN === '1'
    ? [['list'], ['json', { outputFile: 'build_reports/pi-ui/playwright.json' }]] : [['list']],
  metadata: { piRun: process.env.WIRESCAPE_PI_RUN === '1' },
  use: {
    baseURL: 'http://localhost:4173',
    headless: process.env.WIRESCAPE_HEADED !== '1',
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    launchOptions: { executablePath, args: ['--no-sandbox', ...(process.env.WIRESCAPE_PI_RUN === '1' ? ['--start-maximized'] : []), ...(process.env.WIRESCAPE_WAYLAND === '1' ? ['--ozone-platform=wayland'] : [])] },
  },
  projects: [
    {
      name: 'desktop',
      use: process.env.WIRESCAPE_PI_RUN === '1'
        ? { viewport: null }
        : { ...devices['Desktop Chrome'], viewport: { width: 1920, height: 1080 } },
    },
  ],
  webServer: {
    command: 'npm run build && npm run preview',
    url: 'http://localhost:4173',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
});
