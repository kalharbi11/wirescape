// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';
import { QUALITIES, PC_NAMES, chordName, qualityKeys, notesOf, setNotes, validKeys } from '../src/groove/chords';
import { defaultConfig, engineConfig, setKind } from '../src/groove/pattern';

const cfg = async (p: Page) => (await state(p)).modules.find((m: any) => m.module === 'sequencing.groove')?.config;
const flush = (p: Page) => p.evaluate(() => window.__wirescapeTest.flushHistory());

test('every quality on all 12 roots names from its interval set, including ambiguous root readings', () => {
  for (let pc = 0; pc < 12; pc++) {
    const root = 48 + pc;
    for (const [q, symbol] of QUALITIES) {
      const keys = qualityKeys(root, q);
      expect(chordName(keys, root), q + ' root ' + pc).toBe(PC_NAMES[pc] + symbol);
    }
  }
  const notes = [60, 64, 67, 69];
  expect(chordName(notes, 60)).toBe('C6');
  expect(chordName(notes, 69)).toBe('Am7/C');
  const s = { kind: 'note' as const, key: 60, custom: { keep: true } };
  setNotes(s, notes, 69);
  expect((s as any).keys).toEqual([69, 60, 64, 67]);
  expect(validKeys(69, (s as any).keys)).toBe(true);
  setNotes(s, [60]);
  expect((s as any)).toEqual({ kind: 'note', key: 60, custom: { keep: true } });
  const c = defaultConfig();
  const step = { kind: 'note' as const, key: 60, keys: [60, 64, 67] };
  setKind(step, 'rest');
  expect(step.keys).toBeUndefined();
  c.tracks[0]!.steps = [{ kind: 'note', key: 60, keys: [60, 64, 67] }];
  expect((engineConfig(c) as any).tracks[0].steps[0].keys).toEqual([60, 64, 67]);
  expect(notesOf(c.tracks[0]!.steps[0]!)).toEqual([60, 64, 67]);
});

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await settle(page);
});

test('Inspector switches Single→Chord, picker, inversion, Root label and History', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-notes-chord-1').click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67]);
  await expect(page.locator('[data-testid="groove-1"] .g-insp .g-lbl', { hasText: 'Root' })).toBeVisible();
  await expect(page.getByTestId('groove-pad-1-2').locator('.cue-ch i')).toHaveCount(3);
  await expect(page.getByTestId('groove-pad-1-2').locator('.g-nn')).toHaveText('C');
  await page.getByTestId('groove-chord-picker-1').click();
  await page.getByTestId('groove-chord-opt-maj7-1').click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67, 71]);
  await page.getByTestId('groove-invert-up-1').click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([72, 64, 67, 71]);
  await flush(page);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].keys[0]).toBe(60);
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].keys[0]).toBe(72);
  await page.getByTestId('groove-notes-single-1').click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toBeUndefined();
});

test('scale triad, diatonic move, parallel move and clickable off-scale option', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-scale-1').selectOption('Major');
  await page.getByTestId('groove-notes-chord-1').click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67]);
  await page.getByTestId('groove-1').focus();
  await page.keyboard.press('ArrowUp');
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([62, 65, 69]);
  await page.getByTestId('groove-chord-picker-1').click();
  await expect(page.getByTestId('groove-chord-opt-maj-1')).toHaveClass(/out/);
  await page.getByTestId('groove-chord-opt-maj-1').click();
  await page.getByTestId('groove-1').focus();
  await page.keyboard.press('ArrowUp');
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([64, 68, 71]);
});

test('chord bar copies and pastes keys, without leaking into other notes', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-notes-chord-1').click();
  await page.getByTestId('groove-copy-bar-1').click();
  await page.getByTestId('groove-len-32-1').click();
  await page.getByTestId('groove-pad-1-18').click();
  await page.getByTestId('groove-paste-bar-1').click();
  expect((await cfg(page)).tracks[0].steps[17].keys).toEqual([60, 64, 67]);
  await flush(page);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[17]?.keys).toBeUndefined();
});