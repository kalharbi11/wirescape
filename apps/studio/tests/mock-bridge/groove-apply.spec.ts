// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle } from '../helpers';
import { mockBridge } from './bridge';

/** Groove G2a (#498) Apply gate against the real-engine client and the five-member bridge. */
const steps = [{ kind: 'note', key: 60, velocity: 0.8 }, { kind: 'rest' }];
const patch = {
  format: 'wirescape.native.patch', version: 2, connections: [],
  modules: [{ id: 1, module: 'sequencing.groove', version: 2, config: { version: 2, bpm: 120, extra: 'ui-only',
    tracks: [{ length: 16, armed: false, steps }, { length: 16, armed: false, steps: [{ kind: 'rest' }] }] } }],
};
const telemetry = (page: Page, playhead: Record<number, [number, number]>, gen?: string) =>
  page.evaluate(([p, g]) => (window as any).__mockBridge.telemetry({ playhead: p,
    groove: { 1: [{ step: p[1]?.[0] ?? -1, loop: 1, playing: (p[1]?.[0] ?? -1) >= 0, fired: true },
      { step: p[1]?.[1] ?? -1, loop: 1, playing: false, fired: true }] } }, g ?? undefined),
  [playhead, gen] as const);
const sent = (page: Page) => page.evaluate(() => (window as any).__mockBridge.sent as any[]);
const graphConfig = (page: Page) => page.evaluate(() => (window as any).__mockBridge.getGraph().modules[0].config);
const apply = (page: Page) => page.getByTestId('groove-apply-1');

test.beforeEach(async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Groove'), patch);
  await expect.poll(() => graphConfig(page)).toEqual({ version: 2, bpm: 120,
    tracks: [{ length: 16, armed: false, steps }, { length: 16, armed: false, steps: [{ kind: 'rest' }] }] });
  await settle(page);
  await page.evaluate(() => {
    const v = window.__wirescapeTest.canvas.viewport;
    const c = document.querySelector('[data-testid="groove-1"]')!.getBoundingClientRect();
    const r = v.rect();
    v.setView(v.view.x - (c.left - r.left) + 40, v.view.y - (c.top - r.top) + 60, 1);
  });
  await settle(page);
});

test('disarmed running, queued STOP and pre-boundary samples keep Apply off; measured [-1,-1] applies stopped', async ({ page }) => {
  await telemetry(page, { 1: [4, 4] }); // running global transport with both tracks disarmed
  await expect(page.getByTestId('groove-status-1')).toHaveText('PLAYING 1.2.1');
  await page.getByTestId('groove-pad-1-1').click(); // initially highlighted, but not selected by a click yet
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  await page.getByTestId('groove-pad-1-1').click(); // second click toggles the note off
  await expect(page.getByTestId('groove-draft-1')).toBeVisible();
  await expect(apply(page)).toBeDisabled();
  await page.getByTestId('groove-play-1').click(); // queue global STOP
  await expect.poll(async () => (await sent(page)).filter(m => m.type === 'setParam' && m.paramId === 'stop')
    .map(m => m.value)).toEqual([1, 0]);
  await telemetry(page, { 1: [9, 9] });
  await expect(page.getByTestId('groove-status-1')).toHaveText('STOPS at loop end');
  await expect(apply(page)).toBeDisabled();
  await telemetry(page, { 1: [15, 15] }); // last sample before the boundary
  await expect(apply(page)).toBeDisabled();
  await telemetry(page, { 1: [-1, -1] }, '0'); // stale generation never authorizes Apply
  await expect(apply(page)).toBeDisabled();
  await telemetry(page, { 1: [-1, 3] }); // one track still owes its release
  await expect(apply(page)).toBeDisabled();
  await telemetry(page, { 1: [-1, -1] });
  await expect(apply(page)).toBeEnabled();
  const before = (await sent(page)).length;
  await apply(page).click();
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  // The draft badge hides when Apply is staged; the bridge records the load a moment later.
  await expect.poll(async () => (await graphConfig(page)).tracks[0].steps[0]).toEqual({ kind: 'rest' });
  const config = await graphConfig(page);
  expect(config.extra).toBeUndefined(); // unknown keys stay in the document only
  expect((await page.evaluate(() => window.__wirescapeTest.state())).modules[0].config.extra).toBe('ui-only');
  const after = (await sent(page)).slice(before);
  expect(after.filter(m => m.type === 'loadGraph')).toHaveLength(1);
  expect(after.some(m => m.type === 'setParam' && m.paramId === 'run')).toBe(false); // never auto-Starts
});

