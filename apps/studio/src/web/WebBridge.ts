// SPDX-License-Identifier: GPL-3.0-only
/**
 * Web demo bridge: the same five-member surface Electron's preload exposes,
 * backed by the real engine compiled to WebAssembly inside an AudioWorklet.
 * RealEngine speaks protocol v0 to it unchanged. No MIDI, OSC, devices or files.
 */
import type { WirescapeBridge } from '../engine/RealEngine';

type Message = Record<string, unknown>;
type Reply = { ok: boolean; gen: string; code?: string; message?: string };

const NO_DEVICES = new Set(['listAudioDevices', 'listMidiDevices']);
const IGNORED = new Set(['selectAudioDevice', 'setMidiEnabled', 'setSampleRate', 'setBufferFrames', 'setOsc']);

export class WebBridge implements WirescapeBridge {
  readonly platform = 'linux' as const;
  readonly web = true;
  private listeners = new Set<(m: Message) => void>();
  private node?: AudioWorkletNode;
  private context?: AudioContext;
  private session = 'web-' + Math.random().toString(36).slice(2, 10);
  private gen = '0';
  private hello?: Message;
  private started = Date.now();
  private tick = 0;
  /** Last output peak (0..1+), for tests and the splash meter. */
  peak = 0;

  /** Must run from a user gesture: browsers only start audio after a click or key. */
  async start(base: string, options: { readyTimeoutMs?: number; processorUrl?: string } = {}): Promise<void> {
    const context = new AudioContext({ latencyHint: 'interactive' });
    // iOS Safari needs resume() invoked during the original tap, not after
    // asynchronous WASM compilation or worklet installation has consumed it.
    // Keep the promise pending until the processor is connected below.
    const resumed = context.resume();
    void resumed.catch(() => undefined);
    let node: AudioWorkletNode | undefined;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let catalogue: unknown;
    // One deadline covers the whole attempt: fetch, worklet load, resume and the ready handshake.
    const deadline = new Promise<never>((_, reject) => {
      timer = setTimeout(() => reject(new Error('the engine did not answer in time')), options.readyTimeoutMs ?? 10_000);
    });
    const within = <T>(p: Promise<T>): Promise<T> => Promise.race([p, deadline]);
    try {
      const [module] = await within(Promise.all([
        WebAssembly.compileStreaming(fetch(base + 'web/wirescape.wasm')),
        context.audioWorklet.addModule(options.processorUrl ?? base + 'web/ws-processor.js'),
      ]));
      const created = new AudioWorkletNode(context, 'wirescape-engine', {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { module },
      });
      node = created;
      const ready = new Promise<Message>((resolve, reject) => {
        created.onprocessorerror = () => reject(new Error('the audio engine stopped while starting'));
        created.port.onmessage = e => {
          const m = e.data as Message;
          if (m.kind === 'ready') resolve(m);
          else if (m.kind === 'failed') reject(new Error(String(m.message)));
        };
      });
      created.connect(context.destination);
      await within(resumed);
      ({ catalogue } = await within(ready));
    } catch (error) {
      // A failed attempt leaves nothing running, so "Try again" starts clean.
      if (node) { node.port.onmessage = null; node.onprocessorerror = null; node.disconnect(); }
      await context.close().catch(() => undefined);
      throw error;
    } finally {
      clearTimeout(timer);
    }
    this.context = context;
    this.node = node;
    node.onprocessorerror = () =>
      this.emit({ v: 0, type: 'app.engineExited', session: this.session, message: 'Audio engine stopped.' });
    node.port.onmessage = e => this.receive(e.data as Message);
    const latencyMs = Math.round(((context.baseLatency || 0) + (context.outputLatency || 0)) * 10000) / 10;
    this.hello = {
      v: 0, type: 'hello', session: this.session, protocol: 0,
      engine: { version: 'web', sha: 'wasm' }, capabilities: ['telemetry.playhead'], catalogue,
      audio: {
        state: 'running', deviceId: 'browser', deviceName: 'Browser audio', sampleRate: context.sampleRate,
        bufferFrames: 128, latencyMs, cpuPercent: 0, dropouts: 0,
      },
      midi: { devices: [], activity: {} },
    };
    this.emit(this.hello);
  }

