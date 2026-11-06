// SPDX-License-Identifier: GPL-3.0-only
import { contextBridge, ipcRenderer } from 'electron';

const LIMIT = 2 * 1024 * 1024;
type Wire = Record<string, unknown>;
const listeners = new Set<(message: Wire) => void>();
let requestedInitialHello = false;
let lastHello: Wire | null = null;

function checkedCommand(value: Wire): string {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new TypeError('Invalid command');
  const seen = new WeakSet<object>();
  function validate(v: unknown, depth = 0): void {
    if (depth > 64) throw new RangeError('Command nesting too deep');
    if (typeof v === 'number' && !Number.isFinite(v)) throw new TypeError('Non-finite number');
    if (typeof v === 'undefined' || typeof v === 'function' || typeof v === 'symbol' ||
        typeof v === 'bigint') throw new TypeError('Unserializable command');
    if (v && typeof v === 'object') {
      if (seen.has(v)) throw new TypeError('Cyclic command');
      seen.add(v);
      if (!Array.isArray(v) && Object.getPrototypeOf(v) !== Object.prototype &&
          Object.getPrototypeOf(v) !== null) throw new TypeError('Invalid command object');
      for (const child of Object.values(v)) validate(child, depth + 1);
      seen.delete(v);
    }
  }
  validate(value);
  let raw: string;
  try { raw = JSON.stringify(value); } catch { throw new TypeError('Unserializable command'); }
  if (!raw || new TextEncoder().encode(raw).length > LIMIT) throw new RangeError('Command too large');
  const obj: Wire = JSON.parse(raw);
  if (obj.v !== 0 || typeof obj.type !== 'string' || obj.type.length > 64 ||
      typeof obj.id !== 'string' || !/^[\x21-\x7e]{1,64}$/.test(obj.id)) {
    throw new TypeError('Invalid protocol envelope');
  }
  return raw;
}

ipcRenderer.on('wirescape:message', (_event, raw: string) => {
  if (typeof raw !== 'string' || new TextEncoder().encode(raw).length > LIMIT) return;
  let value: Wire;
  try {
    value = JSON.parse(raw);
    if (!value || Array.isArray(value) || typeof value !== 'object' || value.v !== 0) return;
  } catch { return; }
  if (value.type === 'hello') {
    // One hello per engine session: main may push it at start and again on 'ready'.
    if (lastHello && lastHello.session === value.session) return;
    lastHello = value;
  }
  if (value.type === 'app.engineExited') lastHello = null;
  for (const listener of [...listeners]) {
    try { listener(value); } catch (e) { console.error('Wirescape message listener failed', e); }
  }
});

// Keyboard delivery in synthetic tests and some WMs does not always trigger
// webContents' before-input-event; main still checks the trusted sender.
window.addEventListener('keydown', event => {
  if (event.key === 'Escape') ipcRenderer.send('wirescape:escape');
}, true);

contextBridge.exposeInMainWorld('wirescape', Object.freeze({
  send(value: Wire): void {
    ipcRenderer.send('wirescape:send', checkedCommand(value));
  },
  onMessage(listener: (message: Wire) => void): () => void {
    if (typeof listener !== 'function') throw new TypeError('Expected callback');
    listeners.add(listener);
    // Replay only to this late subscriber, not to every existing listener.
    if (lastHello) queueMicrotask(() => {
      if (listeners.has(listener) && lastHello) listener(lastHello);
    });
    if (!requestedInitialHello) {
      requestedInitialHello = true;
      ipcRenderer.send('wirescape:ready');
    }
    return () => { listeners.delete(listener); };
  },
  openPatch(): Promise<{ text: string; name: string } | null> {
    return ipcRenderer.invoke('wirescape:open');
  },
  savePatch(text: string, options?: { saveAs?: boolean }): Promise<{ name: string } | null> {
    if (typeof text !== 'string' || new TextEncoder().encode(text).length > LIMIT) {
      return Promise.reject(new TypeError('Patch too large or not text'));
    }
    if (options !== undefined && (!options || typeof options !== 'object' ||
        typeof options.saveAs !== 'undefined' && typeof options.saveAs !== 'boolean')) {
      return Promise.reject(new TypeError('Invalid save options'));
    }
    return ipcRenderer.invoke('wirescape:save', { text, saveAs: options?.saveAs === true });
  },
  platform: process.platform
}));
