// SPDX-License-Identifier: GPL-3.0-only
/**
 * Contract 3 — the patch file (Wirescape patch v2).
 *
 * Patches written by this prototype must open in the real app, so the shape
 * here is the real one. Unknown fields anywhere must survive load -> save.
 */

export type ConnectionKind = 'signal' | 'modulation' | 'event';

export interface Endpoint {
  module: number;
  /** A port id for signal/event; a *parameter* id when kind is 'modulation'. */
  port: string;
  [extra: string]: unknown;
}

export interface Connection {
  kind: ConnectionKind;
  from: Endpoint;
  to: Endpoint;
  /** Only for kind 'modulation'. -1..1. */
  amount?: number;
  [extra: string]: unknown;
}

export interface PatchModule {
  id: number;
  module: string;
  version?: number;
  parameters?: Record<string, number>;
  [extra: string]: unknown;
}

export interface PatchUi {
  positions?: Record<string, [number, number]>;
  view?: { x: number; y: number; zoom: number };
  /** Per-instance display name ("Bass VCO"), keyed by instance id. */
  labels?: Record<string, string>;
  /** User-picked cable colour per connection key, a palette name (DESIGN §9). No meaning. */
  cableColors?: Record<string, string>;
  [extra: string]: unknown;
}

export interface PatchV2 {
  format: 'wirescape.native.patch';
  version: 2;
  modules: PatchModule[];
  connections: Connection[];
  settings?: Record<string, unknown>;
  outputs?: { module: number; port: string; bus_channel: number; [extra: string]: unknown }[];
  ui?: PatchUi;
  [extra: string]: unknown;
}

export const EMPTY_PATCH = (): PatchV2 => ({
  format: 'wirescape.native.patch',
  version: 2,
  modules: [],
  connections: [],
  ui: { positions: {}, view: { x: 0, y: 0, zoom: 1 }, labels: {} },
});

/** Stable key for a connection, used for dedupe and DOM identity. */
export function connKey(c: Connection): string {
  return `${c.kind}|${c.from.module}:${c.from.port}|${c.to.module}:${c.to.port}`;
}

export function sameConn(a: Connection, b: Connection): boolean {
  return connKey(a) === connKey(b);
}
