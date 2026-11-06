import type {
  EngineClient, OscStatus, Telemetry, SystemStatus, Device, EngineError,
} from './EngineClient';
import { EngineRefusal } from './EngineClient';
import type { PatchV2, Connection } from '../patch/types';
import type { ModuleRegistry } from '../modules/registry';
import { findParam, findPort, normToValue, valueToNorm } from '../modules/registry';

const TICK_HZ = 30;

interface Node { id: number; type: string; params: Record<string, number> }
interface Voice { note: number; velocity: number; on: boolean; since: number; level: number; stage: 'a' | 'd' | 's' | 'r' | 'off' }

export interface DebugSwitches {
  unplugAudio: boolean;
  extraMidiDevice: boolean;
  cpuSpike: boolean;
  dropouts: boolean;
  refuseEdits: boolean;
  /** Fake a stream of incoming OSC messages while OSC is on. */
  oscTraffic: boolean;
}

/**
 * The fake engine.
 *
 * It keeps its own copy of the graph, runs a 30 Hz clock and computes
 * believable numbers for everything the UI draws. It makes no sound, and it
 * is the only place in the app where a value is invented.
 */
export class FakeEngine implements EngineClient {
  private nodes = new Map<number, Node>();
  private conns: Connection[] = [];
  private voices: Voice[] = [];
  private telemetryCbs = new Set<(t: Telemetry) => void>();
  private statusCbs = new Set<(s: SystemStatus) => void>();
  private timer: number | undefined;
  private tick = 0;
  private t0 = performance.now();
  private time = 0;
  private lastEffective = new Map<string, number>();
  private phases = new Map<number, number>();
  private randomHold = new Map<number, { value: number; target: number; last: number }>();
  private clockEdge = new Map<number, number>();
  private seqStep = new Map<number, number>();
  private meterPeak = new Map<number, [number, number]>();
  private scopePhase = 0;
  private eventActivity = new Map<string, number>();

  debug: DebugSwitches = {
    unplugAudio: false, extraMidiDevice: false, cpuSpike: false,
    dropouts: false, refuseEdits: false, oscTraffic: true,
  };
  private osc: OscStatus = { enabled: false, port: 9000, allowNetwork: false, recent: [] };

  private status: SystemStatus = {
    audio: {
      state: 'starting', deviceId: 'hw:0', deviceName: 'Built-in Audio',
      sampleRate: 48000, bufferFrames: 256, latencyMs: 0, cpuPercent: 4, dropouts: 0,
    },
    midi: { devices: [], activity: {} },
  };

  private readonly audioDevices: Device[] = [
    { id: 'hw:0', name: 'Built-in Audio', kind: 'audio', sampleRates: [44100, 48000] },
    { id: 'usb:1', name: 'Scarlett 2i2 USB', kind: 'audio', sampleRates: [44100, 48000, 96000] },
  ];
  private midiDevices: Device[] = [
    { id: 'midi:0', name: 'Arturia KeyStep', kind: 'midi', enabled: true },
    { id: 'midi:1', name: 'Launchpad Mini', kind: 'midi', enabled: true },
  ];

  constructor(private readonly registry: ModuleRegistry) {
    this.status.midi.devices = this.midiDevices.map(d => ({ ...d }));
    this.recomputeLatency();
  }

  // ---- lifecycle --------------------------------------------------------

  start(): void {
    if (this.timer !== undefined) return;
    this.t0 = performance.now();
    this.timer = window.setInterval(() => this.step(), 1000 / TICK_HZ);
    // The real engine auto-starts audio at launch; imitate the short delay.
    window.setTimeout(() => {
      this.status.audio.state = this.debug.unplugAudio ? 'no-device' : 'running';
      this.pushStatus();
    }, 420);
    this.pushStatus();
  }

  stop(): void {
    if (this.timer !== undefined) { clearInterval(this.timer); this.timer = undefined; }
  }

  // ---- EngineClient: the patch -----------------------------------------

