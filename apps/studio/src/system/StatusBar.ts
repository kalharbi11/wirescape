// SPDX-License-Identifier: GPL-3.0-only
import type { SystemStatus } from '../engine/EngineClient';
import { el } from '../app/dom';

export interface StatusBarHooks {
  onPanic(): void;
  onOpenSettings(): void;
  onClockToggle(running: boolean): void;
}

const AUDIO_TEXT: Record<SystemStatus['audio']['state'], string> = {
  'starting': 'STARTING',
  'running': 'RUNNING',
  'no-device': 'NO DEVICE',
  'device-lost': 'DEVICE LOST',
  'reconnecting': 'RECONNECTING',
};

export class StatusBar {
  readonly root: HTMLElement;
  private audio: HTMLElement;
  private engineMode: HTMLElement;
  private audioDetail: HTMLElement;
  private cpu: HTMLElement;
  private drop: HTMLElement;
  private midi: HTMLElement;
  private osc: HTMLElement;
  private clock: HTMLElement;
  private clockBtn: HTMLButtonElement;
  private last: SystemStatus | null = null;

  constructor(private readonly hooks: StatusBarHooks) {
    this.root = el('footer', 'statusbar');
    this.root.dataset.testid = 'statusbar';

    const left = el('div', 'status-left');
    this.engineMode = el('span', 'status-chip', 'engine: simulated');
    this.engineMode.dataset.testid = 'status-engine';
    this.audio = el('span', 'status-chip');
    this.audio.dataset.testid = 'status-audio';
    this.audioDetail = el('span', 'status-detail');
    this.audioDetail.dataset.testid = 'status-audio-detail';
    this.cpu = el('span', 'status-detail');
    this.cpu.dataset.testid = 'status-cpu';
    this.drop = el('span', 'status-detail');
    this.drop.dataset.testid = 'status-dropouts';
    left.append(this.engineMode, this.audio, this.audioDetail, this.cpu, this.drop);

    const right = el('div', 'status-right');
    this.osc = el('span', 'status-detail status-osc');
    this.osc.dataset.testid = 'status-osc';
    this.osc.style.display = 'none';
    this.midi = el('span', 'status-midi');
    this.midi.dataset.testid = 'status-midi';
    this.clock = el('span', 'status-detail');
    this.clock.dataset.testid = 'status-clock';
    this.clockBtn = el('button', 'status-btn', 'STOP') as HTMLButtonElement;
    this.clockBtn.type = 'button';
    this.clockBtn.dataset.testid = 'clock-toggle';
    this.clockBtn.style.display = 'none';
    this.clockBtn.addEventListener('click', () => {
      this.hooks.onClockToggle(!(this.last?.clock?.running ?? false));
    });
    const panic = el('button', 'status-btn danger', 'PANIC') as HTMLButtonElement;
    panic.type = 'button';
    panic.title = 'Stop all notes';
    panic.dataset.testid = 'panic';
    panic.addEventListener('click', () => hooks.onPanic());
    const settings = el('button', 'status-btn', 'SETTINGS') as HTMLButtonElement;
    settings.type = 'button';
    settings.dataset.testid = 'open-settings';
    settings.addEventListener('click', () => hooks.onOpenSettings());
    right.append(this.clock, this.clockBtn, this.osc, this.midi, panic, settings);

    this.root.append(left, right);
  }

  setEngineState(state: 'real' | 'simulated' | 'restarting'): void {
    this.engineMode.textContent = 'engine: ' + state;
    this.engineMode.dataset.state = state;
    if (state === 'restarting') {
      // Stale callback metrics must not survive a sidecar exit.
      this.last = null;
      this.audio.textContent = 'DISCONNECTED';
      this.audio.dataset.state = 'no-device';
      this.audioDetail.textContent = 'no output';
      this.cpu.textContent = 'CPU not available';
      this.drop.textContent = 'dropouts not available';
      this.midi.replaceChildren();
      this.clock.textContent = '';
      this.clockBtn.style.display = 'none';
      this.osc.style.display = 'none';
    }
  }

  update(s: SystemStatus): void {
    this.last = s;
    const a = s.audio;
    this.audio.textContent = AUDIO_TEXT[a.state];
    this.audio.dataset.state = a.state;
    this.audioDetail.textContent = a.state === 'running' || a.state === 'reconnecting'
      ? `${a.deviceName} · ${(a.sampleRate / 1000).toFixed(1)} kHz · ${a.bufferFrames} frames · ${a.state === 'running' ? a.latencyMs + ' ms est.' : 'latency not available'}`
      : 'no output';
    const cpuAvailable = a.state === 'running' && Number.isFinite(a.cpuPercent) && a.cpuPercent >= 0;
    this.cpu.textContent = cpuAvailable ? `CPU ${a.cpuPercent.toFixed(0)}%` : 'CPU not available';
    this.cpu.classList.toggle('warn', cpuAvailable && a.cpuPercent > 80);
    this.drop.textContent = `${a.dropouts} dropouts`;
    this.drop.classList.toggle('warn', a.dropouts > 0);

    this.midi.replaceChildren();
    for (const d of s.midi.devices) {
      const chip = el('span', `midi-chip${d.enabled ? '' : ' off'}`);
      chip.dataset.testid = `midi-${d.id}`;
      const dot = el('i', 'midi-dot');
      dot.style.opacity = String(s.midi.activity[d.id] ?? 0);
      chip.append(dot, document.createTextNode(d.name));
      chip.title = `${d.name} — ${d.enabled ? 'enabled' : 'disabled'}`;
      this.midi.append(chip);
    }

    if (s.osc?.enabled) {
      this.osc.style.display = '';
      const last = s.osc.recent[0];
      this.osc.textContent = `OSC ${s.osc.port} · ${s.osc.allowNetwork ? 'NETWORK' : 'LOCAL'}`;
      this.osc.title = last ? `Last: ${last.address} ${last.args.join(' ')}` : 'OSC on, no messages yet';
      this.osc.classList.toggle('warn', s.osc.allowNetwork);
    } else {
      this.osc.style.display = 'none';
    }

    if (s.clock) {
      this.clock.textContent = `${Math.round(s.clock.bpm)} BPM`;
      this.clockBtn.style.display = '';
      this.clockBtn.textContent = s.clock.running ? 'STOP' : 'RUN';
    } else {
      this.clock.textContent = '';
      this.clockBtn.style.display = 'none';
    }
  }
}