  send(obj: Message): void {
    const id = String(obj.id ?? '');
    const type = String(obj.type ?? '');
    if (NO_DEVICES.has(type)) return this.reply(id, { ok: true, gen: this.gen }, { result: { devices: [] } });
    if (IGNORED.has(type)) return this.reply(id, { ok: true, gen: this.gen });
    if (!this.node) {
      return this.reply(id, { ok: false, gen: this.gen, code: 'unavailable', message: 'Audio has not started.' });
    }
    const { v: _v, id: _id, ...command } = obj;
    this.node.port.postMessage({ kind: 'command', id, command });
  }

  onMessage(cb: (m: Message) => void): () => void {
    this.listeners.add(cb);
    // RealEngine may subscribe after audio started; it still needs the handshake.
    if (this.hello) queueMicrotask(() => { if (this.listeners.has(cb)) cb(this.hello!); });
    return () => this.listeners.delete(cb);
  }

  /** Browser file picker; nothing leaves the machine. Resolves null when cancelled. */
  openPatch(request?: { library: 'list' | 'read' | 'reset'; name?: string }): Promise<{ text: string; name: string } | null> {
    if (request) return Promise.resolve(request.library === 'list' ? { name: 'User patches', text: '[]' } : null);
    return new Promise(resolve => {
      const input = document.createElement('input');
      input.type = 'file';
      input.accept = '.json,application/json';
      input.addEventListener('change', () => {
        const file = input.files?.[0];
        if (!file || file.size > 2 * 1024 * 1024) { resolve(null); return; }
        void file.text().then(text => resolve({ text, name: file.name.replace(/\.(wirescape\.)?json$/i, '') }),
          () => resolve(null));
      });
      input.addEventListener('cancel', () => resolve(null));
      input.click();
    });
  }
  /** Downloads the patch as a file. Returns null so the app keeps its name and dirty state. */
  async savePatch(text: string): Promise<null> {
    const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = 'wirescape-patch.json';
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 10_000);
    return null;
  }

  private emit(m: Message): void { for (const cb of [...this.listeners]) cb(m); }
  private reply(id: string, r: Reply, extra: Message = {}): void {
    if (/^\d+$/.test(r.gen)) this.gen = r.gen;
    const body: Message = { v: 0, session: this.session, re: id, ok: r.ok, gen: this.gen, ...extra };
    if (!r.ok) { body.code = r.code ?? 'refused'; body.message = r.message ?? 'Refused by the engine.'; }
    queueMicrotask(() => this.emit(body));
  }
  private receive(m: Message): void {
    if (m.kind === 'reply') this.reply(String(m.id), m.reply as Reply);
    else if (m.kind === 'telemetry') {
      // Use the worklet's graph generation, never relabel an older snapshot
      // with the latest command reply's generation.
      if (typeof m.gen !== 'string' || !/^\d+$/.test(m.gen) || BigInt(m.gen) < BigInt(this.gen)) return;
      this.peak = Number(m.peak) || 0;
      const t = (m.telemetry ?? {}) as Message;
      this.emit({
        v: 0, type: 'telemetry', session: this.session, gen: m.gen,
        telemetry: {
          tick: ++this.tick, time: (Date.now() - this.started) / 1000,
          effective: t.effective ?? {}, phase: t.phase ?? {}, envelope: t.envelope ?? {},
          scope: t.scope ?? {}, eventActivity: t.eventActivity ?? {}, meters: t.meters ?? {},
          heldNotes: t.heldNotes ?? [],
          outputs: t.outputs ?? {},
          playhead: t.playhead ?? {}, groove: t.groove ?? {},
        },
      });
    } else if (m.kind === 'failed') {
      this.emit({ v: 0, type: 'app.engineExited', session: this.session, message: m.message });
    }
  }

  async suspend(): Promise<void> { await this.context?.suspend(); }
}
