<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi USB MIDI → CHAOS demo

Use the exact accepted demo build, not an old main checkout. This preset needs
J2 + J4 + the CHAOS catalogue integration. It uses current CHAOS-lite synthesis,
with no tape stage. Other synthesis/body/drift features are retained.

1. User connects USB-to-aux audio adapter and the powered USB hub to the Pi;
   Launchkey/LPD8 (or M8/Launchpad) connect to the hub. HDMI is display-only.
2. From a UI-enabled Release build on the Pi desktop, launch
   `build/pi-ui/Release/wirescape_studio --modules build/pi-ui --confirm-midi --fullscreen`.
   For a persistent `tools/dev.py build` checkout, use `build/native/Release/wirescape_studio`
   and `--modules build/native` instead. Keep that checkout outside runner cleanup;
   use the accepted SHA, not a temporary workflow build directory. Esc leaves fullscreen.
3. Open `native/examples/chaos_usb_midi.json`. While stopped, use MIDI settings
   to enable the actual device endpoints. No device names are guessed or enabled
   by this preset. Select performance/note ports as shown by the device, not an
   unrelated control/DAW port. Save a local working copy with those selections.
4. Select the USB audio adapter in the bottom physical-output selector, then
   Start audio. Output-card graph selection is not the hardware selector.
   Stop audio before Refresh/changing outputs. Start at a low listening level.
5. Play notes on either controller or sequence notes from M8. MIDI In feeds the
   four-voice converter; pitch/gate/velocity drive both CHAOS instances. The two
   hardware sources share this four-voice instrument; separate converters/channel
   filters can separate instruments later. No Python or OSC bridge is required.
6. Check note-off, repeated notes, sustain if used, and Panic. User-only rehearsal:
   listen for one minute, stop playback, then test reconnect/reselection while
   stopped. Record the tested SHA, adapter/ports and any dropouts. A runner's
   offline pass does not certify audible output or physical MIDI.

CC-to-CV is available in J4 but this starter preset does not guess hardware CC
assignments. Internal clock runs independently; external MIDI clock follow is
not implemented/claimed by this preset. Do not enable a MIDI echo/return loop.
