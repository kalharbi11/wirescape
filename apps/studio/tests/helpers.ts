// SPDX-License-Identifier: GPL-3.0-only
import type { Page, Locator } from '@playwright/test';

export interface TestApi {
  state(): any;
  load(p: any, name?: string): void;
  autosaveNow(): void;
  clearAutosave(): void;
  patchName(): string;
  zoomTo(s: number): void;
  fit(): void;
  noteOn(n: number, v?: number): void;
  noteOff(n: number): void;
  fps(): number;
}

declare global {
  interface Window { __wirescapeTest: TestApi & Record<string, any> }
}

export async function boot(page: Page, opts: { blank?: boolean } = {}): Promise<void> {
  // Note: no addInitScript here. It would run again on page.reload() and
  // wipe the autosave the restore test is checking.
  await page.goto('/');
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
  await page.evaluate(() => window.__wirescapeTest.clearAutosave());
  if (opts.blank !== false) {
    await page.evaluate(() => window.__wirescapeTest.load({
      format: 'wirescape.native.patch', version: 2, modules: [], connections: [],
      ui: { positions: {}, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'Test'));
  }
  await settle(page);
}

export const settle = (page: Page, frames = 3) =>
  page.evaluate(async n => {
    for (let i = 0; i < n; i++) await new Promise(r => requestAnimationFrame(() => r(null)));
  }, frames);

export const state = (page: Page) => page.evaluate(() => window.__wirescapeTest.state());

/** Add a module from the library, then drag it to a world position. */
export async function addModuleAt(page: Page, type: string, x: number, y: number): Promise<number> {
  const before = await page.evaluate(() => window.__wirescapeTest.state().modules.map((m: any) => m.id));
  await page.getByTestId(`library-item-${type}`).click();
  await settle(page);
  const id = await page.evaluate(
    (prev: number[]) => window.__wirescapeTest.state().modules
      .map((m: any) => m.id).find((i: number) => !prev.includes(i)),
    before,
  );
  if (id === undefined) throw new Error(`${type} was not added`);
  await dragModuleTo(page, id, x, y);
  return id;
}

/** Drag a module by its header until its world position is (x, y). */
export async function dragModuleTo(page: Page, id: number, x: number, y: number): Promise<void> {
  const header = page.getByTestId(`module-header-${id}`);
  const box = await header.boundingBox();
  if (!box) throw new Error(`module ${id} header not visible`);
  const { pos, zoom } = await page.evaluate((mid: number) => {
    const s = window.__wirescapeTest.state();
    return { pos: s.ui.positions[String(mid)] as [number, number], zoom: s.ui.view.zoom as number };
  }, id);
  const dx = (x - pos[0]) * zoom;
  const dy = (y - pos[1]) * zoom;
  const sx = box.x + Math.min(box.width / 2, 60);
  const sy = box.y + box.height / 2;
  await page.mouse.move(sx, sy);
  await page.mouse.down();
  await page.mouse.move(sx + dx / 2, sy + dy / 2, { steps: 6 });
  await page.mouse.move(sx + dx, sy + dy, { steps: 6 });
  await page.mouse.up();
  await settle(page);
}

export const port = (page: Page, module: number, dir: 'input' | 'output', id: string): Locator =>
  page.getByTestId(`port-${module}-${dir}-${id}`);

export const knob = (page: Page, module: number, param: string): Locator =>
  page.getByTestId(`knob-${module}-${param}`);

/** Patch a cable by dragging one port onto another. */
export async function patch(
  page: Page,
  from: { module: number; port: string },
  to: { module: number; port: string },
  opts: { shift?: boolean } = {},
): Promise<void> {
  const a = await port(page, from.module, 'output', from.port).boundingBox();
  const b = await port(page, to.module, 'input', to.port).boundingBox();
  if (!a || !b) throw new Error(`ports not visible: ${from.module}:${from.port} -> ${to.module}:${to.port}`);
  await dragBetween(page, centre(a), centre(b), opts.shift);
}

/** Drop a signal output onto a knob to create a modulation. */
export async function patchToKnob(
  page: Page,
  from: { module: number; port: string },
  to: { module: number; param: string },
): Promise<void> {
  const a = await port(page, from.module, 'output', from.port).boundingBox();
  const b = await knob(page, to.module, to.param).locator('.dial-wrap').boundingBox();
  if (!a || !b) throw new Error('port or knob not visible');
  await dragBetween(page, centre(a), centre(b));
}

async function dragBetween(
  page: Page, a: { x: number; y: number }, b: { x: number; y: number }, shift = false,
): Promise<void> {
  await page.mouse.move(a.x, a.y);
  await page.mouse.down();
  await page.mouse.move((a.x + b.x) / 2, (a.y + b.y) / 2 - 30, { steps: 8 });
  await page.mouse.move(b.x, b.y, { steps: 8 });
  if (shift) await page.keyboard.down('Shift');
  await page.mouse.up();
  if (shift) await page.keyboard.up('Shift');
  await settle(page);
}

export const centre = (b: { x: number; y: number; width: number; height: number }) =>
  ({ x: b.x + b.width / 2, y: b.y + b.height / 2 });

/** The live marker's rotation on a knob, in degrees. */
export async function markerAngle(page: Page, module: number, param: string): Promise<number> {
  return page.evaluate(({ module, param }) => {
    const el = document.querySelector<SVGGElement>(
      `[data-testid="knob-${module}-${param}"] .live-marker`);
    const t = el?.style.transform ?? '';
    const m = /rotate\(([-0-9.]+)deg\)/.exec(t);
    return m ? Number(m[1]) : NaN;
  }, { module, param });
}
