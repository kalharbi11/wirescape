// SPDX-License-Identifier: GPL-3.0-only
import type { Telemetry } from '../engine/EngineClient';

export type Binding = (t: Telemetry) => void;

/**
 * The one animation loop.
 *
 * Every signal-driven visual registers a binding here; nothing else in the
 * app may own a timer or a CSS keyframe loop for motion that represents a
 * value. Bindings are keyed by module id so off-screen modules can be
 * skipped entirely (Pi rule 5).
 */
export class Ticker {
  private bindings = new Map<number, Set<Binding>>();
  /** Bindings that always run — the status bar, the FPS counter. */
  private global = new Set<Binding>();
  private visible = new Set<number>();
  private latest: Telemetry | null = null;
  private raf = 0;
  private running = false;

  /** Draw at most this many times a second. 0 = every frame. */
  throttleHz = 0;
  private lastDraw = 0;

  /** Rolling frame rate, for the dev panel. */
  fps = 0;
  private frames = 0;
  private fpsSince = 0;
  private frameMs = 0;
  /** Mean time spent inside the loop applying telemetry, in ms. */
  get applyMs(): number { return Math.round(this.frameMs * 100) / 100; }

  bind(moduleId: number, fn: Binding): () => void {
    let set = this.bindings.get(moduleId);
    if (!set) { set = new Set(); this.bindings.set(moduleId, set); }
    set.add(fn);
    return () => { set!.delete(fn); if (!set!.size) this.bindings.delete(moduleId); };
  }

  bindGlobal(fn: Binding): () => void {
    this.global.add(fn);
    return () => this.global.delete(fn);
  }

  dropModule(moduleId: number, preserveTelemetry = false): void {
    this.bindings.delete(moduleId);
    this.visible.delete(moduleId);
    // IDs can be reused after loading a patch or undoing. Never seed a new
    // card with an effective value retained from the previous module.
    if (this.latest && !preserveTelemetry) {
      const prev = this.latest;
      const t = this.latest = {
        ...prev, effective: { ...prev.effective }, effective_pitch: { ...prev.effective_pitch }, phase: { ...prev.phase },
        envelope: { ...prev.envelope }, meters: { ...prev.meters },
        scope: { ...prev.scope }, eventActivity: { ...prev.eventActivity },
        outputs: prev.outputs ? { ...prev.outputs } : undefined,
      };
      delete t.effective[moduleId];
      if (t.effective_pitch) delete t.effective_pitch[moduleId];
      delete t.phase[moduleId];
      delete t.envelope[moduleId];
      delete t.meters[moduleId];
      delete t.scope[moduleId];
      for (const key of Object.keys(t.eventActivity)) {
        if (key.startsWith(`${moduleId}:`)) delete t.eventActivity[key];
      }
      if (t.outputs) for (const key of Object.keys(t.outputs)) {
        if (key.startsWith(`${moduleId}:`)) delete t.outputs[key];
      }
    }
  }

  setVisible(ids: Iterable<number>): void {
    this.visible = new Set(ids);
  }

  push(t: Telemetry): void {
    // Complete snapshots: an absent key is unavailable, never a retained value.
    this.latest = {
      ...t, effective: t.effective ?? {}, effective_pitch: t.effective_pitch ?? {}, phase: t.phase ?? {},
      envelope: t.envelope ?? {}, eventActivity: t.eventActivity ?? {},
    };
  }

  /** A graph replacement starts a new telemetry epoch, even if IDs repeat. */
  resetTelemetry(): void { this.latest = null; }

  /** The most recent telemetry frame, for readers outside a binding (info boxes). */
  get last(): Telemetry | null { return this.latest; }

  start(): void {
    if (this.running) return;
    this.running = true;
    this.fpsSince = performance.now();
    const loop = (now: number) => {
      if (!this.running) return;
      this.raf = requestAnimationFrame(loop);

      this.frames++;
      if (now - this.fpsSince >= 500) {
        this.fps = Math.round((this.frames * 1000) / (now - this.fpsSince));
        this.frames = 0; this.fpsSince = now;
      }

      const t = this.latest;
      if (!t) return;
      if (this.throttleHz > 0 && now - this.lastDraw < 1000 / this.throttleHz) return;
      this.lastDraw = now;

      const started = performance.now();
      for (const fn of this.global) fn(t);
      for (const id of this.visible) {
        const set = this.bindings.get(id);
        if (!set) continue;
        for (const fn of set) fn(t);
      }
      const spent = performance.now() - started;
      this.frameMs = this.frameMs * 0.9 + spent * 0.1;
    };
    this.raf = requestAnimationFrame(loop);
  }

  stop(): void { this.running = false; cancelAnimationFrame(this.raf); }
}
