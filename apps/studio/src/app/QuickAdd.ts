import type { ModuleRegistry } from '../modules/registry';
import { inputs, outputs } from '../modules/registry';
import type { PortDirection } from '../modules/types';
import { el } from './dom';

export interface QuickAddRequest {
  /** Client coordinates to open at. */
  at: { x: number; y: number };
  /** When set, only modules with a matching free port are offered. */
  need: { direction: PortDirection; kind: string } | null;
  choose(type: string): void;
  cancel(): void;
}

/** The double-click / cable-drop search palette. */
export class QuickAdd {
  readonly root: HTMLElement;
  private input: HTMLInputElement;
  private list: HTMLElement;
  private req: QuickAddRequest | null = null;
  private items: string[] = [];
  private active = 0;

  constructor(private readonly registry: ModuleRegistry) {
    this.root = el('div', 'quick-add');
    this.root.dataset.testid = 'quick-add';
    this.root.style.display = 'none';
    this.root.setAttribute('role', 'dialog');
    this.root.setAttribute('aria-label', 'Add a module');

    this.input = el('input', 'quick-input') as HTMLInputElement;
    this.input.type = 'search';
    this.input.placeholder = 'Add a module…';
    this.input.dataset.testid = 'quick-add-input';
    this.list = el('div', 'quick-list');
    this.root.append(this.input, this.list);

    this.input.addEventListener('input', () => this.render());
    this.input.addEventListener('keydown', e => {
      if (e.key === 'Escape') { e.preventDefault(); this.close(true); }
      if (e.key === 'ArrowDown') { e.preventDefault(); this.move(1); }
      if (e.key === 'ArrowUp') { e.preventDefault(); this.move(-1); }
      if (e.key === 'Enter') {
        e.preventDefault();
        const t = this.items[this.active];
        if (t) this.pick(t);
      }
    });
    document.addEventListener('pointerdown', e => {
      if (this.req && !this.root.contains(e.target as Node)) this.close(true);
    });
  }

  open(req: QuickAddRequest): void {
    this.req = req;
    this.root.style.display = '';
    this.root.style.left = `${Math.max(12, Math.min(window.innerWidth - 332, req.at.x))}px`;
    this.root.style.top = `${Math.max(12, Math.min(window.innerHeight - 330, req.at.y))}px`;
    this.input.value = '';
    this.active = 0;
    this.render();
    this.input.focus();
  }

  close(cancelled = false): void {
    if (cancelled) this.req?.cancel();
    this.req = null;
    this.root.style.display = 'none';
  }

  get isOpen(): boolean { return this.req !== null }

  private candidates(): string[] {
    const need = this.req?.need;
    let list = this.registry.search(this.input.value);
    if (need) {
      list = list.filter(m => {
        const ports = need.direction === 'input' ? inputs(m) : outputs(m);
        return ports.some(p => p.kind === need.kind);
      });
    }
    return list.map(m => m.id);
  }

  private render(): void {
    this.items = this.candidates();
    if (this.active >= this.items.length) this.active = 0;
    this.list.replaceChildren();
    if (!this.items.length) {
      this.list.append(el('div', 'quick-empty',
        this.req?.need ? 'No module has a matching port.' : 'No module matches that.'));
      return;
    }
    this.items.forEach((id, i) => {
      const m = this.registry.get(id)!;
      const row = el('button', `quick-row${i === this.active ? ' active' : ''}`) as HTMLButtonElement;
      row.type = 'button';
      row.dataset.testid = `quick-add-${id}`;
      row.append(el('span', 'quick-name', m.name), el('span', 'quick-cat', m.category));
      row.addEventListener('click', () => this.pick(id));
      row.addEventListener('pointerenter', () => { this.active = i; this.paintActive(); });
      this.list.append(row);
    });
  }

  private paintActive(): void {
    [...this.list.children].forEach((c, i) => c.classList.toggle('active', i === this.active));
  }

  private move(d: number): void {
    if (!this.items.length) return;
    this.active = (this.active + d + this.items.length) % this.items.length;
    this.paintActive();
    this.list.children[this.active]?.scrollIntoView({ block: 'nearest' });
  }

  private pick(type: string): void {
    const req = this.req;
    this.req = null;
    this.root.style.display = 'none';
    req?.choose(type);
  }
}
