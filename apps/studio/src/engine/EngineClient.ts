// SPDX-License-Identifier: GPL-3.0-only
/**
 * Contract 2 — the engine interface.
 *
 * Every piece of talk between the UI and "the engine" goes through this one
 * object. Nothing else in the UI may fake sound or invent a value. The code
 * team implements the same interface for real as `RealEngine`.
 */
import type { PatchV2, Connection } from '../patch/types';

export interface Device {
  id: string;
  name: string;
  /** MIDI: OS endpoint name; id is `name|system_name`. */
  system_name?: string;
  /** Present for audio devices. */
  sampleRates?: number[];
  /** Audio: PortAudio host API (e.g. "Windows WASAPI"); the id is `hostApi|name[#n]`. */
  hostApi?: string;
  /** Optional audio backend capabilities, supplied by the native device enumerator. */
  backend?: string;
  outputChannels?: number;
  supported?: boolean;
  unsupportedReason?: string;
  defaultSampleRate?: number;
  enabled?: boolean;
  kind: 'audio' | 'midi';
}

export type AudioState =
  | 'starting' | 'running' | 'no-device' | 'device-lost' | 'reconnecting';

/** Transient engine MIDI CC observation. Sequence wraps at uint32; compare inequality only. */
export interface MidiLearn {
  armed: true;
  seq?: string;
  deviceId?: string;
  channel?: number;
  controller?: number;
  value?: number;
}

export interface SystemStatus {
  audio: {
    state: AudioState;
    /** A refused hardware configuration; it does not imply device loss or retry. */
    reason?: string;
    deviceId: string | null;
    deviceName: string | null;
    sampleRate: number;
    bufferFrames: number;
    /** Round-trip latency in ms, derived from buffer + sample rate. */
    latencyMs: number;
    cpuPercent: number;
    dropouts: number;
  };
  midi: {
    devices: Device[];
    /** deviceId -> 0..1 activity, decaying. */
    activity: Record<string, number>;
    /** Only present while the engine is armed; no observation fields until first CC. */
    learn?: MidiLearn;
  };
  /** Present only while a Clock module exists in the graph. */
  clock?: { bpm: number; running: boolean };
  /**
   * Optional (phase 5): OSC server status. An engine without OSC omits it and
   * the UI hides the OSC settings. `recent` holds the last few messages in.
   */
  osc?: OscStatus;
}

export interface OscStatus {
  enabled: boolean;
  port: number;
  /** Off by default: listen on localhost only. On: accept from the network. */
  allowNetwork: boolean;
  recent: { time: number; address: string; args: (number | string)[] }[];
}

/** Live values, pushed ~30x a second. Each message is a complete snapshot. */
export interface GrooveTrackObservation {
  /** Measured index, or -1 only after transport and owed releases stop. */
  step: number;
  loop: number;
  playing: boolean;
  fired: boolean;
}

export interface Telemetry {
  /** Actual outer wire generation, attached by RealEngine. */
  gen?: string;
  playhead?: Record<number, [number, number]>;
  groove?: Record<number, [GrooveTrackObservation, GrooveTrackObservation]>;
  /** Monotonic tick counter. */
  tick: number;
  /** Seconds since the engine started. */
  time: number;
  /**
   * Effective (post-modulation) parameter values:
   * `effective[moduleId][paramId] = value in the parameter's own units`.
   */
  effective: Record<number, Record<string, number>>;
  /** LFO-ish phase 0..1 per module, for phase dots. */
  phase: Record<number, number>;
  /** Envelope position 0..1 per module (the loudest live voice). */
  envelope: Record<number, number>;
  /** Panel-labelled meters per module: Output [left, right], Mixer [A, B].
   * Linear levels 0..1+ (above 1 = clipping). */
  meters: Record<number, [number, number]>;
  /** A short scope buffer per module, -1..1. */
  scope: Record<number, Float32Array>;
  /** Event-port activity 0..1 per `moduleId:portId`, decaying. */
  eventActivity: Record<string, number>;
  /**
   * Optional: the current value (-1..1) of each connected signal output,
   * keyed `moduleId:portId`. Drives the dotted "carrying signal" cable look
   * and the info-box sparklines. An engine that omits it still conforms.
   */
  outputs?: Record<string, number>;
  /** MIDI note numbers currently held. */
  heldNotes: number[];
}

export interface EngineError {
  code: 'unknown-module' | 'kind-mismatch' | 'duplicate' | 'not-found' | 'refused' | 'invalid' | 'busy' | 'limit' | 'unavailable';
  message: string;
  /** The engine itself answered with this refusal (not a local supersede/disconnect). */
  engine?: true;
}

/** Thrown/returned when the engine refuses an edit; the UI rolls back. */
export class EngineRefusal extends Error {
  constructor(public readonly detail: EngineError) {
    super(detail.message);
    this.name = 'EngineRefusal';
  }
}

export interface EngineClient {
  // ---- the patch -------------------------------------------------------
  loadGraph(patch: PatchV2): Promise<void>;
  addModule(id: number, type: string, params?: Record<string, number>): void;
  removeModule(id: number): void;
  connect(c: Connection): void;
  disconnect(c: Connection): void;
  setParam(moduleId: number, paramId: string, value: number): void;
  setModAmount(c: Connection, amount: number): void;

  // ---- playing ---------------------------------------------------------
  noteOn(note: number, velocity: number): void;
  noteOff(note: number): void;
  panic(): void;

  // ---- live values -----------------------------------------------------
  /** Release-safe, current-generation stopped Apply gate; ACK alone is insufficient. */
  canApplyGroove?(moduleId: number): boolean;
  onTelemetry(cb: (t: Telemetry) => void): () => void;
  onStatus(cb: (s: SystemStatus) => void): () => void;
  /** Async refusals of optimistic real-engine edits (FakeEngine remains synchronous). */
  onRefusal?(cb: (r: { requestId: string; type: string; detail: EngineError; gen: string }) => void): () => void;

  // ---- devices ---------------------------------------------------------
  listAudioDevices(): Promise<Device[]>;
  selectAudioDevice(id: string): void;
  listMidiDevices(): Promise<Device[]>;
  setMidiEnabled(id: string, on: boolean): void;
  /** Capability-gated. Single engine-side armed observation, no DSP/document state. */
  midiLearn?(armed: boolean): void;

  // ---- optional (phase 5): engines may omit these; the UI hides what's missing
  setSampleRate?(hz: number): void;
  setBufferFrames?(frames: number): void;
  setOsc?(cfg: Partial<Pick<OscStatus, 'enabled' | 'port' | 'allowNetwork'>>): void;
}
