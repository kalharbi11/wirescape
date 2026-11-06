// SPDX-License-Identifier: GPL-3.0-only
const { defineConfig } = require('../studio/node_modules/@playwright/test');
module.exports = defineConfig({
  testDir: './tests', testMatch: '*.spec.cjs', timeout: 45000,
  workers: 1, fullyParallel: false, reporter: [['list']],
  use: { trace: 'retain-on-failure', screenshot: 'only-on-failure' }
});
