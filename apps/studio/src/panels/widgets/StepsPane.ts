// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleParam } from '../../modules/types';
import type { Ticker } from '../../runtime/Ticker';
import { el } from '../../app/dom';
import { NumberBox } from './NumberBox';
import '../../styles/sequence.css';

export interface StepsOptions {
  moduleId: number;
  ticker: Ticker;
  steps: number;
  param(id: string): ModuleParam | undefined;
  getValue(id: string): number;
  setValue(id: string, v: number, commit?: boolean): void;
}

/** Eight editable lanes; live position is read only from engine telemetry. */
export class StepsPane {
  readonly root: HTMLElement;
  private cells: HTMLElement[] = [];
  private gates: HTMLButtonElement[] = [];
  private numbers: NumberBox[] = [];
  private unbind: () => void;
  private current = -1;

  constructor(private readonly o: StepsOptions) {
    this.root = el('div', 'steps-pane signature-sequence');
    this.root.dataset.testid = `steps-${o.moduleId}`;
    const head = el('div', 'pane-head');
    head.append(el('span', '', 'SEQUENCE'), el('span', '', 'PITCH · GATE · VELOCITY'));
    this.root.append(head);

    const grid = el('div', 'steps-grid');
    grid.style.gridTemplateColumns = `repeat(${o.steps}, minmax(0,1fr))`;
    for (let i = 1; i <= o.steps; i++) {
      const cell = el('div', 'step-cell');
      cell.dataset.step = String(i);
      cell.dataset.testid = `step-${o.moduleId}-${i}`;
      cell.append(el('div', 'step-index', String(i).padStart(2, '0')));

      const pitch = this.number(`pitch${i}`, 'PITCH');
      if (pitch) cell.append(pitch.root);
      const gate = el('button', 'step-gate') as HTMLButtonElement;
      gate.type = 'button';
      gate.dataset.testid = `step-gate-${o.moduleId}-${i}`;
      gate.addEventListener('click', () => {
        const p = o.param(`gate${i}`);
        if (!p) return;
        const choices = p.choices ?? [];
        const at = choices.findIndex(c => c.value === o.getValue(p.id));
        const next = choices.length ? choices[(at + 1) % choices.length]!.value
          : (o.getValue(p.id) + 1) % (p.maximum + 1);
        o.setValue(p.id, next, true);
        this.refresh();
      });
      cell.append(gate);
      const velocity = this.number(`vel${i}`, 'VEL');
      if (velocity) cell.append(velocity.root);
      grid.append(cell);
      this.cells.push(cell);
      this.gates.push(gate);
    }
    this.root.append(grid);
    this.refresh();

    this.unbind = o.ticker.bind(o.moduleId, t => {
      const ph = t.phase[o.moduleId];
      const length = this.length();
      const idx = ph === undefined || o.getValue('run') < 0.5 ? -1
        : Math.max(0, Math.min(length - 1, Math.floor(ph * length + 1e-6)));
      if (idx === this.current) return;
      this.cells[this.current]?.classList.remove('playing');
      this.current = idx;
      this.cells[idx]?.classList.add('playing');
    });
  }

  private number(id: string, label: string): NumberBox | undefined {
    const p = this.o.param(id);
    if (!p) return;
    const number = new NumberBox({
      param: { ...p, name: label }, moduleId: this.o.moduleId,
      getValue: () => this.o.getValue(id),
      setValue: (v, commit) => this.o.setValue(id, v, commit),
    });
    number.root.setAttribute('aria-label', p.name);
    this.numbers.push(number);
    return number;
  }

  private length(): number {
    return Math.max(1, Math.min(this.o.steps, Math.round(this.o.getValue('length') || this.o.steps)));
  }

  refresh(): void {
    const length = this.length();
    for (let i = 1; i <= this.o.steps; i++) {
      const cell = this.cells[i - 1]!;
      const p = this.o.param(`gate${i}`);
      const value = this.o.getValue(`gate${i}`);
      const gate = this.gates[i - 1]!;
      const label = p?.choices?.find(c => c.value === value)?.label ?? String(value);
      gate.textContent = label;
      gate.dataset.state = label.toLowerCase();
      gate.title = `Step ${i} gate: ${label} · click to cycle Rest / Note / Tie`;
      gate.setAttribute('aria-label', `Step ${i} gate: ${label}`);
      cell.classList.toggle('outside-length', i > length);
      // A tie joins the preceding step's gate; the first step wraps to the last.
      cell.classList.toggle('tied', value === 2);
      if ((i > length || this.o.getValue('run') < 0.5) && this.current === i - 1) {
        cell.classList.remove('playing');
        this.current = -1;
      }
    }
    for (const number of this.numbers) number.refresh();
  }

  destroy(): void {
    this.unbind();
    for (const number of this.numbers) number.destroy();
  }
}