  async loadGraph(patch: PatchV2): Promise<void> {
    this.nodes.clear(); this.conns = []; this.voices = [];
    this.phases.clear(); this.randomHold.clear(); this.seqStep.clear();
    this.lastEffective.clear(); this.meterPeak.clear();
    for (const m of patch.modules) {
      this.nodes.set(m.id, { id: m.id, type: m.module, params: { ...(m.parameters ?? {}) } });
    }
    for (const c of patch.connections) {
      try { this.validateConnection(c); this.conns.push({ ...c }); }
      catch { /* a patch may legitimately carry cables this engine won't model */ }
    }
    this.pushStatus();
  }

  addModule(id: number, type: string, params: Record<string, number> = {}): void {
    this.guardRefusal();
    if (!this.registry.has(type)) {
      throw new EngineRefusal({ code: 'unknown-module', message: `Unknown module type "${type}"` });
    }
    this.nodes.set(id, { id, type, params: { ...params } });
    this.pushStatus();
  }

  removeModule(id: number): void {
    this.nodes.delete(id);
    this.conns = this.conns.filter(c => c.from.module !== id && c.to.module !== id);
    this.pushStatus();
  }

  connect(c: Connection): void {
    this.guardRefusal();
    this.validateConnection(c);
    this.conns.push({ ...c });
  }

  disconnect(c: Connection): void {
    const k = key(c);
    this.conns = this.conns.filter(x => key(x) !== k);
  }

  setParam(moduleId: number, paramId: string, value: number): void {
    this.guardRefusal();
    const n = this.nodes.get(moduleId);
    if (!n) throw new EngineRefusal({ code: 'not-found', message: `No module ${moduleId}` });
    n.params[paramId] = value;
  }

  setModAmount(c: Connection, amount: number): void {
    const k = key(c);
    const found = this.conns.find(x => key(x) === k);
    if (found) found.amount = amount;
  }

  private validateConnection(c: Connection): void {
    if (c.kind === 'modulation') {
      const dst = this.nodes.get(c.to.module);
      const d = dst && this.registry.get(dst.type);
      const p = d && findParam(d, c.to.port);
      if (d && !p) throw new EngineRefusal({ code: 'not-found', message: `No parameter "${c.to.port}"` });
      if (p && !p.modulatable) {
        throw new EngineRefusal({ code: 'refused', message: `${p.name} cannot be modulated` });
      }
      return;
    }
    const src = this.nodes.get(c.from.module), dst = this.nodes.get(c.to.module);
    const sd = src && this.registry.get(src.type), dd = dst && this.registry.get(dst.type);
    const sp = sd && findPort(sd, 'output', c.from.port);
    const dp = dd && findPort(dd, 'input', c.to.port);
    // Unknown modules carry no descriptor; let their cables through untouched.
    if (!sp || !dp) return;
    if (sp.kind !== dp.kind) {
      throw new EngineRefusal({
        code: 'kind-mismatch',
        message: sp.kind === 'event'
          ? 'An event output cannot drive a signal input.'
          : 'A signal output cannot drive an event input.',
      });
    }
  }

  private guardRefusal(): void {
    if (this.debug.refuseEdits) {
      throw new EngineRefusal({ code: 'refused', message: 'Engine refused the edit (debug switch)' });
    }
  }

  // ---- EngineClient: playing -------------------------------------------

  noteOn(note: number, velocity: number): void {
    const existing = this.voices.find(v => v.note === note);
    if (existing) { existing.on = true; existing.since = this.time; existing.stage = 'a'; existing.velocity = velocity; return; }
    this.voices.push({ note, velocity, on: true, since: this.time, level: 0, stage: 'a' });
  }

  noteOff(note: number): void {
    const v = this.voices.find(v => v.note === note && v.on);
    if (v) { v.on = false; v.since = this.time; v.stage = 'r'; }
  }

  panic(): void { this.voices = []; }

  // ---- EngineClient: subscriptions -------------------------------------

  onTelemetry(cb: (t: Telemetry) => void): () => void {
    this.telemetryCbs.add(cb); return () => this.telemetryCbs.delete(cb);
  }
  onStatus(cb: (s: SystemStatus) => void): () => void {
    this.statusCbs.add(cb); cb(this.snapshotStatus()); return () => this.statusCbs.delete(cb);
  }

  // ---- EngineClient: devices -------------------------------------------

