// SPDX-License-Identifier: GPL-3.0-only
/**
 * Web demo entry: a one-click splash (browsers need a gesture before audio),
 * then the WebBridge stands in for Electron's preload before Studio boots.
 */
import { WebBridge } from './WebBridge';

declare global { interface Window { __wirescapeWeb?: WebBridge } }

export function startWeb(): Promise<void> {
  const base = import.meta.env.BASE_URL;
  const splash = document.createElement('div');
  splash.className = 'web-splash';
  splash.dataset.testid = 'web-splash';
  splash.innerHTML = `
    <div class="web-splash-card">
      <h1>Wirescape</h1>
      <p>A modular synth in your browser. Patch cables between modules, turn knobs, and play notes
        with your computer keyboard (A–K) or the on-screen keys.</p>
      <p class="muted">Web demo: every non-MIDI module and the built-in starters. SHARE copies a link to your patch.
        Best in desktop Chrome or Edge. Use headphones or speakers.</p>
      <button type="button" data-testid="web-start">Start with sound</button>
      <p class="web-splash-error" data-testid="web-error" hidden></p>
    </div>`;
  const style = document.createElement('style');
  style.textContent = `
    .web-splash{position:fixed;inset:0;z-index:10000;display:grid;place-items:center;
      background:rgba(14,14,16,.92);font-family:"IBM Plex Sans",system-ui,sans-serif;color:#e8e6e1}
    .web-splash-card{max-width:30rem;padding:2rem;border:1px solid #333;border-radius:10px;background:#18181b}
    .web-splash-card h1{margin:0 0 .75rem;font:500 1.6rem "IBM Plex Mono",monospace;letter-spacing:.05em}
    .web-splash-card p{line-height:1.5;margin:.5rem 0}.web-splash-card .muted{opacity:.7;font-size:.9rem}
    .web-splash-card button{margin-top:1rem;padding:.7rem 1.4rem;font:500 1rem "IBM Plex Sans",sans-serif;
      border-radius:6px;border:0;background:#e8b04a;color:#111;cursor:pointer}
    .web-splash-error{color:#ff8a7a}`;
  document.head.append(style);
  document.body.append(splash);
  const bridge = new WebBridge();
  window.wirescape = bridge;
  window.__wirescapeWeb = bridge;
  const button = splash.querySelector('button')!;
  const error = splash.querySelector<HTMLElement>('[data-testid="web-error"]')!;
  return new Promise(resolve => {
    button.addEventListener('click', () => {
      button.disabled = true;
      button.textContent = 'Starting…';
      bridge.start(base, (window as unknown as { __wirescapeWebTest?: { readyTimeoutMs?: number; processorUrl?: string } })
        .__wirescapeWebTest ?? {}).then(() => { splash.remove(); resolve(); }, (e: unknown) => {
        button.disabled = false;
        button.textContent = 'Try again';
        error.hidden = false;
        error.textContent = 'Could not start audio: ' + (e instanceof Error ? e.message : String(e)) +
          '. This demo needs a recent desktop browser with WebAssembly and AudioWorklet.';
      });
    });
  });
}
