// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2 } from '../patch/types';
import { CURATED_PATCHES } from './curated';
import catalogue from '../../public/wirescape_modules.json' with { type: 'json' };
import qtMinimal from './qt-starters/qt-minimal.json' with { type: 'json' };
import qtLfoFilter from './qt-starters/qt-lfo-filter.json' with { type: 'json' };
import qtMidi from './qt-starters/qt-midi.json' with { type: 'json' };
import qtPoly from './qt-starters/qt-poly.json' with { type: 'json' };
import qtChaos from './qt-starters/qt-chaos.json' with { type: 'json' };
import qtMacro from './qt-starters/qt-macro.json' with { type: 'json' };

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

// The keyboard voice remains the web audio smoke-test baseline. The five other
// generic starters leave the interactive catalogue; native test fixtures remain
// frozen for historical conformance. Bundled Qt/class-demo session patches are kept.
STARTERS.push(...CURATED_PATCHES.map(({ id, name, note, patch }) => ({
  id, name, note, patch: () => authoredStarter(structuredClone(patch)),
})));

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
      return authoredStarter(patch);
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
