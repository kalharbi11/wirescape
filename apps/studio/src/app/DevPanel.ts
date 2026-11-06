import type { FakeEngine } from '../engine/FakeEngine';
import type { Ticker } from '../runtime/Ticker';
import { el } from './dom';

/** Dev-only panel: FPS plus the fake engine's simulation switches. */
export class DevPanel {
  readonly root: HTMLElement;
  private fps: HTMLElement;
  private frame: HTMLElement;
  private raf = 0;
  private open = false;
  /** Read by the frame watchdog in App: pretend the device is slow. */
  simulateSlow = false;

  constructor(private readonly engine: FakeEngine, private readonly ticker: Ticker) {
    this.root = el('div', 'dev-panel');
    this.root.dataset.testid = 'dev-panel';
    this.root.style.display = 'none';

    const head = el('div', 'dev-head');
    head.append(el('span', '', 'DEBUG'));
    const close = el('button', 'dev-close', '×') as HTMLButtonElement;
    close.type = 'button';
    close.addEventListener('click', () => this.toggle(false));
    head.append(close);

    this.fps = el('div', 'dev-stat');
    this.fps.dataset.testid = 'dev-fps';
    this.frame = el('div', 'dev-stat');
    this.frame.dataset.testid = 'dev-frame';

    const switches = el('div', 'dev-switches');
    const add = (key: keyof FakeEngine['debug'], label: string) => {
      const b = el('button', 'dev-switch', label) as HTMLButtonElement;
      b.type = 'button';
      b.dataset.testid = `debug-${String(key)}`;
      b.addEventListener('click', () => {
        this.engine.debug[key] = !this.engine.debug[key];
        b.classList.toggle('on', this.engine.debug[key]);
      });
      switches.append(b);
    };
    add('unplugAudio', 'Unplug audio device');
    add('extraMidiDevice', 'Plug in a MIDI device');
    add('cpuSpike', 'CPU spike');
    add('dropouts', 'Dropouts');
    add('refuseEdits', 'Refuse edits');
    add('oscTraffic', 'OSC traffic (while OSC is on)');
    const slow = el('button', 'dev-switch', 'Simulate slow frames') as HTMLButtonElement;
    slow.type = 'button';
    slow.dataset.testid = 'debug-slowFrames';
    slow.addEventListener('click', () => {
      this.simulateSlow = !this.simulateSlow;
      slow.classList.toggle('on', this.simulateSlow);
    });
    switches.append(slow);

    this.root.append(head, this.fps, this.frame, switches);
  }

  toggle(next = !this.open): void {
    this.open = next;
    this.root.style.display = next ? '' : 'none';
    if (next) this.loop(); else cancelAnimationFrame(this.raf);
  }

  private loop = (): void => {
    this.fps.textContent = `${this.ticker.fps} fps`;
    this.frame.textContent = `${this.ticker.applyMs} ms / frame applying telemetry`;
    this.raf = requestAnimationFrame(this.loop);
  };
}
