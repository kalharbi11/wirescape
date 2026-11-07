// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig, devices } from '@playwright/test';

/** Web demo smoke: needs public/web/wirescape.wasm (built by the web-demo workflow). */
const executablePath = process.env.WIRESCAPE_CHROMIUM ?? '/opt/pw-browsers/chromium';
const base = process.env.WIRESCAPE_BASE ?? '/';

export default defineConfig({
  testDir: './tests-web',
  timeout: 60_000,
  expect: { timeout: 15_000 },
  workers: 1,
  reporter: [['list']],
  use: {
    baseURL: 'http://localhost:4174' + base,
    headless: true,
    trace: 'off',
    launchOptions: { executablePath, args: ['--no-sandbox', '--autoplay-policy=no-user-gesture-required'] },
  },
  projects: [{ name: 'web', use: { ...devices['Desktop Chrome'], viewport: { width: 1600, height: 900 } } }],
  webServer: {
    command: 'VITE_WIRESCAPE_WEB=1 npm run build && npx vite preview --port 4174',
    url: 'http://localhost:4174' + base,
    reuseExistingServer: false,
    timeout: 180_000,
  },
});
