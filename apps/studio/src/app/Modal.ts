// SPDX-License-Identifier: GPL-3.0-only
import { el } from './dom';

/** A minimal centred dialog. Escape and the backdrop both close it. */
export class Modal {
  readonly root: HTMLElement;
  readonly body: HTMLElement;
  private titleEl: HTMLElement;
  private returnFocus: HTMLElement | null = null;
  private onClose: (() => void) | null = null;

  constructor(testid: string) {
    this.root = el('div', 'modal-backdrop');
    this.root.dataset.testid = testid;
    this.root.style.display = 'none';
    const panel = el('div', 'modal');
    panel.setAttribute('role', 'dialog');
    panel.setAttribute('aria-modal', 'true');
    const head = el('div', 'modal-head');
    this.titleEl = el('h2', 'modal-title', '');
    const close = el('button', 'modal-close', '×') as HTMLButtonElement;
    close.type = 'button';
    close.setAttribute('aria-label', 'Close');
    close.dataset.testid = `${testid}-close`;
    close.addEventListener('click', () => this.close());
    head.append(this.titleEl, close);
    this.body = el('div', 'modal-body');
    panel.append(head, this.body);
    this.root.append(panel);
    this.root.addEventListener('pointerdown', e => { if (e.target === this.root) this.close(); });
    document.addEventListener('keydown', e => {
      if (!this.isOpen) return;
      if (e.key === 'Escape') { e.stopPropagation(); this.close(); }
      if (e.key === 'Tab') {
        const items = [...this.root.querySelectorAll<HTMLElement>('button,input,select,textarea,[tabindex]')]
          .filter(el => el.tabIndex >= 0 && !el.matches(':disabled') && el.getClientRects().length);
        const first = items[0], last = items[items.length - 1];
        if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last?.focus(); }
        else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first?.focus(); }
      }
    });
  }

  get isOpen(): boolean { return this.root.style.display !== 'none' }

  open(title: string, onClose?: () => void): void {
    if (this.isOpen) this.close();
    this.returnFocus = document.activeElement instanceof HTMLElement ? document.activeElement : null;
    this.titleEl.textContent = title;
    this.onClose = onClose ?? null;
    this.root.style.display = '';
    this.root.querySelector<HTMLElement>('button,input,select')?.focus();
  }

  close(): void {
    if (!this.isOpen) return;
    this.root.style.display = 'none';
    if (this.returnFocus?.isConnected) this.returnFocus.focus();
    this.returnFocus = null;
    const cb = this.onClose;
    this.onClose = null;
    cb?.();
  }
}

export function field(label: string, control: HTMLElement, hint?: string): HTMLElement {
  const row = el('div', 'field');
  const left = el('div', 'field-label');
  left.append(el('span', '', label));
  if (hint) left.append(el('small', '', hint));
  row.append(left, control);
  return row;
}

export function select(options: { value: string; label: string }[], value: string, onChange: (v: string) => void): HTMLSelectElement {
  const s = el('select', 'input-select') as HTMLSelectElement;
  for (const o of options) {
    const op = document.createElement('option');
    op.value = o.value; op.textContent = o.label;
    s.append(op);
  }
  s.value = value;
  s.addEventListener('change', () => onChange(s.value));
  return s;
}

export function switchButton(on: boolean, onChange: (v: boolean) => void): HTMLButtonElement {
  const b = el('button', `toggle${on ? ' on' : ''}`) as HTMLButtonElement;
  b.type = 'button';
  // Same LED latch as the panel toggles (DESIGN §4.2, OPEN-1 default).
  const state = el('span', 'toggle-state', on ? 'ON' : 'OFF');
  b.append(el('i', 'led'), state);
  b.setAttribute('aria-pressed', String(on));
  b.addEventListener('click', () => {
    const next = !b.classList.contains('on');
    b.classList.toggle('on', next);
    b.setAttribute('aria-pressed', String(next));
    state.textContent = next ? 'ON' : 'OFF';
    onChange(next);
  });
  return b;
}
