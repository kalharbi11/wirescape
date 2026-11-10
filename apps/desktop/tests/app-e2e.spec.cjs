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
    const source = root; // #247 is on main: build the sidecar from this checkout.
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
    // The null sink is not a selected physical device: do not silently choose the first output.
    await expect(page.getByTestId('audio-device')).toHaveValue('');
    await expect(page.getByTestId('audio-route-status')).toContainText('Select an available output explicitly');
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

/** Reuse the production Electron/sidecar harness with isolated profiles. */
async function launchRealE2e(profile) {
  const app = await electron.launch({
    executablePath: electronPath,
    args: ['--no-sandbox', '--user-data-dir=' + path.join(scratch, profile),
      desktop, '--engine', executable, '--null-audio',
      '--settings', path.join(scratch, profile + '-settings.json')],
    env: { ...process.env, WIRESCAPE_TEST_STUB: '0' }
  });
  const page = await app.firstWindow();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await page.evaluate(() => {
    window.__e2e = [];
    window.wirescape.onMessage(message => window.__e2e.push(message));
  });
  return { app, page };
}

async function connectWithReply(page, connection) {
  return page.evaluate(c => new Promise((resolve, reject) => {
    const timer = setTimeout(() => { off(); reject(new Error('Missing graph-edit reply')); }, 8000);
    const off = window.wirescape.onMessage(m => {
      if (typeof m.ok !== 'boolean' || !/^ui\d+$/.test(m.re ?? '')) return;
      clearTimeout(timer); off(); resolve(m);
    });
    try { window.__wirescapeTest.connect(c); }
    catch (e) { clearTimeout(timer); off(); reject(e); }
  }), connection);
}

test('real sidecar: Save, quit and reopen preserve unknown document bytes', async () => {
  test.setTimeout(90000);
  const filename = path.join(scratch, 'evidence-roundtrip.json');
  const patch = JSON.parse(fs.readFileSync(example, 'utf8'));
  patch.future_unknown = { tag: 'retain', data: [true, 42, 'untouched'] };
  patch.modules.find(m => m.id === 3).future_module = { revision: 17, opaque: 'kept' };
  const first = await launchRealE2e('save-first');
  let bytes;
  try {
    await first.page.evaluate(doc => window.__wirescapeTest.load(doc, 'Evidence roundtrip'), patch);
    await first.app.evaluate(({ dialog }, file) => {
      dialog.showSaveDialog = async () => ({ canceled: false, filePath: file });
    }, filename);
    await first.app.evaluate(({ Menu }) =>
      Menu.getApplicationMenu().getMenuItemById('save-as').click());
    await expect.poll(() => fs.existsSync(filename)).toBe(true);
    bytes = fs.readFileSync(filename, 'utf8');
    expect(JSON.parse(bytes).future_unknown).toEqual(patch.future_unknown);
    expect(JSON.parse(bytes).modules.find(m => m.id === 3).future_module)
      .toEqual(patch.modules.find(m => m.id === 3).future_module);
  } finally { await first.app.close().catch(() => {}); }

  const reopened = await launchRealE2e('save-reopened');
  try {
    await reopened.app.evaluate(({ dialog }, file) => {
      dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [file] });
    }, filename);
    await reopened.app.evaluate(({ Menu }) =>
      Menu.getApplicationMenu().getMenuItemById('open-patch').click());
    await expect(reopened.page.getByTestId('patch-name')).toHaveText('evidence-roundtrip.json');
    await expect.poll(() => reopened.page.evaluate(() =>
      window.__wirescapeTest.state().future_unknown?.tag)).toBe('retain');
    const after = await reopened.page.evaluate(() =>
      JSON.stringify(window.__wirescapeTest.state(), null, 2));
    expect(after).toBe(bytes); // Exact persisted serialization, not a hash or normalized subset.
    await expect(reopened.page.getByTestId('status-engine')).toHaveText('engine: real');
  } finally { await reopened.app.close().catch(() => {}); }
});

