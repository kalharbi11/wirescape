// SPDX-License-Identifier: GPL-3.0-only
/**
 * MIDI Learn bar (#287). Arming, the observed CC and the 30 s engine timeout are
 * transient engine state (ENGINE_PROTOCOL §4/§7); nothing here is saved. The bar
 * only reads `status.midi.learn`; committing the mapping is the host's job.
 */
import type { EngineClient, SystemStatus } from '../engine/EngineClient';
import { el } from './dom';
import type { LearnCapture, LearnTarget } from './ccLearn';

export interface MidiLearnHost {
  engine: EngineClient;
  label(target: LearnTarget): string;
  deviceName(deviceId: string): string;
  commit(target: LearnTarget, capture: LearnCapture, anyChannel: boolean): void;
  toast(message: string, tone?: 'info' | 'error'): void;
}

interface Observed { seq: string; deviceId: string; channel: number; controller: number }

export class MidiLearn {
  readonly root: HTMLElement;
  private title: HTMLElement;
  private message: HTMLElement;
  private any: HTMLInputElement;
  private confirmBtn: HTMLButtonElement;
  private target: LearnTarget | null = null;
  private seen = false;
  private observed: Observed | null = null;
  private off: () => void;

  constructor(private readonly host: MidiLearnHost) {
    this.root = el('div', 'learn-bar');
    this.root.hidden = true;
    this.root.dataset.testid = 'learn-bar';
    this.root.setAttribute('role', 'status');
    this.title = el('span', 'learn-title');
    this.title.dataset.testid = 'learn-title';
    this.message = el('span', 'learn-message');
    this.message.dataset.testid = 'learn-message';
    const anyLabel = el('label', 'learn-any');
    this.any = el('input') as HTMLInputElement;
    this.any.type = 'checkbox';
    this.any.dataset.testid = 'learn-any-channel';
    anyLabel.append(this.any, ' any channel');
    this.confirmBtn = el('button', 'learn-btn learn-confirm', 'Confirm') as HTMLButtonElement;
    this.confirmBtn.type = 'button';
    this.confirmBtn.dataset.testid = 'learn-confirm';
    this.confirmBtn.addEventListener('click', () => this.confirm());
    const cancel = el('button', 'learn-btn', 'Cancel') as HTMLButtonElement;
    cancel.type = 'button';
    cancel.dataset.testid = 'learn-cancel';
    cancel.addEventListener('click', () => this.cancel());
    this.root.append(this.title, this.message, anyLabel, this.confirmBtn, cancel);
    this.off = host.engine.onStatus(s => this.onStatus(s));
  }

  /** The engine advertises `midiLearn` only when it can observe CCs. */
  available(): boolean { return typeof this.host.engine.midiLearn === 'function'; }
  get active(): boolean { return this.target !== null; }

  begin(module: number, param: string): void {
    if (!this.available()) return;
    this.target = { module, param };
    this.seen = false;
    this.observed = null;
    this.title.textContent = `MIDI Learn · ${this.host.label(this.target)}`;
    this.paint();
    this.root.hidden = false;
    window.addEventListener('keydown', this.onKey, true);
    this.host.engine.midiLearn!(true);
  }

  cancel(): void { this.close(); }

  confirm(): void {
    const target = this.target, o = this.observed;
    if (!target || !o) return;
    const anyChannel = this.any.checked;
    this.close();
    this.host.commit(target, { deviceId: o.deviceId, channel: o.channel, controller: o.controller }, anyChannel);
  }

  destroy(): void { this.close(); this.off(); }

  private close(): void {
    if (!this.target) return;
    this.target = null;
    this.observed = null;
    this.root.hidden = true;
    window.removeEventListener('keydown', this.onKey, true);
    this.host.engine.midiLearn?.(false);
  }

  private onKey = (e: KeyboardEvent) => {
    if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); this.cancel(); }
    else if (e.key === 'Enter' && this.observed && !(e.target instanceof HTMLButtonElement)) {
      e.preventDefault(); e.stopPropagation(); this.confirm();
    }
  };

  private onStatus(s: SystemStatus): void {
    if (!this.target) return;
    const l = s.midi.learn;
    if (!l?.armed) {
      // The engine dropped the arm (30 s without a CC, or a restart) after confirming it.
      if (this.seen) { this.close(); this.host.toast('MIDI Learn stopped: no controller moved.'); }
      return;
    }
    this.seen = true;
    this.observed = l.seq !== undefined && l.deviceId !== undefined && l.channel !== undefined
      && l.controller !== undefined ? { seq: l.seq, deviceId: l.deviceId, channel: l.channel, controller: l.controller } : null;
    this.paint();
  }

  private paint(): void {
    const o = this.observed;
    this.confirmBtn.disabled = !o;
    this.message.textContent = o
      ? `CC ${o.controller} · ch ${o.channel} · ${this.host.deviceName(o.deviceId)}`
      : 'Move a control on your controller…';
  }
}
