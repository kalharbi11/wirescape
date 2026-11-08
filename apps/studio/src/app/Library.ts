// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleDescriptor } from '../modules/types';
import type { ModuleRegistry } from '../modules/registry';
import { inputs, outputs } from '../modules/registry';
import { el } from './dom';

export interface LibraryHooks {
  /** Click-to-add: the module lands in the middle of the view. */
  add(type: string): void;
  /** Drop-to-add: the module lands where it was dropped. */
  addAt(type: string, clientX: number, clientY: number): void;
}

export class Library {
  readonly root: HTMLElement;
  private list: HTMLElement;
  private countEl: HTMLElement;
  private search: HTMLInputElement;

  constructor(private readonly registry: ModuleRegistry, private readonly hooks: LibraryHooks) {
    this.root = el('aside', 'library');
    this.root.dataset.testid = 'library';
    this.root.id = 'module-library';
    this.root.setAttribute('aria-label', 'Module library');

    const brand = el('div', 'brand');
    const h1 = el('h1', '', 'wirescape');
    brand.append(h1, el('div', 'brand-caption', 'AN OPEN PATCHING ENVIRONMENT\nINTERFACE STUDIES / 2026'));
    this.root.append(brand);

    const head = el('div', 'library-head');
    const title = el('span', 'micro', 'MODULES');
    this.countEl = el('span', 'library-count', `${registry.all.length} available`);
    head.append(title, this.countEl);

    this.search = el('input', 'library-search') as HTMLInputElement;
    this.search.type = 'search';
    this.search.placeholder = 'Search modules…';
    this.search.setAttribute('aria-label', 'Search modules');
    this.search.dataset.testid = 'library-search';
    this.search.addEventListener('input', () => this.render(this.search.value));

    this.list = el('div', 'library-list');
    this.list.dataset.testid = 'library-list';

    const body = el('div', 'library-body');
    body.append(head, this.search, this.list);
    this.root.append(body);

    const foot = el('div', 'library-foot');
    foot.append(el('div', 'micro', 'PORT RINGS'));
    for (const [hint, label] of [
      ['audio', 'Audio'], ['pitch', 'Pitch'], ['cv', 'CV'],
      ['gate', 'Gate / trigger'], ['events', 'Events'],
    ] as const) {
      const row = el('div', 'legend-row');
      const ring = el('i', 'legend-ring');
      ring.style.setProperty('--hint', `var(--hint-${hint})`);
      row.append(ring, document.createTextNode(label));
      foot.append(row);
    }
    foot.append(el('div', 'library-note', 'MODULAR SYNTH · PATCH AND PLAY'));
    this.root.append(foot);

    this.render('');
  }

  focusSearch(): void { this.search.focus(); this.search.select(); }
  refresh(): void { this.render(this.search.value); }

  private render(query: string): void {
    const matches = this.registry.search(query);
    this.countEl.textContent = `${matches.length} of ${this.registry.all.length}`;
    this.list.replaceChildren();
    if (!matches.length) {
      this.list.append(el('div', 'library-empty', 'No module matches that.'));
      return;
    }
    const seen = new Set<string>();
    for (const m of matches) {
      if (!seen.has(m.category)) {
        seen.add(m.category);
        this.list.append(el('div', 'library-group', m.category.toUpperCase()));
      }
      this.list.append(this.item(m));
    }
  }

  private item(m: ModuleDescriptor): HTMLElement {
    const b = el('button', 'library-item') as HTMLButtonElement;
    b.type = 'button';
    b.draggable = true;
    b.dataset.type = m.id;
    b.dataset.testid = `library-item-${m.id}`;
    b.title = `${m.name} — ${inputs(m).length} in, ${outputs(m).length} out, ${m.parameters.length} parameters`;

    const info = el('span', 'library-info');
    info.append(el('strong', '', m.name), el('small', '', `${inputs(m).length} in · ${outputs(m).length} out · ${m.parameters.length} params`));
    b.append(info, el('span', 'library-plus', '+'));

    b.addEventListener('click', () => this.hooks.add(m.id));
    b.addEventListener('dragstart', e => {
      e.dataTransfer?.setData('application/x-wirescape-module', m.id);
      e.dataTransfer?.setData('text/plain', m.id);
      if (e.dataTransfer) e.dataTransfer.effectAllowed = 'copy';
    });
    return b;
  }
}