  async listAudioDevices(): Promise<Device[]> { return this.audioDevices.map(d => ({ ...d })); }
  selectAudioDevice(id: string): void {
    const d = this.audioDevices.find(d => d.id === id);
    if (!d) return;
    this.status.audio.deviceId = d.id;
    this.status.audio.deviceName = d.name;
    this.status.audio.state = 'running';
    this.recomputeLatency();
    this.pushStatus();
  }
  async listMidiDevices(): Promise<Device[]> { return this.midiDevices.map(d => ({ ...d })); }
  setMidiEnabled(id: string, on: boolean): void {
    const d = this.midiDevices.find(d => d.id === id);
    if (d) { d.enabled = on; this.pushStatus(); }
  }

  setBufferFrames(frames: number): void { this.setBuffer(frames); }

  setSampleRate(hz: number): void {
    const d = this.audioDevices.find(d => d.id === this.status.audio.deviceId);
    if (d?.sampleRates && !d.sampleRates.includes(hz)) return;
    this.status.audio.sampleRate = hz;
    this.recomputeLatency();
    this.pushStatus();
  }

  setOsc(cfg: Partial<Pick<OscStatus, 'enabled' | 'port' | 'allowNetwork'>>): void {
    if (cfg.port !== undefined && (!Number.isInteger(cfg.port) || cfg.port < 1024 || cfg.port > 65535)) {
      throw new EngineRefusal({ code: 'refused', message: 'OSC port must be a whole number from 1024 to 65535.' });
    }
    Object.assign(this.osc, cfg);
    if (!this.osc.enabled) this.osc.recent = [];
    this.pushStatus();
  }

  setBuffer(frames: number): void {
    this.status.audio.bufferFrames = frames;
    this.recomputeLatency();
    this.pushStatus();
  }

  private recomputeLatency(): void {
    const a = this.status.audio;
    a.latencyMs = Math.round((a.bufferFrames / a.sampleRate) * 2 * 1000 * 10) / 10;
  }

  // ---- the clock --------------------------------------------------------

  private step(): void {
    this.tick++;
    this.time = (performance.now() - this.t0) / 1000;

    this.applyDebugState();
    this.updateVoices();

    const cache = new Map<string, number>();
    const effective: Record<number, Record<string, number>> = {};
    const phase: Record<number, number> = {};
    const envelope: Record<number, number> = {};
    const meters: Record<number, [number, number]> = {};
    const scope: Record<number, Float32Array> = {};

    // Advance oscillators / clocks / random sources that keep their own state.
    for (const n of this.nodes.values()) this.advance(n);

    // Effective parameter values (base + modulation, in taper space).
    for (const n of this.nodes.values()) {
      const d = this.registry.get(n.type);
      if (!d) continue;
      const mods = this.conns.filter(c => c.kind === 'modulation' && c.to.module === n.id);
      const changed: Record<string, number> = {};
      for (const p of d.parameters) {
        const base = n.params[p.id] ?? p.default;
        let value = base;
        const my = mods.filter(c => c.to.port === p.id);
        if (my.length) {
          let norm = valueToNorm(p, base);
          for (const c of my) {
            norm += (c.amount ?? 0) * this.sourceValue(c, cache);
          }
          value = normToValue(p, Math.max(0, Math.min(1, norm)));
        }
        const k = `${n.id}:${p.id}`;
        const prev = this.lastEffective.get(k);
        if (prev === undefined || Math.abs(prev - value) > Math.abs(value) * 1e-4 + 1e-9) {
          this.lastEffective.set(k, value);
          changed[p.id] = value;
        }
      }
      if (Object.keys(changed).length) effective[n.id] = changed;

      if (n.type === 'essentials.lfo' || n.type === 'example.random_source' || n.type === 'essentials.clock') {
        phase[n.id] = this.phases.get(n.id) ?? 0;
      }
      if (n.type === 'example.step_sequencer') {
        const len = Math.max(1, Math.round(n.params.length ?? 8));
        phase[n.id] = (this.seqStep.get(n.id) ?? 0) / len;
      }
      if (n.type === 'essentials.adsr' || n.type === 'essentials.function_envelope') {
        envelope[n.id] = this.envelopeFor(n);
      }
      if (n.type === 'essentials.mixer') {
        // Channel meters are post-gain input levels, not a fabricated stereo pair.
        const channel = (port: string, param: string) => this.conns
          .filter(c => c.kind === 'signal' && c.to.module === n.id && c.to.port === port)
          .reduce((sum, c) => sum + this.audioLevel(c.from.module, cache), 0)
          * (this.effectiveParam(n, param) ?? 0.5);
        meters[n.id] = [channel('a', 'level_a'), channel('b', 'level_b')];
      }
      if (n.type === 'essentials.output') {
        const lvl = this.audioLevel(n.id, cache);
        meters[n.id] = this.meterWithPeak(n.id, lvl);
        scope[n.id] = this.synthScope(lvl);
      }
    }

    // Event-port activity decays towards zero.
    const eventActivity: Record<string, number> = {};
    for (const [k, v] of this.eventActivity) {
      const next = v * 0.78;
      if (next < 0.01) this.eventActivity.delete(k); else this.eventActivity.set(k, next);
      eventActivity[k] = next;
    }
    for (const n of this.nodes.values()) {
      const d = this.registry.get(n.type);
      if (!d) continue;
      for (const p of d.ports) {
        if (p.kind !== 'event' || p.direction !== 'output') continue;
        if (this.voices.some(v => v.on)) {
          const k = `${n.id}:${p.id}`;
          this.eventActivity.set(k, 1);
          eventActivity[k] = 1;
        }
      }
    }

    // Current value of every connected signal output (optional telemetry field).
    const outputs: Record<string, number> = {};
    for (const c of this.conns) {
      if (c.kind === 'event') continue;
      const k = `${c.from.module}:${c.from.port}`;
      if (k in outputs) continue;
      // Audio outputs report their level; control outputs their value.
      const d = this.registry.get(this.nodes.get(c.from.module)?.type ?? '');
      const hint = d?.ports.find(p => p.direction === 'output' && p.id === c.from.port)?.hint;
      outputs[k] = hint === 'audio' ? this.audioLevel(c.from.module, cache) : this.sourceValue(c, cache);
    }

    const t: Telemetry = {
      tick: this.tick, time: this.time,
      effective, phase, envelope, meters, scope, eventActivity, outputs,
      heldNotes: this.voices.filter(v => v.on).map(v => v.note),
    };
    for (const cb of this.telemetryCbs) cb(t);

    if (this.tick % 15 === 0) this.pushStatus();
  }