test('a refused Apply keeps the draft and starts nothing; Stop & apply waits for the measured stop', async ({ page }) => {
  await telemetry(page, { 1: [-1, -1] });
  await page.getByTestId('groove-pad-1-1').click();
  await page.getByTestId('groove-pad-1-1').click();
  await expect(apply(page)).toBeEnabled();
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'invalid'));
  const before = (await sent(page)).length;
  await apply(page).click();
  await expect(page.getByTestId('groove-draft-1')).toBeVisible();
  expect((await graphConfig(page)).tracks[0].steps[0]).toEqual(steps[0]);
  expect((await sent(page)).slice(before).some(m => m.type === 'setParam' && m.paramId === 'run')).toBe(false);
  expect((await page.evaluate(() => window.__wirescapeTest.state())).modules[0].config.tracks[0].steps[0])
    .toEqual({ kind: 'rest' });

  // Recover the engine graph (the refused load leaves the client not ready), then run.
  await page.evaluate(() => window.__wirescapeTest.engine.setParam(1, 'reset', 0));
  await expect.poll(() => graphConfig(page)).toMatchObject({ version: 2 });
  await telemetry(page, { 1: [2, 2] });
  await expect(page.getByTestId('groove-stop-apply-1')).toBeVisible();
  const stops = (await sent(page)).length;
  await page.getByTestId('groove-stop-apply-1').click();
  await expect.poll(async () => (await sent(page)).slice(stops).filter(m => m.type === 'setParam' && m.paramId === 'stop')
    .map(m => m.value)).toEqual([1, 0]);
  await telemetry(page, { 1: [12, 12] });
  await expect(page.getByTestId('groove-draft-1')).toBeVisible();
  await telemetry(page, { 1: [-1, -1] });
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  expect((await graphConfig(page)).tracks[0].steps[0]).toEqual({ kind: 'rest' });
});

test('undo/redo reloads never carry an unapplied draft to the engine', async ({ page }) => {
  await page.getByTestId('groove-pad-1-1').click();
  await page.getByTestId('groove-pad-1-1').click();
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.noise'));
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  const before = (await sent(page)).length;
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect.poll(async () => (await sent(page)).slice(before).filter(m => m.type === 'loadGraph').length).toBe(1);
  const load = (await sent(page)).slice(before).find(m => m.type === 'loadGraph');
  expect(load.patch.modules[0].config.tracks[0].steps[0]).toEqual(steps[0]);
  expect((await page.evaluate(() => window.__wirescapeTest.state())).modules[0].config.tracks[0].steps[0])
    .toEqual({ kind: 'rest' });
});

test('playhead and skipped-this-pass dim come only from telemetry', async ({ page }) => {
  await expect(page.locator('[data-testid="groove-1"] .g-pad.play')).toHaveCount(0);
  await expect(page.getByTestId('groove-pos-1-1')).toHaveText('');
  await page.evaluate(() => (window as any).__mockBridge.telemetry({ playhead: { 1: [0, -1] },
    groove: { 1: [{ step: 0, loop: 2, playing: true, fired: false }, { step: -1, loop: 0, playing: false, fired: false }] } }));
  await expect(page.getByTestId('groove-pad-1-1')).toHaveClass(/play/);
  await expect(page.getByTestId('groove-pad-1-1')).toHaveClass(/skip/);
  await expect(page.getByTestId('groove-pos-1-1')).toHaveText('01/16');
  await telemetry(page, { 1: [1, -1] });
  await expect(page.getByTestId('groove-pad-1-2')).toHaveClass(/play/);
  await expect(page.getByTestId('groove-pad-1-1')).not.toHaveClass(/play|skip/);
});

