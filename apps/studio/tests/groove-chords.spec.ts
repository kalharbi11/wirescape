// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import {
  CHROMATIC, chordName, invert, isScaleChord, moveNotes, notesOf, qualityKeys, scaleStack,
  setNotes, stepLabel, validKeys, type ScaleCtx,
} from '../src/groove/chords';

const A_MINOR: ScaleCtx = { root: 9, intervals: [0, 2, 3, 5, 7, 8, 10] };
const C_MAJOR: ScaleCtx = { root: 0, intervals: [0, 2, 4, 5, 7, 9, 11] };

test('chord names are a pure function of keys and root', () => {
  expect(chordName([57, 60, 64], 57)).toBe('Am');
  expect(chordName([60, 64, 67, 71], 60)).toBe('Cmaj7');
  expect(chordName([55, 59, 62], 55)).toBe('G');
  expect(chordName([51, 55, 58, 60], 60)).toBe('Cm7/E♭');
  expect(chordName([60, 61, 66], 60)).toBeNull();
  expect(chordName([60], 60)).toBeNull();
  expect(stepLabel({ key: 69, keys: [69, 70, 75] })).toBe('A4+2');
  expect(stepLabel({ key: 60, keys: [60, 51, 55, 58] }, true)).toBe('Cm7');
  expect(stepLabel({ key: 60, keys: [60, 51, 55, 58] })).toBe('Cm7/E♭');
  expect(stepLabel({ key: 60 })).toBe('C4');
});

test('setNotes writes keys only for 2-5 notes, root first', () => {
  const s: { key?: number; keys?: unknown; kind: string } = { kind: 'note', key: 60 };
  setNotes(s, [67, 60, 64], 60);
  expect(s).toEqual({ kind: 'note', key: 60, keys: [60, 64, 67] });
  expect(validKeys(s.key, s.keys)).toBe(true);
  setNotes(s, [55, 60, 64], 60);
  expect(s.keys).toEqual([60, 55, 64]);
  setNotes(s, [60, 62, 64, 65, 67, 69, 71]);
  expect((s.keys as number[]).length).toBe(5);
  setNotes(s, [62]);
  expect(s).toEqual({ kind: 'note', key: 62 });
  expect(notesOf(s)).toEqual([62]);
});

test('validKeys mirrors the native G4a decoder rules', () => {
  expect(validKeys(60, [60, 64, 67])).toBe(true);
  expect(validKeys(60, [64, 60, 67])).toBe(false);
  expect(validKeys(60, [])).toBe(false);
  expect(validKeys(60, [60, 60])).toBe(false);
  expect(validKeys(60, [60, 128])).toBe(false);
  expect(validKeys(60, [60, 61, 62, 63, 64, 65])).toBe(false);
  expect(validKeys(60, [60, 64.5])).toBe(false);
});

test('scale chords stack thirds and move diatonically; typed chords move in parallel', () => {
  expect(scaleStack(57, 3, A_MINOR)).toEqual([57, 60, 64]);
  expect(scaleStack(57, 4, A_MINOR)).toEqual([57, 60, 64, 67]);
  expect(isScaleChord([57, 60, 64], 57, A_MINOR)).toBe(true);
  const up = moveNotes([57, 60, 64], 57, 1, A_MINOR)!;
  expect(chordName(up.keys, up.root)).toBe('B°');
  const up2 = moveNotes(up.keys, up.root, 1, A_MINOR)!;
  expect(chordName(up2.keys, up2.root)).toBe('C');
  // Cmaj7 is the scale seventh in C major, so it moves diatonically; C7 is not, so it moves in parallel.
  expect(isScaleChord(qualityKeys(60, 'maj7'), 60, C_MAJOR)).toBe(true);
  const dom7 = qualityKeys(60, '7');
  expect(isScaleChord(dom7, 60, C_MAJOR)).toBe(false);
  expect(moveNotes(dom7, 60, 1, C_MAJOR)).toEqual({ keys: [62, 66, 69, 72], root: 62 });
  expect(moveNotes([57, 60, 64], 57, 1, A_MINOR, true)).toEqual({ keys: [69, 72, 76], root: 69 });
  expect(moveNotes([120, 124, 127], 120, 1, CHROMATIC)).toBeNull();
});

test('inversions keep the root on its pitch class and stay storable', () => {
  const inv = invert([60, 64, 67], 60, 1)!;
  expect(inv).toEqual({ keys: [64, 67, 72], root: 72 });
  const s: { key?: number; keys?: unknown } = {};
  setNotes(s, inv.keys, inv.root);
  expect(s).toEqual({ key: 72, keys: [72, 64, 67] });
  expect(chordName(notesOf(s), s.key)).toBe('C/E');
  expect(invert([60, 64, 67], 60, -1)).toEqual({ keys: [55, 60, 64], root: 60 });
  expect(invert([60], 60, 1)).toBeNull();
});
