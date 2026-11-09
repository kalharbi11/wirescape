// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import { test, before, after } from 'node:test';
import { mkdir, readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { existsSync } from 'node:fs';
import { chromium } from '@playwright/test';

const executablePath = process.env.WIRESCAPE_CHROMIUM ??
  ['/opt/pw-browsers/chromium', '/usr/bin/chromium', '/usr/bin/google-chrome', '/opt/google/chrome/chrome'].find(existsSync);
let server, study;
before(async () => {
  // Serve only the offline fixture and its local CSS; no app build or engine.
  const files = new Map(await Promise.all(['groove-s1-study.html', 'groove-s1-study.css', 'tokens.css']
    .map(async name => [`/${name}`, await readFile(new URL(`../ui/${name}`, import.meta.url))])));
  server = createServer((request, response) => {
    const data = files.get(request.url);
    response.writeHead(data ? 200 : 204, { 'Content-Type': request.url.endsWith('.css') ? 'text/css' : 'text/html' });
    response.end(data);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  study = `http://127.0.0.1:${server.address().port}/groove-s1-study.html`;
});
after(async () => { await new Promise(resolve => server.close(resolve)); });

test('recovery preserves dirty and refused candidates until explicit Apply ACK', async () => {
  const browser = await chromium.launch({ executablePath, args: ['--no-sandbox'] });
  try {
    for (const refuse of [false, true]) {
      const page = await browser.newPage();
      await page.goto(study);
      await page.locator('#key').fill('72');
      await page.locator('#key').press('Tab');
      if (refuse) {
        await page.locator('#apply').click();
        await page.locator('#sim-refuse').click();
      }
      await page.locator('#sim-disconnect').click();
      assert.equal(await page.locator('#key').isDisabled(), true);
      assert.equal(await page.locator('#start').isDisabled(), true);
      await page.locator('#sim-recover').click();
      assert.equal(await page.locator('#key').inputValue(), '72');
      assert.equal(await page.locator('#apply').isEnabled(), true);
      assert.equal(await page.locator('#start').isDisabled(), true);
      assert.match(await page.locator('#global-summary').innerText(), /DRAFT DIFFERS/);
      if (refuse) assert.match(await page.locator('#loaded-status').innerText(), /Refused/);
      // Undo back to the original snapshot proves recovery did not acknowledge 72.
      await page.locator('#undo').click();
      assert.match(await page.locator('#global-summary').innerText(), /DRAFT MATCHES/);
      if (refuse) assert.equal(await page.locator('#start').isDisabled(), true);
      await page.locator('#redo').click();
      await page.locator('#apply').click();
      assert.equal(await page.locator('#start').isDisabled(), true);
      await page.locator('#sim-ack').click();
      assert.equal(await page.locator('#start').isEnabled(), true);
      assert.match(await page.locator('#global-summary').innerText(), /STOPPED/);
      await page.locator('#start').click();
      await page.locator('#stop').click();
      assert.equal(await page.locator('#key').isDisabled(), true);
      await page.locator('#sim-complete').click();
      assert.equal(await page.locator('#key').isEnabled(), true);
      await page.close();
    }
  } finally { await browser.close(); }
});

for (const [width, height] of [[1280, 720], [1920, 1080], [390, 844]]) {
  test(`offline geometry, console and keyboard focus at ${width}x${height}`, async () => {
    const browser = await chromium.launch({ executablePath, args: ['--no-sandbox'] });
    try {
      const page = await browser.newPage({ viewport: { width, height }, reducedMotion: 'reduce' });
      const errors = [];
      page.on('pageerror', e => errors.push(e.message));
      page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
      await page.goto(study);
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
      assert.equal(await page.locator('#key').evaluate(e => e.getBoundingClientRect().height >= 44), true);
      assert.equal(await page.locator('.step-cell.is-selected').evaluate(e =>
        getComputedStyle(e).color === getComputedStyle(document.querySelector('#start')).color), true);
      const scroller = page.locator('.step-scroller');
      if (width === 390) {
        assert.equal(await scroller.evaluate(e => e.scrollWidth > e.clientWidth), true);
        await scroller.focus();
        assert.equal(await scroller.evaluate(e => getComputedStyle(e).outlineStyle), 'solid');
        await page.keyboard.press('ArrowRight');
        await page.waitForFunction(() => document.querySelector('.step-scroller').scrollLeft > 0);
        await scroller.evaluate(e => { e.scrollLeft = 0; });
        await page.locator('#page-1').click();
        assert.match(await page.locator('#steps button').last().getAttribute('aria-label'), /step 32/);
        await page.locator('#page-0').click();
      }
      await page.locator('#start').focus();
      await page.keyboard.press('Tab');
      assert.equal(await page.evaluate(() => document.activeElement.id), 'reset');
      assert.equal(await page.locator('#reset').evaluate(e => getComputedStyle(e).outlineStyle), 'solid');
      assert.deepEqual(errors, []);
      if (process.env.WIRESCAPE_GROOVE_ARTIFACTS) {
        const directory = process.env.WIRESCAPE_GROOVE_ARTIFACTS;
        await mkdir(directory, { recursive: true });
        await page.screenshot({ path: `${directory}/groove-${width}x${height}.png`, fullPage: true });
        await page.locator('#editor-title').scrollIntoViewIfNeeded();
        await page.screenshot({ path: `${directory}/groove-editor-${width}x${height}.png` });
      }
    } finally { await browser.close(); }
  });
}
