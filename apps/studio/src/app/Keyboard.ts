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
  private held = new Map<string, number>();
  onOctaveChange: (o: number) => void = () => {};

  constructor(private readonly engine: EngineClient) {}

  get currentOctave(): number { return this.octave }

  attach(): () => void {
    const down = (e: KeyboardEvent) => {
      if (isTyping() || [...document.querySelectorAll<HTMLElement>('.modal-backdrop')].some(d => d.style.display !== 'none')) return;
      if (e.code === 'KeyZ' && !e.metaKey && !e.ctrlKey) {
        this.octave = Math.max(0, this.octave - 1); this.onOctaveChange(this.octave); return;
      }
      if (e.code === 'KeyX' && !e.metaKey && !e.ctrlKey) {
        this.octave = Math.min(8, this.octave + 1); this.onOctaveChange(this.octave); return;
      }
      const semi = MAP[e.code];
      if (semi === undefined || e.repeat || this.held.has(e.code) || e.metaKey || e.ctrlKey || e.altKey) return;
      e.preventDefault();
      const note = this.octave * 12 + semi;
      this.held.set(e.code, note);
      this.engine.noteOn(note, 0.8);
    };
    const up = (e: KeyboardEvent) => {
      const semi = MAP[e.code];
      if (semi === undefined || !this.held.has(e.code)) return;
      const note = this.held.get(e.code)!;
      this.held.delete(e.code);
      this.engine.noteOff(note);
    };
    const blur = () => {
      for (const note of this.held.values()) this.engine.noteOff(note);
      this.held.clear();
    };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', blur);
    return () => {
      blur();
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
      window.removeEventListener('blur', blur);
    };
  }
}

export function isTyping(): boolean {
  const a = document.activeElement;
  return !!a && (['INPUT', 'TEXTAREA', 'SELECT'].includes(a.tagName) || (a as HTMLElement).isContentEditable);
}