  // ---- per-module state advance ----------------------------------------

  private advance(n: Node): void {
    const dt = 1 / TICK_HZ;
    switch (n.type) {
      case 'essentials.lfo': {
        const sync = (n.params.sync ?? 0) >= 0.5;
        const bpm = this.graphBpm();
        const beats = n.params.beats ?? 1;
        const hz = sync ? bpm / 60 / Math.max(0.01, beats) : (n.params.rate ?? 1);
        this.phases.set(n.id, frac((this.phases.get(n.id) ?? 0) + hz * dt));
        break;
      }
      case 'essentials.clock': {
        const bpm = n.params.bpm ?? 120;
        const tpb = Math.max(1, Math.round(n.params.ticks_per_beat ?? 1));
        const running = (n.params.run ?? 1) >= 0.5;
        if (running) {
          const hz = (bpm / 60) * tpb;
          const p = frac((this.phases.get(n.id) ?? 0) + hz * dt);
          const prev = this.phases.get(n.id) ?? 0;
          if (p < prev) this.clockEdge.set(n.id, this.tick);
          this.phases.set(n.id, p);
        }
        break;
      }
      case 'example.random_source': {
        const st = this.randomHold.get(n.id) ?? { value: 0, target: 0, last: -1 };
        // Step on a clock edge if clocked, otherwise free-run a few times a second.
        const clocked = this.conns.some(c => c.kind !== 'modulation' && c.to.module === n.id && c.to.port === 'clock');
        const edge = clocked
          ? [...this.clockEdge.values()].includes(this.tick)
          : this.tick % 12 === 0;
        if (edge) st.target = (Math.random() * 2 - 1) * (n.params.range ?? 1);
        const smooth = Math.max(0.001, n.params.smooth ?? 0.05);
        st.value += (st.target - st.value) * Math.min(1, dt / smooth);
        this.randomHold.set(n.id, st);
        this.phases.set(n.id, (st.value + 1) / 2);
        break;
      }
      case 'example.step_sequencer': {
        const running = (n.params.run ?? 1) >= 0.5;
        if (!running) break;
        const len = Math.max(1, Math.round(n.params.length ?? 8));
        const clocked = this.conns.some(c => c.kind !== 'modulation' && c.to.module === n.id && c.to.port === 'clock');
        const edge = clocked
          ? [...this.clockEdge.values()].includes(this.tick)
          : this.tick % 12 === 0;
        if (edge) this.seqStep.set(n.id, ((this.seqStep.get(n.id) ?? 0) + 1) % len);
        break;
      }
      default: break;
    }
  }

