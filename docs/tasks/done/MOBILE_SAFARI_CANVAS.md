<!-- SPDX-License-Identifier: GPL-3.0-only -->
# iPhone Safari patch canvas gestures and chrome (#334)

Restore dependable two-finger zoom on a touch device when either contact
begins on a module header, knob, port, or empty canvas. Handle multi-touch at
capture phase, before child controls stop or capture pointer events. The
gesture must scale and pan around the initial midpoint world position, cancel
an in-progress cable, and avoid moving modules or changing parameters during
a pinch. Single-finger panning, module dragging and parameter edits must
continue to work after both fingers lift.

At small browser widths, keep the top toolbar and bottom status bar inside
the dynamic viewport. Make the toolbar horizontally scrollable for actions
that do not fit; keep zoom controls near its leading edge. Protect mobile
safe areas without disabling browser-wide accessibility zoom.

Browser tests use Chromium touch emulation. **Owner / device check:** iPhone
Safari portrait and landscape, with expanded and collapsed browser bars, and
pinches on background / selected header / knob / port. Confirm the address
bar and browser chrome do not zoom the whole document. This test must not be
claimed as completed from desktop emulation.

```acceptance
tier: T1
size: <= 450 changed lines, <= 6 files
must_pass:
  - ci: web job typecheck, build and Playwright including tests/mobile-gestures.spec.ts
  - ci: web-demo site job green
  - bash -c "git diff --check"
stops: [touch gesture changes outside canvas, disabling accessibility page zoom globally, test removal, new dependency, ABI/DSP changes]
```
