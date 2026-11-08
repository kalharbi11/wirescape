// SPDX-License-Identifier: GPL-3.0-only
/**
 * Web demo sharing: the patch travels in the URL hash (`#p=`, gzip + base64url),
 * so a link is the whole patch. Nothing is uploaded anywhere.
 */
import type { PatchV2 } from '../patch/types';

const PREFIX = '#p=';
/** Keeps links pasteable in chat and email; bigger patches still download as files. */
export const MAX_LINK_CHARS = 16_000;

/** Same 2 MiB ceiling as the engine's patch limit; stops reading past it (no gzip bombs). */
const MAX_PATCH_BYTES = 2 * 1024 * 1024;
async function pipe(bytes: Uint8Array, stream: CompressionStream | DecompressionStream): Promise<Uint8Array> {
  const reader = new Blob([bytes]).stream().pipeThrough(stream).getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
    if (total > MAX_PATCH_BYTES) { await reader.cancel(); throw new Error('Patch too large'); }
    chunks.push(value);
  }
  const out = new Uint8Array(total);
  let at = 0;
  for (const c of chunks) { out.set(c, at); at += c.length; }
  return out;
}
const toBase64Url = (bytes: Uint8Array): string => {
  let s = '';
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
};
const fromBase64Url = (text: string): Uint8Array => {
  const s = atob(text.replace(/-/g, '+').replace(/_/g, '/'));
  return Uint8Array.from(s, c => c.charCodeAt(0));
};

export async function encodePatch(patch: PatchV2): Promise<string> {
  const json = new TextEncoder().encode(JSON.stringify(patch));
  return PREFIX + toBase64Url(await pipe(json, new CompressionStream('gzip')));
}

/** Returns null for anything that isn't a well-formed shared patch. */
export async function decodePatch(hash: string): Promise<PatchV2 | null> {
  if (!hash.startsWith(PREFIX) || hash.length > MAX_LINK_CHARS * 2) return null;
  try {
    const bytes = await pipe(fromBase64Url(hash.slice(PREFIX.length)), new DecompressionStream('gzip'));
    const patch = JSON.parse(new TextDecoder().decode(bytes)) as PatchV2;
    return patch && Array.isArray(patch.modules) && Array.isArray(patch.connections) ? patch : null;
  } catch {
    return null;
  }
}

export interface ShareHost {
  save(): PatchV2;
  loadPatch(patch: PatchV2, name?: string): Promise<void>;
}

/** Adds the SHARE button and loads a shared patch from the URL, if there is one. */
export async function installSharing(host: ShareHost): Promise<void> {
  const save = document.querySelector('[data-testid="save-patch"]');
  if (!save?.parentElement) return;
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'action';
  button.textContent = 'SHARE';
  button.title = 'Copy a link that opens this patch';
  button.dataset.testid = 'share-patch';
  button.addEventListener('click', () => void (async () => {
    const hash = await encodePatch(host.save());
    if (hash.length > MAX_LINK_CHARS) { button.textContent = 'TOO BIG'; return; }
    history.replaceState(null, '', location.pathname + location.search + hash);
    try { await navigator.clipboard.writeText(location.href); button.textContent = 'COPIED'; }
    catch { button.textContent = 'IN URL'; }
    setTimeout(() => { button.textContent = 'SHARE'; }, 1800);
  })());
  save.parentElement.insertBefore(button, save);
  // Load a shared patch once, then drop the hash so a reload keeps later edits.
  const shared = await decodePatch(location.hash);
  if (location.hash.startsWith(PREFIX)) history.replaceState(null, '', location.pathname + location.search);
  if (shared) {
    try { await host.loadPatch(shared, 'Shared patch'); } catch { /* the engine refused it; keep the current patch */ }
  }
}