  private graphBpm(): number {
    for (const n of this.nodes.values()) if (n.type === 'essentials.clock') return n.params.bpm ?? 120;
    return 120;
  }

  // ---- source values for modulation ------------------------------------

  /**
   * The value a modulation cable carries this tick, roughly -1..1 for
   * bipolar sources and 0..1 for unipolar ones. Modulation is summed in
   * *normalised* (taper-aware) knob space, so an `amount` of 0.4 always
   * means "four tenths of the knob's travel", whatever the unit.
   */
  private sourceValue(c: Connection, cache: Map<string, number>): number {
    const k = `${c.from.module}:${c.from.port}`;
    const hit = cache.get(k);
    if (hit !== undefined) return hit;
    cache.set(k, 0); // cycle guard
    const v = this.computeSource(c.from.module, c.from.port, cache);
    cache.set(k, v);
    return v;
  }

  private computeSource(moduleId: number, portId: string, cache: Map<string, number>): number {
    const n = this.nodes.get(moduleId);
    if (!n) return 0;
    switch (n.type) {
      case 'essentials.lfo': {
        const ph = this.phases.get(n.id) ?? 0;
        return Math.sin(ph * Math.PI * 2) * (n.params.depth ?? 1) + (n.params.offset ?? 0);
      }
      case 'essentials.adsr':
      case 'essentials.function_envelope':
        return this.envelopeFor(n);
      case 'essentials.clock':
        return (this.phases.get(n.id) ?? 0) < (n.params.duty ?? 0.5) ? 1 : 0;
      case 'example.random_source':
        return portId === 'smooth'
          ? (this.randomHold.get(n.id)?.value ?? 0)
          : (this.randomHold.get(n.id)?.target ?? 0);
      case 'host.keyboard':
      case 'midi.to_cv': {
        const v = this.voices.filter(v => v.on).at(-1);
        if (!v) return portId === 'gate' ? 0 : 0;
        if (portId === 'pitch') return (v.note - 60) / 60;
        if (portId === 'gate') return 1;
        if (portId === 'velocity') return v.velocity;
        return 0;
      }
      case 'example.step_sequencer': {
        const step = (this.seqStep.get(n.id) ?? 0) + 1;
        if (portId === 'pitch') return (n.params[`pitch${step}`] ?? 0) / 2;
        if (portId === 'gate') return (n.params[`gate${step}`] ?? 1) > 0 ? 1 : 0;
        if (portId === 'velocity') return n.params[`vel${step}`] ?? 0.8;
        return 0;
      }
      case 'essentials.noise':
        return (Math.random() * 2 - 1) * (n.params.level ?? 0.2);
      case 'essentials.scale_offset': {
        const up = this.conns.find(c => c.kind !== 'modulation' && c.to.module === n.id && c.to.port === 'control');
        const src = up ? this.sourceValue(up, cache) : 0;
        return src * (n.params.scale ?? 1) + (n.params.offset ?? 0);
      }
      case 'essentials.slew': {
        const up = this.conns.find(c => c.kind !== 'modulation' && c.to.module === n.id && c.to.port === 'control');
        return up ? this.sourceValue(up, cache) : 0;
      }
      case 'midi.cc_to_cv': {
        // No real MIDI: a slow deterministic wander per CC so the UI has life.
        const i = Number(portId.replace('cc', '')) || 1;
        return Math.sin(this.time * (0.13 + i * 0.037) + i) * 0.6;
      }
      default:
        return 0;
    }
  }

  // ---- envelopes --------------------------------------------------------

