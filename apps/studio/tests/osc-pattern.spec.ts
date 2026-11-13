// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { matchOscAddress } from '../src/app/oscCapture';

test('OSC matcher mirrors native wildcard, class and nested-alternative fixtures', () => {
  for (const pattern of ['/pico1/knob/*', '/pico?/knob/[1-9]', '/{pico1,esp32}/knob/3']) {
    expect(matchOscAddress(pattern, '/pico1/knob/3')).toBe(true);
    expect(matchOscAddress(pattern, '/pico1/knob/3/extra')).toBe(false);
  }
  expect(matchOscAddress('/a/[!0-3]/{x,{y,z}}', '/a/5/z')).toBe(true);
  expect(matchOscAddress('/a/[!0-3]/{x,{y,z}}', '/a/1/z')).toBe(false);
  expect(matchOscAddress('/a/[!0-3]/{x,{y,z}}', '/a///z')).toBe(false);
  expect(matchOscAddress('/a/*', '/a/')).toBe(true);
  expect(matchOscAddress('/a/?', '/a/1')).toBe(true);
  expect(matchOscAddress('/a/?', '/a/12')).toBe(false);
  expect(matchOscAddress('/a/\\*', '/a/*')).toBe(true);
  expect(matchOscAddress('/a/\\*', '/a/any')).toBe(false);
  expect(matchOscAddress('/{x,y}', '/y')).toBe(true);
  expect(matchOscAddress('/abc', '/abcd')).toBe(false);
  expect(matchOscAddress('/abc', '/abc\n')).toBe(false);
  for (const pattern of ['/{oops', '/[z-a]', '/[', '/a\\', '/a}', '/a]']) {
    expect(matchOscAddress(pattern, '/a')).toBe(false);
  }
});
