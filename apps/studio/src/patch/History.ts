// SPDX-License-Identifier: GPL-3.0-only
import type { PatchDoc } from './PatchDoc';
import type { PatchV2 } from './types';

/**
 * Undo/redo for the patch document (phase 4).
 *
 * Snapshot-based: after each user gesture the whole patch (minus the view)
 * is serialised; undo restores the previous snapshot. A patch of 20 modules
 * serialises in well under a millisecond, and snapshots make every edit
 * undoable without each feature writing its own inverse.
 *
 * One gesture is one step (brief: "one knob drag is one step; one
 * multi-cable stereo connection is one step"):
 *  - changes made while a pointer is down are committed on pointerup, so a
 *    knob drag, a module drag, a cable drop or a mapping click is one step;
 *  - other changes (keys, typed values, paste, delete) are committed after
 *    350 ms of quiet, so holding an arrow key is one step too.
 */
export class History {
  private past: string[] = [];
  private future: string[] = [];
  private current: string;
  private pending = false;
  private suppress = false;
  private pointerDown = false;
  private finishingPointer = false;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private listeners = new Set<() => void>();
  readonly limit = 200;

  constructor(private readonly doc: PatchDoc, private readonly restore: (p: PatchV2) => void) {
    this.current = this.snap();
    doc.on(e => {
      if (this.suppress || e.t === 'view' || e.t === 'reset') return;
      this.schedule();
    });
    window.addEventListener('pointerdown', () => {
      // A new gesture must not absorb a keyboard/typed edit still debouncing.
      if (!this.pointerDown) this.flush();
      this.pointerDown = true;
    }, true);
    const up = () => {
      this.pointerDown = false;
      // The click event runs after pointerup and may itself change the doc.
      // Keep its edits in this gesture's end-of-task commit.
      this.finishingPointer = true;
      this.later(0);
    };
    window.addEventListener('pointerup', up, true);
    window.addEventListener('pointercancel', up, true);
    window.addEventListener('blur', up);
  }

  /** Start over from the document as it is (after a load or a new patch). */
  reset(): void {
    this.clearTimer();
    this.pending = false;
    this.finishingPointer = false;
    this.past = [];
    this.future = [];
    this.current = this.snap();
    this.changed();
  }

  /** Save the current document's history before a provisional patch load. */
  checkpoint(): () => void {
    this.flush();
    const past = [...this.past], future = [...this.future], current = this.current;
    return () => {
      this.clearTimer();
      this.pending = false;
      this.finishingPointer = false;
      this.past = [...past];
      this.future = [...future];
      this.current = current;
      this.changed();
    };
  }

  get canUndo(): boolean { return this.past.length > 0 || (this.pending && this.snap() !== this.current); }
  get canRedo(): boolean { return this.future.length > 0; }
  get depth(): { past: number; future: number } { return { past: this.past.length, future: this.future.length }; }

  /** Commit any pending change now (call before saving or undoing). */
  flush(): void {
    this.clearTimer();
    this.finishingPointer = false;
    if (!this.pending) return;
    this.pending = false;
    const s = this.snap();
    if (s === this.current) return;
    this.past.push(this.current);
    if (this.past.length > this.limit) this.past.shift();
    this.current = s;
    this.future = [];
    this.changed();
  }

  undo(): boolean {
    this.flush();
    const prev = this.past.pop();
    if (prev === undefined) return false;
    this.future.push(this.current);
    this.current = prev;
    this.apply(prev);
    return true;
  }

  redo(): boolean {
    this.flush();
    const next = this.future.pop();
    if (next === undefined) return false;
    this.past.push(this.current);
    this.current = next;
    this.apply(next);
    return true;
  }

  onChange(cb: () => void): () => void { this.listeners.add(cb); return () => this.listeners.delete(cb); }

  // ---- internals ---------------------------------------------------------

  private apply(s: string): void {
    this.suppress = true;
    try { this.restore(JSON.parse(s) as PatchV2); }
    finally { this.suppress = false; }
    this.changed();
  }

  private schedule(): void {
    this.pending = true;
    if (this.pointerDown) { this.clearTimer(); return; }
    this.later(this.finishingPointer ? 0 : 350);
  }

  private later(ms: number): void {
    this.clearTimer();
    this.timer = setTimeout(() => { this.timer = null; this.flush(); }, ms);
  }

  private clearTimer(): void {
    if (this.timer !== null) { clearTimeout(this.timer); this.timer = null; }
  }

  private snap(): string {
    const j = this.doc.toJSON();
    const { view: _view, ...ui } = j.ui ?? {};
    return JSON.stringify({ ...j, ui });
  }

  private changed(): void { for (const l of [...this.listeners]) l(); }
}
