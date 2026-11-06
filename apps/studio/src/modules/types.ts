// SPDX-License-Identifier: GPL-3.0-only
/**
 * Contract 1 — module data.
 *
 * Mirrors `public/wirescape_modules.json` exactly. Nothing in the UI may
 * hand-code a module: every card is built from one of these descriptors.
 */

export type PortDirection = 'input' | 'output';

/** `signal` = audio-rate cable. `event` = notes / MIDI messages. */
export type PortKind = 'signal' | 'event';

/**
 * Advisory only — sets colour and icon. Hints NEVER block a connection;
 * only signal<->event mismatches are refused. (Brief, "Module data".)
 */
export type PortHint =
  | 'audio' | 'pitch' | 'cv_uni' | 'cv_bi' | 'gate' | 'trigger'
  | 'ev_notes' | 'ev_midi';

export type PortPoly = 'poly' | 'sum' | 'first';

export interface ModulePort {
  id: string;
  name: string;
  direction: PortDirection;
  kind: PortKind;
  hint: PortHint | string;
  poly: PortPoly | string;
  stereo_pair: 'stereo' | null;
}

export interface ParamChoice { value: number; label: string }

export interface ModuleParam {
  id: string;
  name: string;
  minimum: number;
  maximum: number;
  default: number;
  unit: string;
  taper: 'linear' | 'log' | string;
  /** Non-zero means the parameter snaps to this increment. */
  step: number;
  decimals: number;
  modulatable: boolean;
  choices?: ParamChoice[];
}

export interface ModuleDescriptor {
  id: string;
  name: string;
  category: string;
  ports: ModulePort[];
  parameters: ModuleParam[];
}

export interface ModuleCatalogFile {
  note?: string;
  modules: ModuleDescriptor[];
}

/**
 * Port identity.
 *
 * The real catalogue reuses the same port `id` for an input and an output on
 * the same module (essentials.ladder, essentials.output, essentials.vca,
 * essentials.scale_offset, essentials.slew all have "audio"/"control" on both
 * sides). So a port is only unique as (direction, id) and every lookup in the
 * UI must key on both. Patch v2 endpoints stay `{module, port}` because the
 * direction is implied by which side of the connection they sit on.
 */
export type PortRef = `${PortDirection}:${string}`;
export const portRef = (d: PortDirection, id: string): PortRef => `${d}:${id}`;
