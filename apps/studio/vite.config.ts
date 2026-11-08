// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig } from 'vite';
import { execSync } from 'node:child_process';
const version = process.env.npm_package_version ?? '0.3.0';
let sha = 'dev';
try { sha = execSync('git rev-parse --short HEAD').toString().trim(); } catch { /* not a git checkout */ }
const build = `${sha} · ${new Date().toISOString().slice(0, 10)}`;

export default defineConfig({
  // The web demo is served from a sub-path (GitHub Pages); the app uses relative fetches.
  base: process.env.WIRESCAPE_BASE ?? '/',
  define: { __APP_VERSION__: JSON.stringify(`v${version}`), __BUILD__: JSON.stringify(build) },
  server: { port: 5173, host: '127.0.0.1' },
  preview: { port: 4173, host: '127.0.0.1' },
  build: { target: 'es2022', sourcemap: true },
});
