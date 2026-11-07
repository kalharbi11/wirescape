// SPDX-License-Identifier: GPL-3.0-only
import '@fontsource/ibm-plex-sans/latin-400.css';
import '@fontsource/ibm-plex-sans/latin-500.css';
import '@fontsource/ibm-plex-mono/latin-400.css';
import '@fontsource/ibm-plex-mono/latin-500.css';
import '@fontsource/ibm-plex-mono/latin-600.css';
import '../ui/tokens.css';
import './theme/tokens.css';
import './styles/app.css';
import './styles/controls.css';
import './styles/displays.css';
import './styles/info.css';
import './styles/chrome.css';
import { App } from './app/App';

const mount = document.getElementById('root');
if (!mount) throw new Error('No #root element');

// Web demo build (VITE_WIRESCAPE_WEB=1): the WebAssembly engine stands in for Electron.
if (import.meta.env.VITE_WIRESCAPE_WEB === '1') await (await import('./web/start')).startWeb();

App.boot(mount)
  .then(app => {
    // Test hooks: a scripted test can drive the whole app from here.
    (window as unknown as Record<string, unknown>).__wirescapeTest = app.testApi();
    document.body.dataset.ready = 'true';
  })
  .catch((err: unknown) => {
    const msg = err instanceof Error ? err.message : String(err);
    mount.innerHTML = `<div class="boot-error" data-testid="boot-error">
      <h1>Wirescape could not start</h1><p>${msg}</p></div>`;
    document.body.dataset.ready = 'error';
  });
