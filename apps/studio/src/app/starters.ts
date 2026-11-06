// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2 } from '../patch/types';
import qtMinimal from './qt-starters/qt-minimal.json' with { type: 'json' };
import qtLfoFilter from './qt-starters/qt-lfo-filter.json' with { type: 'json' };
import qtMidi from './qt-starters/qt-midi.json' with { type: 'json' };
import qtPoly from './qt-starters/qt-poly.json' with { type: 'json' };
import qtChaos from './qt-starters/qt-chaos.json' with { type: 'json' };
import qtMacro from './qt-starters/qt-macro.json' with { type: 'json' };

export interface Starter { id: string; name: string; note: string; patch: () => PatchV2 }

const p = (
  modules: PatchV2['modules'],
  connections: PatchV2['connections'],
  positions: Record<string, [number, number]>,
  labels: Record<string, string> = {},
): PatchV2 => ({
  format: 'wirescape.native.patch', version: 2, modules, connections,
  ui: { positions, labels, view: { x: 0, y: 0, zoom: 1 } },
});

export const STARTERS: Starter[] = [
  {
    id: 'keyboard-voice',
    name: 'Keyboard voice',
    note: 'Keyboard → VCO → VCF → VCA → Output, with an envelope and an LFO on the cutoff.',
    patch: () => p(
      [
        { id: 1, module: 'host.keyboard' },
        { id: 2, module: 'essentials.vco', parameters: { frequency: 220 } },
        { id: 3, module: 'essentials.vcf', parameters: { cutoff: 900, resonance: 0.3 } },
        { id: 4, module: 'essentials.vca', parameters: { gain: 1 } },
        { id: 5, module: 'essentials.output' },
        { id: 6, module: 'essentials.adsr', parameters: { attack: 0.02, decay: 0.25, sustain: 0.6, release: 0.4 } },
        { id: 7, module: 'essentials.lfo', parameters: { rate: 0.6, depth: 0.8 } },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'pitch' }, to: { module: 2, port: 'pitch' } },
        { kind: 'signal', from: { module: 1, port: 'gate' }, to: { module: 6, port: 'gate' } },
        { kind: 'signal', from: { module: 2, port: 'audio' }, to: { module: 3, port: 'audio' } },
        { kind: 'signal', from: { module: 3, port: 'low24' }, to: { module: 4, port: 'audio' } },
        { kind: 'signal', from: { module: 6, port: 'envelope' }, to: { module: 4, port: 'gain_cv' } },
        { kind: 'signal', from: { module: 4, port: 'audio' }, to: { module: 5, port: 'audio' } },
        { kind: 'modulation', from: { module: 7, port: 'control' }, to: { module: 3, port: 'cutoff' }, amount: 0.35 },
      ],
      { 1: [40, 120], 2: [558, 120], 3: [1118, 120], 4: [1678, 120], 5: [2238, 110], 6: [558, 580], 7: [1334, 580] },
      { 2: 'Bass VCO' },
    ),
  },
  {
    id: 'drone',
    name: 'Two-source drone',
    note: 'A VCO and noise through the mixer into the ladder filter.',
    patch: () => p(
      [
        { id: 1, module: 'essentials.vco', parameters: { frequency: 110 } },
        { id: 2, module: 'essentials.noise', parameters: { level: 0.12 } },
        { id: 3, module: 'essentials.mixer', parameters: { level_a: 0.7, level_b: 0.3 } },
        { id: 4, module: 'essentials.ladder', parameters: { cutoff: 700, resonance: 0.45, drive: 0.4 } },
        { id: 5, module: 'essentials.output' },
        { id: 6, module: 'essentials.lfo', parameters: { rate: 0.12, depth: 1 } },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 3, port: 'a' } },
        { kind: 'signal', from: { module: 2, port: 'audio' }, to: { module: 3, port: 'b' } },
        { kind: 'signal', from: { module: 3, port: 'audio' }, to: { module: 4, port: 'audio' } },
        { kind: 'signal', from: { module: 4, port: 'audio' }, to: { module: 5, port: 'audio' } },
        { kind: 'modulation', from: { module: 6, port: 'control' }, to: { module: 4, port: 'cutoff' }, amount: 0.4 },
      ],
      { 1: [40, 100], 2: [40, 480], 3: [620, 240], 4: [1180, 240], 5: [1844, 250], 6: [1180, 820] },
    ),
  },
  {
    id: 'clocked-random',
    name: 'Clocked random',
    note: 'A clock steps the random source, which sweeps the filter through scale/offset.',
    patch: () => p(
      [
        { id: 1, module: 'essentials.clock', parameters: { bpm: 96, ticks_per_beat: 2 } },
        { id: 2, module: 'example.random_source', parameters: { smooth: 0.08, range: 1 } },
        { id: 3, module: 'essentials.scale_offset', parameters: { scale: 0.8, offset: 0.1 } },
        { id: 4, module: 'essentials.vco', parameters: { frequency: 160 } },
        { id: 5, module: 'essentials.vcf', parameters: { cutoff: 1200, resonance: 0.5 } },
        { id: 6, module: 'essentials.output' },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'clock' }, to: { module: 2, port: 'clock' } },
        { kind: 'signal', from: { module: 2, port: 'smooth' }, to: { module: 3, port: 'control' } },
        { kind: 'signal', from: { module: 4, port: 'audio' }, to: { module: 5, port: 'audio' } },
        { kind: 'signal', from: { module: 5, port: 'low12' }, to: { module: 6, port: 'audio' } },
        { kind: 'modulation', from: { module: 3, port: 'control' }, to: { module: 5, port: 'cutoff' }, amount: 0.5 },
      ],
      { 1: [40, 120], 2: [704, 110], 3: [1368, 160], 4: [704, 600], 5: [1368, 600], 6: [1928, 612] },
    ),
  },
  {
    id: 'sequence',
    name: 'Eight-step sequence',
    note: 'Clock into the step sequencer, driving a VCO and an envelope.',
    patch: () => p(
      [
        { id: 1, module: 'essentials.clock', parameters: { bpm: 110, ticks_per_beat: 2 } },
        { id: 2, module: 'example.step_sequencer', parameters: { length: 8, pitch1: 0, pitch2: 0.25, pitch3: -0.5, pitch4: 0.5 } },
        { id: 3, module: 'essentials.vco', parameters: { frequency: 220 } },
        { id: 4, module: 'essentials.adsr', parameters: { attack: 0.005, decay: 0.18, sustain: 0.2, release: 0.25 } },
        { id: 5, module: 'essentials.vca' },
        { id: 6, module: 'essentials.output' },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'clock' }, to: { module: 2, port: 'clock' } },
        { kind: 'signal', from: { module: 2, port: 'pitch' }, to: { module: 3, port: 'pitch' } },
        { kind: 'signal', from: { module: 2, port: 'gate' }, to: { module: 4, port: 'gate' } },
        { kind: 'signal', from: { module: 3, port: 'audio' }, to: { module: 5, port: 'audio' } },
        { kind: 'signal', from: { module: 4, port: 'envelope' }, to: { module: 5, port: 'gain_cv' } },
        { kind: 'signal', from: { module: 5, port: 'audio' }, to: { module: 6, port: 'audio' } },
      ],
      { 1: [40, 180], 2: [704, 120], 3: [1724, 120], 4: [704, 660], 5: [2284, 300], 6: [2844, 312] },
    ),
  },
  {
    id: 'macro',
    name: 'Macro voice',
    note: 'The macro oscillator played from the keyboard, with CV on timbre.',
    patch: () => p(
      [
        { id: 1, module: 'host.keyboard' },
        { id: 2, module: 'ports.macro_oscillator', parameters: { harmonics: 0.6, timbre: 0.4, morph: 0.5 } },
        { id: 3, module: 'essentials.output' },
        { id: 4, module: 'essentials.lfo', parameters: { rate: 0.25, depth: 1 } },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'pitch' }, to: { module: 2, port: 'pitch' } },
        { kind: 'signal', from: { module: 1, port: 'gate' }, to: { module: 2, port: 'trigger' } },
        { kind: 'signal', from: { module: 2, port: 'main' }, to: { module: 3, port: 'audio' } },
        { kind: 'modulation', from: { module: 4, port: 'control' }, to: { module: 2, port: 'timbre' }, amount: 0.3 },
      ],
      { 1: [40, 200], 2: [580, 80], 3: [1620, 200], 4: [580, 1100] },
    ),
  },
  {
    id: 'chaos',
    name: 'CHAOS-lite',
    note: 'The complex voice, played from the keyboard, out in stereo.',
    patch: () => p(
      [
        { id: 1, module: 'host.keyboard' },
        { id: 2, module: 'essentials.chaos_lite' },
        { id: 3, module: 'essentials.output' },
      ],
      [
        { kind: 'signal', from: { module: 1, port: 'pitch' }, to: { module: 2, port: 'pitch' } },
        { kind: 'signal', from: { module: 1, port: 'gate' }, to: { module: 2, port: 'gate' } },
        { kind: 'signal', from: { module: 1, port: 'velocity' }, to: { module: 2, port: 'velocity' } },
        { kind: 'signal', from: { module: 2, port: 'left' }, to: { module: 3, port: 'audio' } },
      ],
      { 1: [40, 300], 2: [580, 80], 3: [1760, 320] },
    ),
  },
];