  private updateVoices(): void {
    const dt = 1 / TICK_HZ;
    for (const v of this.voices) {
      const { a, d, s, r } = this.envelopeTimes();
      if (v.on) {
        if (v.stage === 'a') {
          v.level += dt / Math.max(0.001, a);
          if (v.level >= 1) { v.level = 1; v.stage = 'd'; }
        } else if (v.stage === 'd') {
          v.level -= dt / Math.max(0.001, d) * (1 - s);
          if (v.level <= s) { v.level = s; v.stage = 's'; }
        } else { v.level = s; }
      } else {
        v.level -= dt / Math.max(0.001, r);
        if (v.level <= 0) { v.level = 0; v.stage = 'off'; }
      }
    }
    this.voices = this.voices.filter(v => v.on || v.level > 0.0005);
  }

  private envelopeTimes(): { a: number; d: number; s: number; r: number } {
    for (const n of this.nodes.values()) {
      if (n.type === 'essentials.adsr') {
        return {
          a: n.params.attack ?? 0.01, d: n.params.decay ?? 0.1,
          s: n.params.sustain ?? 0.7, r: n.params.release ?? 0.2,
        };
      }
      if (n.type === 'essentials.function_envelope') {
        return { a: n.params.rise ?? 0.01, d: n.params.fall ?? 0.1, s: 1, r: n.params.fall ?? 0.1 };
      }
    }
    return { a: 0.01, d: 0.1, s: 0.7, r: 0.2 };
  }

  private envelopeFor(_n: Node): number {
    let max = 0;
    for (const v of this.voices) if (v.level > max) max = v.level;
    return max;
  }

  // ---- audio level estimate --------------------------------------------

  /** A rough RMS estimate propagated through the graph. Not DSP — a guess. */
  private audioLevel(moduleId: number, cache: Map<string, number>): number {
    const k = `lvl:${moduleId}`;
    const hit = cache.get(k);
    if (hit !== undefined) return hit;
    cache.set(k, 0);
    const n = this.nodes.get(moduleId);
    if (!n) return 0;
    const upstream = (port?: string) => this.conns
      .filter(c => c.kind === 'signal' && c.to.module === moduleId && (port ? c.to.port === port : true))
      .reduce((sum, c) => sum + this.audioLevel(c.from.module, cache), 0);

    const env = this.envelopeFor(n);
    let out = 0;
    switch (n.type) {
      case 'essentials.vco': out = 0.55 * (this.voices.length ? env : 0.75); break;
      case 'essentials.noise': out = n.params.level ?? 0.2; break;
      case 'essentials.chaos_lite':
        out = (n.params.level ?? 0.5) * ((n.params.level_a ?? 0.55) + (n.params.level_b ?? 0.35)) * (this.voices.length ? env : 0.6);
        break;
      case 'ports.macro_oscillator': out = 0.6 * (this.voices.length ? env : 0.7); break;
      case 'essentials.vca': {
        const gainCv = this.conns.find(c => c.kind === 'signal' && c.to.module === moduleId && c.to.port === 'gain_cv');
        const cv = gainCv ? Math.max(0, this.sourceValue(gainCv, cache)) : 1;
        out = upstream('audio') * (this.effectiveParam(n, 'gain') ?? 1) * cv;
        break;
      }
      case 'essentials.vcf': case 'essentials.ladder': {
        const cutoff = this.effectiveParam(n, 'cutoff') ?? 1000;
        out = upstream('audio') * (0.35 + 0.65 * Math.min(1, Math.log10(cutoff / 20) / 3));
        break;
      }
      case 'essentials.mixer':
        out = upstream('a') * (this.effectiveParam(n, 'level_a') ?? 0.5) + upstream('b') * (this.effectiveParam(n, 'level_b') ?? 0.5);
        break;
      case 'essentials.output': out = upstream('audio'); break;
      default: out = upstream(); break;
    }
    out = Math.max(0, out);
    cache.set(k, out);
    return out;
  }

  private effectiveParam(n: Node, id: string): number | undefined {
    return this.lastEffective.get(`${n.id}:${id}`) ?? n.params[id];
  }

