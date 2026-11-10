// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2 } from '../patch/types';
import { CURATED_PATCHES } from './curated';
import catalogue from '../../public/wirescape_modules.json' with { type: 'json' };
import midiKeysAndKnobs from './bundled/midi-keys-and-knobs.json' with { type: 'json' };

export interface Starter { id: string; name: string; note: string; patch: () => PatchV2 }

/** Builtins author their defaults explicitly; imported user patches stay untouched. */
function authoredStarter(patch: PatchV2): PatchV2 {
  const versions = new Map(catalogue.modules.map(m => [m.id, m.version]));
  return {
    ...patch,
    settings: patch.settings ?? { sample_rate: 48000, max_block_frames: 256, max_channels: 16, feedback_block: 1 },
    outputs: patch.outputs ?? patch.modules.filter(m => m.module === 'essentials.output')
      .flatMap(m => [0, 1].map(bus_channel => ({ module: m.id, port: 'audio', bus_channel }))),
    modules: patch.modules.map(m => ({ ...(!('version' in m) && versions.has(m.module)
      ? { version: versions.get(m.module) } : {}), ...m, parameters: { ...(m.parameters ?? {}) } })),
    ui: { positions: {}, labels: {}, view: { x: 0, y: 0, zoom: 1 }, ...patch.ui },
  };
}

const p = (
  modules: PatchV2['modules'],
  connections: PatchV2['connections'],
  positions: Record<string, [number, number]>,
  labels: Record<string, string> = {},
): PatchV2 => authoredStarter({
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

];

export const DEFAULT_STARTER = STARTERS[0]!;

// The keyboard voice remains the web audio smoke-test baseline. The curated
// snapshots stay as regression fixtures (hidden from the interactive list); the
// public library in apps/studio/patches/ supplies the shipped starter patches.
STARTERS.push(...CURATED_PATCHES.map(({ id, name, note, patch }) => ({
  id, name, note, patch: () => authoredStarter(structuredClone(patch)),
})));

/**
 * Patches bundled in the app instead of the public library: the shared library
 * (and the WebAssembly host) excludes hardware MIDI modules, so the MIDI example
 * ships here. Desktop/native only; the web Starter list hides it automatically
 * because its modules are not in the web registry. The six historical Qt
 * starters were retired in favour of the library's Basics patches.
 */
function bundledStarter(id: string, note: string, source: unknown): Starter {
  const title = (source as { name?: unknown }).name;
  const name = typeof title === 'string' ? title : id;
  return { id, name, note, patch: () => authoredStarter(structuredClone(source) as PatchV2) };
}

export const BUNDLED_STARTERS: Starter[] = [
  bundledStarter('midi-keys-and-knobs',
    'MIDI In → MIDI to CV and CC to CV: every MIDI output (pitch, gate, velocity, aftertouch, pressure, bend, mod wheel, notes, last note) and CC21–28 wired to sound controls. Desktop app with an enabled MIDI device.',
    midiKeysAndKnobs),
];
STARTERS.push(...BUNDLED_STARTERS);