export const DEFAULT_STARTER = STARTERS[0]!;

/**
 * Faithful imports of the Qt starter library (#187 / wirescape PR #192,
 * 96cad1cd7eeef673db6b5cbdcf09d4dc9008ab9d). Bundled JSON works offline.
 * Keep native x/y, settings, config and licence fields intact; bundled presets
 * route audio to both speakers, and ui.positions adapts spacing to browser cards.
 */
function qtStarter(id: string, name: string, note: string, source: unknown): Starter {
  return {
    id, name, note,
    patch: () => {
      const patch = structuredClone(source) as PatchV2;
      // Author bundled presets in stereo without changing generic patch imports.
      for (const route of patch.outputs ?? []) {
        if (route.bus_channel !== 0 || patch.outputs!.some(r => r.module === route.module
          && r.port === route.port && r.bus_channel === 1)) continue;
        patch.outputs!.push({ ...route, bus_channel: 1 });
      }
      patch.ui = {
        ...patch.ui,
        positions: Object.fromEntries(patch.modules.map(m => [String(m.id), [
          Math.round(Number(m.x ?? 0) * 2.6), Math.round(Number(m.y ?? 0) * 2.1),
        ] as [number, number]])),
        view: { x: 0, y: 0, zoom: 1 },
      };
      return patch;
    },
  };
}

