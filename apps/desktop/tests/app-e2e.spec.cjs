// SPDX-License-Identifier: GPL-3.0-only
// Hosted Linux: actual Electron + real C++ sidecar; no device authorization.
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
const { execFileSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const example = path.join(root, 'native/examples/class_demo_2_subtractive.json');
const desktop = path.resolve(__dirname, '../build/main.js');
const electronPath = require('electron');
let scratch, executable;

test.skip(process.platform !== 'linux', 'Hosted Linux Xvfb smoke only');
test.beforeAll(() => {
  test.setTimeout(12 * 60 * 1000); // Native dependency download + C++ build.
  scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-app-e2e-'));
  if (process.env.WIRESCAPE_REAL_ENGINE) {
    executable = path.resolve(process.env.WIRESCAPE_REAL_ENGINE);
  } else {
    let source = root;
    if (!fs.existsSync(path.join(source, 'native/sidecar/main.cpp'))) {
      // Until #247 lands, compile its PR head rather than a test stub.
      source = path.join(scratch, 'sidecar-source');
      execFileSync('git', ['init', '-q', source], { stdio: 'inherit' });
      execFileSync('git', ['-C', source, 'remote', 'add', 'origin',
        'https://github.com/kalharbi11/wirescape.git'], { stdio: 'inherit' });
      execFileSync('git', ['-C', source, 'fetch', '--depth=1', 'origin', 'refs/pull/247/head'],
        { stdio: 'inherit', timeout: 120000 });
      execFileSync('git', ['-C', source, 'checkout', '-q', '--detach', 'FETCH_HEAD'], { stdio: 'inherit' });
      console.log('Real sidecar source #247:', execFileSync('git',
        ['-C', source, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim());
    }
    const build = path.join(scratch, 'build/native');
    execFileSync('cmake', ['-S', path.join(source, 'native'), '-B', build,
      '-DCMAKE_BUILD_TYPE=Release', '-DWIRESCAPE_BUILD_UI=OFF',
      '-DCMAKE_SKIP_INSTALL_RULES=ON'],
    { stdio: 'inherit', timeout: 600000 });
    execFileSync('cmake', ['--build', build, '--target', 'wirescape_engine_sidecar',
      '--parallel', '2'], { stdio: 'inherit', timeout: 600000 });
    executable = path.join(build, 'wirescape-engine');
  }
  expect(fs.statSync(executable).isFile()).toBe(true);
});
test.afterAll(() => { if (scratch) fs.rmSync(scratch, { recursive: true, force: true }); });

function engineChild(app) {
  const mainPid = app.process().pid;
  const children = fs.readFileSync(`/proc/${mainPid}/task/${mainPid}/children`, 'utf8');
  for (const child of children.trim().split(/\s+/).filter(Boolean)) {
    try {
      const argv = fs.readFileSync(`/proc/${child}/cmdline`, 'utf8').split('\0').filter(Boolean);
      if (argv[0] === executable) return { pid: Number(child), argv };
    } catch { /* Child exited during inspection. */ }
  }
  return null;
}

test('real sidecar: hello, devices, native patch, live knob, cable D5 and crash restore', async () => {
  test.setTimeout(90000);
  const app = await electron.launch({
    executablePath: electronPath,
    args: ['--no-sandbox', '--user-data-dir=' + path.join(scratch, 'profile'),
      desktop, '--engine', executable, '--null-audio',
      '--settings', path.join(scratch, 'settings.json')],
    env: { ...process.env, WIRESCAPE_TEST_STUB: '0' }
  });
  try {
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    await page.evaluate(() => {
      window.__e2e = [];
      window.wirescape.onMessage(m => window.__e2e.push(m));
    });
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m => m.type === 'status'))).toBe(true);
    const initial = await page.evaluate(() => window.__e2e.find(m => m.type === 'hello'));
    expect(initial.protocol).toBe(0);
    expect(initial.catalogue.modules.map(m => m.id)).toContain('essentials.vcf');
    await expect(page.getByTestId('library-item-essentials.vcf')).toHaveCount(1);
    const child = engineChild(app);
    expect(child).not.toBeNull();
    expect(child.argv).toContain('--null-audio');
    expect(child.argv).not.toContain('--confirm-audio');
    expect(child.argv).not.toContain('--confirm-midi');

    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-device')).toHaveValue('USB Stereo');
    await expect(page.getByTestId('midi-device')).toHaveValue('M8 MIDI 1|M8:M8 MIDI 1');
    // Null audio renders paced blocks; it must not impersonate a hardware device.
    expect(await page.evaluate(() => window.__e2e.find(m => m.type === 'status').status.audio.state))
      .toBe('no-device');
    const originalGen = await page.evaluate(() => window.__e2e.filter(m => m.ok === true).at(-1)?.gen ?? '0');
    await page.evaluate(gen => { window.__e2eBeforeGen = gen; }, originalGen);

    await app.evaluate(({ dialog }, file) => {
      dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [file] });
    }, example);
    await app.evaluate(({ Menu }) => Menu.getApplicationMenu().getMenuItemById('open-patch').click());
    await expect(page.getByTestId('patch-name')).toHaveText('class_demo_2_subtractive.json');
    await expect.poll(() => page.evaluate(() => window.__wirescapeTest.state().modules.length)).toBe(9);
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m =>
      m.ok === true && /^ui\d+$/.test(m.re ?? '') && Number(m.gen) > Number(window.__e2eBeforeGen ?? 0)
    ))).toBe(true);
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
    // The load ack precedes the next 10 Hz status push. Do not compare the
    // live knob ack with a stale, pre-load graph generation.
    await expect.poll(() => page.evaluate(() => {
      const loaded = window.__e2e.filter(m => m.ok === true && /^ui\d+$/.test(m.re ?? '') &&
        Number(m.gen) > Number(window.__e2eBeforeGen ?? 0)).at(-1);
      const status = window.__e2e.filter(m => m.type === 'status').at(-1);
      return !!loaded && status?.gen === loaded.gen;
    })).toBe(true);

    // Exercise the same commit callback as the interactive VCF cutoff knob.
    const knob = await page.evaluate(() => new Promise((resolve, reject) => {
      const before = window.__e2e.filter(m => m.type === 'status').at(-1)?.gen;
      const timeout = setTimeout(() => { off(); reject(new Error('Missing live knob ack')); }, 8000);
      const off = window.wirescape.onMessage(m => {
        if (!/^ui\d+$/.test(m.re ?? '')) return;
        clearTimeout(timeout); off(); resolve({ reply: m, before });
      });
      window.__wirescapeTest.commitParam(4, 'cutoff', 650, true);
    }));
    expect(knob.reply.ok).toBe(true);
    expect(knob.reply.gen).toBe(knob.before);
    expect(await page.evaluate(() => window.__wirescapeTest.state().modules.find(m => m.id === 4)
      .parameters.cutoff)).toBe(650);

    // New modulation cable exercises RealEngine's optimistic edit journal and D5 swap.
    const cable = await page.evaluate(() => new Promise((resolve, reject) => {
      const before = window.__e2e.filter(m => m.type === 'status').at(-1)?.gen;
      const timeout = setTimeout(() => { off(); reject(new Error('Missing cable ack')); }, 8000);
      const off = window.wirescape.onMessage(m => {
        if (!/^ui\d+$/.test(m.re ?? '')) return;
        clearTimeout(timeout); off();
        resolve({ reply: m, elapsedMs: performance.now() - started, before });
      });
      const started = performance.now();
      window.__wirescapeTest.connect({
        kind: 'modulation', from: { module: 7, port: 'control' },
        to: { module: 6, port: 'release' }, amount: 0.05
      });
    }));
    expect(cable.reply.ok).toBe(true);
    expect(Number(cable.reply.gen)).toBeGreaterThan(Number(cable.before));
    expect(cable.elapsedMs).toBeLessThanOrEqual(150); // Wall-clock UI→ack D5 smoke budget.
    const saved = await page.evaluate(() => window.__wirescapeTest.state());
    expect(saved.connections.some(c => c.from.module === 7 && c.to.module === 6 &&
      c.to.port === 'release')).toBe(true);
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m =>
      m.type === 'telemetry' && m.telemetry.tick > 1 && m.telemetry.meters['8']))).toBe(true);

    process.kill(child.pid, 'SIGKILL');
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m =>
      m.type === 'app.engineExited' && m.expected === false))).toBe(true);
    await expect.poll(() => page.evaluate(() => window.__e2e.filter(m =>
      m.type === 'hello').length)).toBe(2);
    const restarted = await page.evaluate(() => window.__e2e.filter(m => m.type === 'hello').at(-1).session);
    expect(restarted).not.toBe(initial.session);
    await expect.poll(() => page.evaluate(session => window.__e2e.some(m =>
      m.session === session && /^ui\d+$/.test(m.re ?? '') &&
      m.ok === true && Number(m.gen) >= 1), restarted)).toBe(true);
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
    expect(await page.evaluate(() => window.__wirescapeTest.state())).toEqual(saved);
  } finally {
    await app.close().catch(() => {});
  }
});
