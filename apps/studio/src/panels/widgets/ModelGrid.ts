// SPDX-License-Identifier: GPL-3.0-only
import { el } from '../../app/dom';
import type { PaneContext } from '../panes';
import './model-grid.css';

/** Pinned Plaits Voice::Init registration order: eurorack f4cfa7697095a28d292bff758beb51deb49b5a86, plaits/dsp/voice.cc. */
const MACRO_MODELS: ReadonlyArray<{ short: string; full: string }> = [
  { short: 'VA Filter', full: 'Virtual analog filter' },
  { short: 'Phase Dist', full: 'Phase distortion' },
  { short: '6-op FM 1', full: 'Six-operator FM preset 1' },
  { short: '6-op FM 2', full: 'Six-operator FM preset 2' },
  { short: '6-op FM 3', full: 'Six-operator FM preset 3' },
  { short: 'Terrain', full: 'Wave terrain' },
  { short: 'Str Mach', full: 'String machine' },
  { short: 'Chiptune', full: 'Chiptune' },
  { short: 'VA', full: 'Virtual analog' },
  { short: 'Waveshape', full: 'Waveshaping' },
  { short: 'FM', full: 'FM synthesis' },
  { short: 'Granular', full: 'Granular synthesis' },
  { short: 'Additive', full: 'Additive synthesis' },
  { short: 'Wavetable', full: 'Wavetable synthesis' },
  { short: 'Chords', full: 'Chord synthesis' },
  { short: 'Speech', full: 'Speech synthesis' },
  { short: 'Swarm', full: 'Swarm synthesis' },
  { short: 'Noise', full: 'Noise synthesis' },
  { short: 'Particle', full: 'Particle synthesis' },
  { short: 'String', full: 'String physical model' },
  { short: 'Modal', full: 'Modal resonator' },
  { short: 'Bass Drum', full: 'Bass drum' },
  { short: 'Snare Drum', full: 'Snare drum' },
  { short: 'Hi-Hat', full: 'Hi-hat' },
];

/** Direct selection for a bank of integer models; values come from the catalogue. */
export class ModelGrid {
  readonly root: HTMLElement;
  private readonly buttons = new Map<number, HTMLButtonElement>();
  private readonly parameter: string;
  private readonly heading?: HTMLElement;

  constructor(private readonly c: PaneContext) {
    this.parameter = typeof c.options.parameter === 'string' ? c.options.parameter : 'model';
    const p = c.param(this.parameter);
    this.root = el('div', 'model-bank');
    this.root.dataset.testid = `models-${c.moduleId}`;
    if (!p) return;
    this.heading = el('div', 'control-label', p.name);
    this.root.append(this.heading);
    const grid = el('div', 'model-grid');
    grid.setAttribute('role', 'radiogroup');
    grid.setAttribute('aria-label', p.name);
    const choices = p.choices?.length ? p.choices : Array.from(
      { length: Math.min(64, Math.floor((p.maximum - p.minimum) / (p.step || 1)) + 1) },
      (_, i) => ({ value: p.minimum + i * (p.step || 1), label: String(p.minimum + i * (p.step || 1)).padStart(2, '0') }),
    );
    const macro = c.descriptor.id === 'ports.macro_oscillator' && this.parameter === 'model';
    for (const choice of choices) {
      const engine = macro ? MACRO_MODELS[choice.value] : undefined;
      const b = el('button', 'model-button', engine ? '' : choice.label);
      if (engine) {
        b.append(el('span', 'model-index', String(choice.value).padStart(2, '0')),
          el('span', 'model-name', engine.short));
        b.title = engine.full;
      }
      b.type = 'button';
      b.dataset.testid = `model-${c.moduleId}-${choice.value}`;
      b.setAttribute('role', 'radio');
      b.setAttribute('aria-label', engine ? `${p.name} ${choice.value}: ${engine.full}` : `${p.name} ${choice.label}`);
      b.addEventListener('click', () => { c.setValue(this.parameter, choice.value); this.refresh(); });
      b.addEventListener('keydown', e => {
        const columns = getComputedStyle(grid).gridTemplateColumns.split(' ').length;
        const offsets: Record<string, number> = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -columns, ArrowDown: columns };
        const index = choices.findIndex(v => v.value === choice.value);
        let next = index;
        if (e.key in offsets) next = Math.max(0, Math.min(choices.length - 1, index + offsets[e.key]!));
        else if (e.key === 'Home') next = 0;
        else if (e.key === 'End') next = choices.length - 1;
        else return;
        e.preventDefault(); e.stopPropagation();
        const value = choices[next]!.value;
        c.setValue(this.parameter, value);
        this.refresh();
        this.buttons.get(value)?.focus();
      });
      this.buttons.set(choice.value, b);
      grid.append(b);
    }
    this.root.append(grid);
    this.refresh();
  }

  refresh(): void {
    const value = this.c.getValue(this.parameter);
    const engine = this.c.descriptor.id === 'ports.macro_oscillator' && this.parameter === 'model'
      ? MACRO_MODELS[value] : undefined;
    if (engine && this.heading) this.heading.textContent = `Model · ${engine.full} · model ${value}`;
    for (const [v, b] of this.buttons) {
      const selected = v === value;
      b.setAttribute('aria-checked', String(selected));
      b.tabIndex = selected ? 0 : -1;
    }
  }

  destroy(): void { /* no timers or telemetry subscriptions */ }
}
