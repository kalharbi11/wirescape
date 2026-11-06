// SPDX-License-Identifier: GPL-3.0-only
import { el } from '../app/dom';

export class Toasts {
  readonly root: HTMLElement;
  private timer = 0;

  constructor() {
    this.root = el('div', 'toast');
    this.root.setAttribute('role', 'status');
    this.root.setAttribute('aria-live', 'polite');
    this.root.dataset.testid = 'toast';
  }

  show(message: string, tone: 'info' | 'error' = 'info'): void {
    this.root.textContent = message;
    this.root.classList.toggle('error', tone === 'error');
    this.root.classList.add('show');
    clearTimeout(this.timer);
    this.timer = window.setTimeout(() => this.root.classList.remove('show'), tone === 'error' ? 3200 : 1900);
  }
}
