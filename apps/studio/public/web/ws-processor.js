// SPDX-License-Identifier: GPL-3.0-only
// Web demo AudioWorklet: hosts the standalone wirescape.wasm engine. Commands
// arrive on the port between render quanta, so engine state has one thread.
/* global sampleRate, currentTime, registerProcessor, AudioWorkletProcessor */

// AudioWorkletGlobalScope has no TextEncoder/TextDecoder; a small UTF-8 codec.
function encode(text) {
  const out = [];
  for (const ch of text) {
    let c = ch.codePointAt(0);
    if (c < 0x80) out.push(c);
    else if (c < 0x800) out.push(0xc0 | (c >> 6), 0x80 | (c & 63));
    else if (c < 0x10000) out.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
    else out.push(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
  }
  return out;
}
function decode(bytes, start) {
  let s = '', i = start;
  while (bytes[i]) {
    let c = bytes[i++];
    if (c >= 0xf0) c = ((c & 7) << 18) | ((bytes[i++] & 63) << 12) | ((bytes[i++] & 63) << 6) | (bytes[i++] & 63);
    else if (c >= 0xe0) c = ((c & 15) << 12) | ((bytes[i++] & 63) << 6) | (bytes[i++] & 63);
    else if (c >= 0xc0) c = ((c & 31) << 6) | (bytes[i++] & 63);
    s += String.fromCodePoint(c);
  }
  return s;
}

// The standalone module imports a handful of WASI calls; none touch devices or files.
function imports(module, state) {
  const out = {};
  const view = () => new DataView(state.exports.memory.buffer);
  const known = {
    clock_time_get: (_id, _precision, ptr) => {
      view().setBigUint64(ptr, BigInt(Math.round(currentTime * 1e9)), true);
      return 0;
    },
    random_get: (ptr, len) => {
      const bytes = new Uint8Array(state.exports.memory.buffer, ptr, len);
      for (let i = 0; i < len; i++) bytes[i] = (Math.random() * 256) | 0;
      return 0;
    },
    fd_write: (_fd, iovs, count, written) => {
      const v = view();
      let total = 0;
      for (let i = 0; i < count; i++) total += v.getUint32(iovs + i * 8 + 4, true);
      v.setUint32(written, total, true);
      return 0;
    },
    environ_sizes_get: (count, size) => { view().setUint32(count, 0, true); view().setUint32(size, 0, true); return 0; },
    args_sizes_get: (count, size) => { view().setUint32(count, 0, true); view().setUint32(size, 0, true); return 0; },
    proc_exit: code => { throw new Error('wasm exit ' + code); },
  };
  for (const entry of WebAssembly.Module.imports(module)) {
    if (entry.kind !== 'function') continue;
    out[entry.module] ??= {};
    out[entry.module][entry.name] = known[entry.name] ?? (() => 0);
  }
  return out;
}

class WirescapeProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.state = { exports: null };
    this.blocks = 0;
    this.gen = '0';
    this.error = '';
    try {
      const module = options.processorOptions.module;
      const instance = new WebAssembly.Instance(module, imports(module, this.state));
      this.state.exports = instance.exports;
      const x = instance.exports;
      if (x._initialize) x._initialize();
      if (!x.wsw_init(sampleRate)) throw new Error('Engine refused sample rate ' + sampleRate);
      this.port.postMessage({ kind: 'ready', catalogue: JSON.parse(this.text(x.wsw_catalogue())) });
    } catch (e) {
      this.error = String(e && e.message ? e.message : e);
      this.port.postMessage({ kind: 'failed', message: this.error });
    }
    this.port.onmessage = event => this.command(event.data);
  }
  text(ptr) { return decode(new Uint8Array(this.state.exports.memory.buffer), ptr); }
  call(fn, text) {
    const x = this.state.exports;
    const bytes = encode(text);
    const ptr = x.wsw_alloc(bytes.length + 1);
    if (!ptr) return '{"ok":false,"code":"limit","message":"Out of memory"}';
    const heap = new Uint8Array(x.memory.buffer, ptr, bytes.length + 1);
    heap.set(bytes);
    heap[bytes.length] = 0;
    try { return this.text(fn(ptr)); } finally { x.wsw_free(ptr); }
  }
  command(message) {
    if (!this.state.exports || !message || message.kind !== 'command') return;
    let reply;
    try { reply = JSON.parse(this.call(this.state.exports.wsw_command, JSON.stringify(message.command))); }
    catch (e) { reply = { ok: false, code: 'unavailable', message: String(e && e.message ? e.message : e), gen: this.gen }; }
    if (typeof reply.gen === 'string') this.gen = reply.gen;
    this.port.postMessage({ kind: 'reply', id: message.id, reply });
  }
  process(_inputs, outputs) {
    const x = this.state.exports;
    const out = outputs[0];
    if (!x || !out || !out.length) return true;
    const frames = out[0].length;
    try {
      const left = x.wsw_render(frames);
      const right = x.wsw_right();
      if (left) {
        const heap = x.memory.buffer;
        out[0].set(new Float32Array(heap, left, frames));
        if (out[1]) out[1].set(new Float32Array(heap, right, frames));
      }
    } catch (e) {
      // A trapped engine stays silent; the page reports it once.
      this.state.exports = null;
      this.port.postMessage({ kind: 'failed', message: String(e && e.message ? e.message : e) });
      return true;
    }
    if (++this.blocks % 12 === 0) {
      const telemetry = JSON.parse(this.text(x.wsw_telemetry()));
      this.port.postMessage({ kind: 'telemetry', gen: this.gen, telemetry, peak: x.wsw_peak() });
    }
    return true;
  }
}
registerProcessor('wirescape-engine', WirescapeProcessor);
