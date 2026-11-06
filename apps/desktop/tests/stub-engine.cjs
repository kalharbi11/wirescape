// SPDX-License-Identifier: GPL-3.0-only
// Null-sink, fake-MIDI protocol fixture. Never opens devices or network sockets.
const fs = require('node:fs');
const readline = require('node:readline');
const flags = process.argv.slice(2);
if (!flags.includes('--null-audio') ||
    flags.some(a => a === '--confirm-audio' || a === '--confirm-midi')) {
  process.stderr.write('Test stub requires --null-audio and no confirm flags\n');
  process.exit(2);
}
const session = 'stub-' + process.pid;
const send = object => process.stdout.write(JSON.stringify(object) + '\n');
send({
  v: 0, session, type: 'hello', protocol: 0,
  engine: { version: 'test', sha: 'stub' }, capabilities: [],
  catalogue: { modules: [] },
  audio: { state: 'no-device', deviceId: null, deviceName: null,
    sampleRate: 48000, bufferFrames: 256, latencyMs: 0, cpuPercent: 0, dropouts: 0 },
  midi: { devices: [], activity: {} }
});
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