test('refused whole-draft Apply restores the playing Groove for an unrelated undo reload with overlapping IDs', async ({ page }) => {
  // A new document reuses module ID 1 but changes the note. A refused load leaves the original playing.
  const candidate = JSON.parse(JSON.stringify(patch));
  candidate.modules[0].config.tracks[0].steps[0] = { kind: 'note', key: 72, velocity: 0.8 };
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused Groove'), candidate);
  await expect(page.getByTestId('open-refused-draft')).toBeVisible();
  await page.getByTestId('open-refused-draft').click();
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  expect((await page.evaluate(() => window.__wirescapeTest.state())).modules[0].config.tracks[0].steps[0].key).toBe(72);

  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'unavailable'));
  await page.getByTestId('apply-refused-draft').click();
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  expect((await graphConfig(page)).tracks[0].steps[0]).toEqual(steps[0]);
  await page.getByTestId('back-to-playing').click();
  await expect(page.getByTestId('patch-name')).toHaveText('Groove');

  // Undoing an unrelated topology edit invokes the whole-graph projection again.
  // It must not replay the rejected candidate's config for the same module ID.
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.noise'));
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  const before = (await sent(page)).length;
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect.poll(async () => (await sent(page)).slice(before).filter(m => m.type === 'loadGraph').length).toBe(1);
  const reload = (await sent(page)).slice(before).find(m => m.type === 'loadGraph');
  expect(reload.patch.modules.find((m: any) => m.id === 1).config.tracks[0].steps[0]).toEqual(steps[0]);
  await expect.poll(async () => (await graphConfig(page)).tracks[0].steps[0]).toEqual(steps[0]);
});

test('G2b Roll/bar edits stay in draft until a measured stop and explicit Apply', async ({ page }) => {
  const baseline = JSON.stringify(await graphConfig(page));
  await page.getByTestId('groove-copy-bar-1').click();
  await page.getByTestId('groove-len-32-1').click();
  await page.getByTestId('groove-pad-1-18').click(); // select bar two; does not mutate the pattern
  await page.getByTestId('groove-paste-bar-1').click();
  await page.getByTestId('groove-scale-1').selectOption('Minor');
  await page.getByTestId('groove-look-1').selectOption('led');
  await page.getByTestId('groove-view-roll-1').click();
  await expect(page.getByTestId('groove-roll-1')).toBeVisible();
  const current = await page.evaluate(() => window.__wirescapeTest.state().modules[0].config);
  expect(current.tracks[0].length).toBe(32);
  expect(current.tracks[0].steps[16]).toMatchObject({ kind: 'note', key: 60 });
  expect(JSON.stringify(await graphConfig(page))).toBe(baseline);
  await telemetry(page, { 1: [18, -1] }); // a rest still has a real measured Roll playhead
  await expect(page.getByTestId('groove-roll-1-19-60')).toHaveClass(/play/);
  await expect(page.getByTestId('groove-roll-velocity-1-19')).toHaveClass(/play/);
  await expect(apply(page)).toBeDisabled();
  await telemetry(page, { 1: [-1, -1] });
  await expect(apply(page)).toBeEnabled();
  const before = (await sent(page)).length;
  await apply(page).click();
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  expect((await graphConfig(page)).tracks[0].steps[16]).toMatchObject({ kind: 'note', key: 60 });
  expect((await sent(page)).slice(before).filter(m => m.type === 'loadGraph')).toHaveLength(1);
  expect((await sent(page)).slice(before).some(m => m.type === 'setParam' && m.paramId === 'run')).toBe(false);
});
