// SPDX-License-Identifier: GPL-3.0-only
import type { EngineClient } from '../engine/EngineClient';

/** Two octaves on the computer keyboard; Z and X shift the octave. */
const MAP: Record<string, number> = {
  KeyA: 0, KeyW: 1, KeyS: 2, KeyE: 3, KeyD: 4, KeyF: 5, KeyT: 6, KeyG: 7,
  KeyY: 8, KeyH: 9, KeyU: 10, KeyJ: 11, KeyK: 12, KeyO: 13, KeyL: 14,
  KeyP: 15, Semicolon: 16, Quote: 17,
};

export class ComputerKeyboard {
  private octave = 4;
  private enabled = true;
  private held = new Map<string, number>();
  onOctaveChange: (o: number) => void = () => {};
  onNotesChange: () => void = () => {};

  constructor(private readonly engine: EngineClient) {}

  get currentOctave(): number { return this.octave }

  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
    if (!enabled) this.releaseAll('computer:');
  }

  setOctave(octave: number): void {
    this.octave = Math.max(0, Math.min(8, octave));
    this.onOctaveChange(this.octave);
  }

  /** Local input sources share note ownership; one release must not cut off another. */
  press(source: string, note: number): void {
    if (this.held.has(source)) return;
    const sounding = this.isHeld(note);
    this.held.set(source, note);
    if (!sounding) this.engine.noteOn(note, 0.8);
    this.onNotesChange();
  }

  release(source: string): void {
    const note = this.held.get(source);
    if (note === undefined) return;
    this.held.delete(source);
    if (!this.isHeld(note)) this.engine.noteOff(note);
    this.onNotesChange();
  }

  isHeld(note: number): boolean { return [...this.held.values()].includes(note); }

  releaseAll(prefix = ''): void {
    for (const source of [...this.held.keys()]) if (source.startsWith(prefix)) this.release(source);
  }

  attach(): () => void {
    const down = (e: KeyboardEvent) => {
      if (!this.enabled || !canPlayNotes()) return;
      if (e.code === 'KeyZ' && !e.metaKey && !e.ctrlKey && !e.altKey) {
        this.setOctave(this.octave - 1); return;
      }
      if (e.code === 'KeyX' && !e.metaKey && !e.ctrlKey && !e.altKey) {
        this.setOctave(this.octave + 1); return;
      }
      const semi = MAP[e.code];
      if (semi === undefined || e.repeat || this.held.has(`computer:${e.code}`) || e.metaKey || e.ctrlKey || e.altKey) return;
      e.preventDefault();
      const note = this.octave * 12 + semi;
      this.press(`computer:${e.code}`, note);
    };
    const up = (e: KeyboardEvent) => {
      const semi = MAP[e.code];
      if (semi === undefined) return;
      this.release(`computer:${e.code}`);
    };
    const blur = () => {
      this.releaseAll();
    };
    const visibility = () => { if (document.hidden) blur(); };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', blur);
    document.addEventListener('visibilitychange', visibility);
    return () => {
      blur();
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
      window.removeEventListener('blur', blur);
      document.removeEventListener('visibilitychange', visibility);
    };
  }
}

export function canPlayNotes(): boolean {
  return !isTyping() && ![...document.querySelectorAll<HTMLElement>('.modal-backdrop')]
    .some(d => d.style.display !== 'none');
}

export function isTyping(): boolean {
  const a = document.activeElement;
  return !!a && (['INPUT', 'TEXTAREA', 'SELECT'].includes(a.tagName) || (a as HTMLElement).isContentEditable);
}
