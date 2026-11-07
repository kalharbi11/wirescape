// SPDX-License-Identifier: GPL-3.0-only
/**
 * Web demo sharing: the patch travels in the URL hash (`#p=`, gzip + base64url),
 * so a link is the whole patch. Nothing is uploaded anywhere.
 */
import type { PatchV2 } from '../patch/types';

const PREFIX = '#p=';
/** Keeps links pasteable in chat and email; bigger patches still download as files. */
export const MAX_LINK_CHARS = 16_000;

async function pipe(bytes: Uint8Array, stream: CompressionStream | DecompressionStream): Promise<Uint8Array> {
  const out = new Blob([bytes]).stream().pipeThrough(stream);
  return new Uint8Array(await new Response(out).arrayBuffer());
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
  const shared = await decodePatch(location.hash);
  if (shared) await host.loadPatch(shared, 'Shared patch');
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
}