STARTERS.push(
  qtStarter('qt-minimal', 'Qt · Minimal oscillator',
    'VCO → Output. A continuous tone in the real engine; this prototype makes no sound.', qtMinimal),
  qtStarter('qt-lfo-filter', 'Qt · LFO shaped amplitude',
    'VCO → VCF → VCA → Output; an LFO drives gain CV. Fake-engine visual preview only.', qtLfoFilter),
  qtStarter('qt-midi', 'Qt · MIDI pitch to oscillator',
    'MIDI In → MIDI to CV → VCO → Output. Requires an enabled MIDI device and the real engine for MIDI input and sound.', qtMidi),
  qtStarter('qt-poly', 'Qt · Four-voice keyboard chords',
    'Keyboard → VCO → Output, four voices. Play chords with the computer keyboard; sound requires the real polyphonic engine.', qtPoly),
  qtStarter('qt-chaos', 'Qt · CHAOS-lite',
    'Keyboard pitch → CHAOS-lite, left/right summed through Mixer → Output. Generic CHAOS panel; real-engine sound only.', qtChaos),
  qtStarter('qt-macro', 'Qt · Macro Oscillator',
    'Keyboard pitch, gate and velocity → Macro Oscillator → Output. Play notes with the computer keyboard; fake-engine visual preview only.', qtMacro),
);
