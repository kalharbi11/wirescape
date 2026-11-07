// SPDX-License-Identifier: GPL-3.0-only
import { ComputerKeyboard, canPlayNotes } from './Keyboard';
import { el } from './dom';

const NAMES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];

/** Event-driven local input. Pressed keys describe input ownership, not engine telemetry. */
export class OnScreenKeyboard {
  readonly root = el('section', 'on-screen-keyboard');
  private keys: HTMLButtonElement[] = [];
  private octave = el('span', 'keyboard-octave');
  private down: HTMLButtonElement;
  private up: HTMLButtonElement;

  constructor(private readonly keyboard: ComputerKeyboard) {
    this.root.id = 'on-screen-keyboard';
    this.root.dataset.testid = 'on-screen-keyboard';
    this.root.setAttribute('aria-label', 'Playable keyboard');
    this.root.hidden = true;
    const controls = el('div', 'keyboard-controls');
    this.down = this.button('−', 'Keyboard octave down', () => keyboard.setOctave(keyboard.currentOctave - 1));
    this.up = this.button('+', 'Keyboard octave up', () => keyboard.setOctave(keyboard.currentOctave + 1));
    this.down.dataset.testid = 'keyboard-octave-down';
    this.up.dataset.testid = 'keyboard-octave-up';
    controls.append(this.down, this.octave, this.up);
    const keys = el('div', 'keyboard-keys');
    for (let semi = 0; semi < 18; semi++) {
      const key = el('button', 'piano-key') as HTMLButtonElement;
      key.type = 'button';
      key.dataset.testid = `piano-key-${semi}`;
      key.classList.toggle('accidental', NAMES[semi % 12].includes('♯'));
      key.addEventListener('pointerdown', e => {
        if (e.button !== 0 || this.root.hidden) return;
        key.focus({ preventScroll: true });
        if (!canPlayNotes()) return;
        e.preventDefault();
        key.setPointerCapture(e.pointerId);
        keyboard.press(`screen:pointer:${e.pointerId}`, keyboard.currentOctave * 12 + semi);
      });
      const release = (e: PointerEvent) => keyboard.release(`screen:pointer:${e.pointerId}`);
      key.addEventListener('pointerup', release);
      key.addEventListener('pointercancel', release);
      key.addEventListener('lostpointercapture', release);
      key.addEventListener('keydown', e => {
        if (e.code !== 'Space' && e.code !== 'Enter') return;
        e.preventDefault();
        if (!e.repeat && !this.root.hidden && canPlayNotes() && !e.ctrlKey && !e.metaKey && !e.altKey)
          keyboard.press(`screen:key:${semi}:${e.code}`, keyboard.currentOctave * 12 + semi);
      });
      const releaseKeys = () => keyboard.releaseAll(`screen:key:${semi}:`);
      key.addEventListener('keyup', e => {
        if (e.code === 'Space' || e.code === 'Enter') {
          e.preventDefault(); keyboard.release(`screen:key:${semi}:${e.code}`);
        }
      });
      key.addEventListener('blur', releaseKeys);
      this.keys.push(key);
      keys.append(key);
    }
    this.root.append(controls, keys);
    keyboard.onNotesChange = () => this.refresh();
    this.refresh();
  }

  private button(label: string, name: string, click: () => void): HTMLButtonElement {
    const b = el('button', 'action', label) as HTMLButtonElement;
    b.type = 'button'; b.setAttribute('aria-label', name); b.addEventListener('click', click);
    return b;
  }

  toggle(): boolean {
    this.root.hidden = !this.root.hidden;
    if (this.root.hidden) this.keyboard.releaseAll('screen:');
    return !this.root.hidden;
  }

  refresh(): void {
    const octave = this.keyboard.currentOctave;
    this.octave.textContent = `OCT ${octave}`;
    this.down.disabled = octave === 0;
    this.up.disabled = octave === 8;
    this.keys.forEach((key, semi) => {
      const name = NAMES[semi % 12] + (octave + Math.floor(semi / 12));
      key.textContent = name;
      key.setAttribute('aria-label', `Play ${name}`);
      key.setAttribute('aria-pressed', String(this.keyboard.isHeld(octave * 12 + semi)));
    });
  }
}