  private meterWithPeak(id: number, level: number): [number, number] {
    // A touch of stereo spread so the two bars aren't identical.
    const wob = Math.sin(this.time * 1.7 + id) * 0.04;
    const l = Math.max(0, level * (1 + wob));
    const r = Math.max(0, level * (1 - wob));
    const prev = this.meterPeak.get(id) ?? [0, 0];
    const decay = 0.93;
    const peak: [number, number] = [Math.max(l, prev[0] * decay), Math.max(r, prev[1] * decay)];
    this.meterPeak.set(id, peak);
    return [l, r];
  }

  peakOf(id: number): [number, number] { return this.meterPeak.get(id) ?? [0, 0]; }

  private synthScope(level: number): Float32Array {
    const N = 192;
    const buf = new Float32Array(N);
    const v = this.voices.filter(v => v.level > 0).at(-1);
    const hz = v ? 440 * Math.pow(2, (v.note - 69) / 12) : 110;
    const cycles = Math.max(1.5, Math.min(6, hz / 60));
    this.scopePhase = frac(this.scopePhase + 0.07);
    for (let i = 0; i < N; i++) {
      const x = (i / N) * cycles * Math.PI * 2 + this.scopePhase * Math.PI * 2;
      buf[i] = level * (Math.sin(x) * 0.78 + Math.sin(x * 2) * 0.16 + Math.sin(x * 3) * 0.06)
        + (Math.random() - 0.5) * 0.012;
    }
    return buf;
  }

  // ---- status -----------------------------------------------------------

  private applyDebugState(): void {
    const a = this.status.audio;
    if (this.debug.unplugAudio && a.state === 'running') { a.state = 'device-lost'; this.pushStatus(); }
    if (!this.debug.unplugAudio && (a.state === 'device-lost' || a.state === 'no-device')) {
      a.state = 'reconnecting';
      window.setTimeout(() => {
        if (!this.debug.unplugAudio) { this.status.audio.state = 'running'; this.pushStatus(); }
      }, 900);
    }
    if (this.debug.dropouts && this.tick % 20 === 0) a.dropouts++;

    // OSC: a fake message about every 1.5 s while the server is on.
    if (this.osc.enabled && this.debug.oscTraffic && this.tick % 45 === 0) {
      const ids = [...this.nodes.keys()];
      const id = ids.length ? ids[this.tick % ids.length] : 1;
      this.osc.recent = [{ time: this.time, address: `/wirescape/${id}/param`, args: [Math.round(Math.random() * 1000) / 1000] },
        ...this.osc.recent].slice(0, 8);
      this.pushStatus();
    }

    const base = 4 + this.nodes.size * 1.7 + this.voices.length * 0.9;
    const target = Math.min(99, this.debug.cpuSpike ? base + 62 : base);
    a.cpuPercent = Math.round((a.cpuPercent + (target - a.cpuPercent) * 0.25) * 10) / 10;

    const wantExtra = this.debug.extraMidiDevice;
    const hasExtra = this.midiDevices.some(d => d.id === 'midi:2');
    if (wantExtra !== hasExtra) {
      if (wantExtra) this.midiDevices.push({ id: 'midi:2', name: 'Keystation 49', kind: 'midi', enabled: true });
      else this.midiDevices = this.midiDevices.filter(d => d.id !== 'midi:2');
      this.pushStatus();
    }
  }

  private snapshotStatus(): SystemStatus {
    const clockNode = [...this.nodes.values()].find(n => n.type === 'essentials.clock');
    const activity: Record<string, number> = {};
    for (const d of this.midiDevices) {
      activity[d.id] = d.enabled && this.voices.some(v => v.on) ? 1 : 0;
    }
    return {
      audio: { ...this.status.audio },
      midi: { devices: this.midiDevices.map(d => ({ ...d })), activity },
      osc: { ...this.osc, recent: this.osc.recent.map(m => ({ ...m, args: [...m.args] })) },
      ...(clockNode
        ? { clock: { bpm: clockNode.params.bpm ?? 120, running: (clockNode.params.run ?? 1) >= 0.5 } }
        : {}),
    };
  }

  private pushStatus(): void {
    const s = this.snapshotStatus();
    for (const cb of this.statusCbs) cb(s);
  }
}

const frac = (n: number) => n - Math.floor(n);
const key = (c: Connection) => `${c.kind}|${c.from.module}:${c.from.port}|${c.to.module}:${c.to.port}`;
export type { EngineError };

