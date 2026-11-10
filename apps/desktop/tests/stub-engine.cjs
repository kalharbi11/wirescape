// SPDX-License-Identifier: GPL-3.0-only
// Null-sink, fake-MIDI protocol fixture. Never opens devices or network sockets.
const fs = require('node:fs');
const readline = require('node:readline');
const flags = process.argv.slice(2);
// Explicitly record forwarded flags in a device/network-free child fixture.
if (process.env.STUB_FLAGS_FILE) fs.writeFileSync(process.env.STUB_FLAGS_FILE, JSON.stringify(flags));
if (!flags.includes('--null-audio') ||
    flags.some(a => a === '--confirm-audio' || a === '--confirm-midi')) {
  process.stderr.write('Test stub requires --null-audio and no confirm flags\n');
  process.exit(2);
}
const session = 'stub-' + process.pid;
const send = object => process.stdout.write(JSON.stringify(object) + '\n');
const audio = { state: process.env.STUB_AUDIO_RUNNING === '1' ? 'running' : 'no-device',
  deviceId: null, deviceName: null, sampleRate: 48000, bufferFrames: 256,
  latencyMs: 0, cpuPercent: 0, dropouts: 0 };
function announce() {
  send({ v: 0, session, type: 'hello', protocol: 0,
    engine: { version: 'test', sha: 'stub' }, capabilities: [],
    catalogue: { modules: [] }, audio, midi: { devices: [], activity: {} } });
  if (process.env.STUB_AUDIO_RUNNING === '1') send({ v: 0, session, gen: '0', type: 'status', status: { audio } });
}
const startupDelay = Number(process.env.STUB_STARTUP_DELAY_MS) || 0;
if (startupDelay > 0) setTimeout(announce, startupDelay);
else announce();
// Device-free crash-loop fixture: records every spawn, then exits unexpectedly.
if (process.env.STUB_CRASH_COUNT_FILE) {
  const file = process.env.STUB_CRASH_COUNT_FILE;
  let previous = 0;
  try { previous = Number(fs.readFileSync(file, 'utf8')) || 0; } catch {}
  fs.writeFileSync(file, String(previous + 1));
  setTimeout(() => process.exit(9), previous === 0 ? 1500 : 300);
}
const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
input.on('line', line => {
  let request;
  try { request = JSON.parse(line); } catch { return; }
  if (process.env.STUB_CRASH_ON_PANIC === '1' && request.type === 'panic') process.exit(9);
  send({ v: 0, session, re: request.id, ok: true, gen: '0' });
});
input.once('close', () => {
  if (process.env.STUB_EXIT_MARKER) fs.writeFileSync(process.env.STUB_EXIT_MARKER, 'stdin EOF');
  process.exit(0);
});