test('real sidecar: refused unknown-port connection rolls back and leaves graph usable', async () => {
  test.setTimeout(90000);
  const { app, page } = await launchRealE2e('rejected-port');
  try {
    const patch = JSON.parse(fs.readFileSync(example, 'utf8'));
    await page.evaluate(doc => window.__wirescapeTest.load(doc, 'Rollback graph'), patch);
    const before = await page.evaluate(() => window.__wirescapeTest.state());
    const gen = await page.evaluate(() => window.__e2e.filter(m =>
      m.ok === true && /^ui\d+$/.test(m.re ?? '')).at(-1)?.gen);
    expect(gen).toMatch(/^\d+$/);
    const bad = { kind: 'signal', from: { module: 3, port: 'audio' },
      to: { module: 5, port: 'nonexistent-input' } };
    const rejected = await connectWithReply(page, bad);
    expect(rejected.ok).toBe(false);
    expect(rejected.code).toBe('not-found');
    expect(rejected.gen).toBe(gen);
    await expect.poll(() => page.evaluate(() =>
      JSON.stringify(window.__wirescapeTest.state().connections)))
      .toBe(JSON.stringify(before.connections));

    // A legal graph swap after the refusal proves the engine did not retain the bad edge.
    const legal = { kind: 'modulation', from: { module: 7, port: 'control' },
      to: { module: 6, port: 'release' }, amount: 0.05 };
    const accepted = await connectWithReply(page, legal);
    expect(accepted.ok).toBe(true);
    expect(Number(accepted.gen)).toBeGreaterThan(Number(gen));
    await expect.poll(() => page.evaluate(() => window.__wirescapeTest.state().connections
      .some(c => c.kind === 'modulation' && c.from.module === 7 && c.to.module === 6 &&
        c.to.port === 'release'))).toBe(true);
  } finally { await app.close().catch(() => {}); }
});

test('real sidecar: device-free ADSR and modulated cutoff telemetry carry numeric values', async () => {
  test.setTimeout(90000);
  const { app, page } = await launchRealE2e('telemetry-values');
  try {
    // Sidecar noteOn targets host.keyboard; the class demo's midi.in is not an injection seam.
    const patch = JSON.parse(fs.readFileSync(example, 'utf8'));
    patch.modules = patch.modules.filter(m => ![1, 2, 9].includes(m.id));
    patch.modules.push({ id: 1, module: 'host.keyboard', version: 1, parameters: { voices: 4 } });
    // Avoid the original 20 Hz floor masking a valid negative LFO modulation.
    patch.modules.find(m => m.id === 4).parameters.cutoff = 650;
    patch.connections = patch.connections.filter(c =>
      ![1, 2, 9].includes(c.from.module) && ![1, 2, 9].includes(c.to.module));
    patch.connections.push(
      { kind: 'signal', from: { module: 1, port: 'pitch' }, to: { module: 3, port: 'pitch' } },
      { kind: 'signal', from: { module: 1, port: 'gate' }, to: { module: 6, port: 'gate' } }
    );
    await page.evaluate(doc => window.__wirescapeTest.load(doc, 'Native telemetry'), patch);
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m =>
      m.type === 'telemetry' && Number.isFinite(m.telemetry?.effective?.['4']?.cutoff))))
      .toBe(true);
    await page.evaluate(() => window.__wirescapeTest.noteOn(60, 0.8));
    await expect.poll(() => page.evaluate(() => window.__e2e.some(m =>
      m.type === 'telemetry' && Number(m.telemetry?.envelope?.['6']) > 0.1 &&
      Number(m.telemetry.envelope['6']) <= 1))).toBe(true);
    const cutoff = await page.evaluate(() => window.__e2e.filter(m =>
      m.type === 'telemetry' && Number.isFinite(m.telemetry?.effective?.['4']?.cutoff))
      .at(-1).telemetry.effective['4'].cutoff);
    expect(cutoff).toBeGreaterThanOrEqual(20);
    expect(cutoff).toBeLessThanOrEqual(20000);
    // Two same-generation native samples must actually differ: merely reading
    // the stored base cutoff (650 Hz) would otherwise satisfy the range check.
    await expect.poll(() => page.evaluate(() => {
      const gen = window.__e2e.filter(m => m.ok === true && /^ui\d+$/.test(m.re ?? '')).at(-1)?.gen;
      const values = window.__e2e.filter(m => m.type === 'telemetry' && m.gen === gen)
        .map(m => m.telemetry?.effective?.['4']?.cutoff)
        .filter(Number.isFinite);
      return values.length > 1 ? Math.max(...values) - Math.min(...values) : 0;
    }), { timeout: 10000 }).toBeGreaterThan(0.5);
    await page.evaluate(() => window.__wirescapeTest.noteOff(60));
    await expect.poll(() => page.evaluate(() => {
      const envelope = window.__e2e.filter(m =>
        m.type === 'telemetry' && Number.isFinite(m.telemetry?.envelope?.['6']))
        .at(-1)?.telemetry.envelope['6'];
      return envelope === undefined ? 1 : envelope;
    })).toBeLessThan(0.05);
  } finally { await app.close().catch(() => {}); }
});
