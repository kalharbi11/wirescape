// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2 } from '../patch/types';
import type { PatchDoc } from '../patch/PatchDoc';
import { GROOVE, engineConfig } from './pattern';

interface Applied { engine: string; raw: unknown }
const json = (v: unknown): string => JSON.stringify(v ?? null);
const clone = <T>(v: T): T => v === undefined ? v : JSON.parse(JSON.stringify(v)) as T;

/**
 * What the engine is playing for each Groove, versus the editable draft in
 * the document. Every whole-graph load goes through `project`, so an undo,
 * an engine restart or another module's reload never sends an unapplied
 * draft: only `apply` moves a draft to the engine (stopped Apply, #498).
 */
export class GrooveSession {
  private applied = new Map<number, Applied>();
  private listeners = new Set<() => void>();

  constructor(private readonly doc: PatchDoc) {
    doc.on(e => {
      if (e.t === 'module-added' && doc.modules.get(e.id)?.module === GROOVE) this.record(e.id);
      else if (e.t === 'module-removed') this.applied.delete(e.id);
      if (e.t === 'config' || e.t === 'reset' || e.t === 'module-added') this.changed();
    });
  }

  /** A whole new document is about to load: its drafts become what plays. */
  forget(): void { this.applied.clear(); this.changed(); }

  /** Preserve the actually running patterns around a trial document load that may refuse. */
  checkpoint(): () => void {
    const before = new Map(this.applied);
    return () => { this.applied = before; this.changed(); };
  }

  /** Execution copy of a full patch: each Groove carries its applied, known-fields-only config. */
  project(patch: PatchV2): PatchV2 {
    const out = clone(patch);
    for (const m of out.modules) {
      if (m.module !== GROOVE) continue;
      if (!this.applied.has(m.id)) this.record(m.id, m.config);
      const cfg = this.applied.get(m.id)!.engine;
      if (cfg === 'null') delete m.config; else m.config = JSON.parse(cfg) as unknown;
    }
    return out;
  }

  /** The draft differs from what plays (unknown keys never count). */
  isDraft(id: number): boolean {
    const a = this.applied.get(id);
    return !!a && a.engine !== json(engineConfig(this.doc.modules.get(id)?.config));
  }
  /** The applied pattern as the document held it (unknown keys kept), for Revert and change cues. */
  appliedRaw(id: number): unknown { return clone(this.applied.get(id)?.raw); }

  /** Mark the current draft as the one to send; returns an undo for a refused load. */
  stage(id: number): () => void {
    const before = this.applied.get(id);
    this.record(id);
    this.changed();
    return () => { if (before) this.applied.set(id, before); else this.applied.delete(id); this.changed(); };
  }

  onChange(cb: () => void): () => void { this.listeners.add(cb); return () => this.listeners.delete(cb); }
  changed(): void { for (const cb of [...this.listeners]) cb(); }

  private record(id: number, raw: unknown = this.doc.modules.get(id)?.config): void {
    this.applied.set(id, { engine: json(engineConfig(raw)), raw: clone(raw) });
  }
}
