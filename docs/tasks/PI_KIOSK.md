<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi kiosk launcher and owner instructions (#166)

Document a persistent accepted build outside runner cleanup, owner-installed XDG
fullscreen startup, Escape, USB-output selection and reversible disabling. Ship
a disabled template; never configure the Pi or open devices. Validate the template
and owner-rendered copy with desktop-file-validate when available. Portable CI
uses the documented standard-library structural/safe-launch checks below; this
is not a claim of physical desktop startup. Require current GUI provenance before
the owner enables the launcher; no automatic playback or MIDI consent.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -m unittest tools/foundation/tests/test_doc_links.py
  - python -c "import configparser,shlex; c=configparser.ConfigParser(interpolation=None); c.read('tools/pi/wirescape-kiosk.desktop'); s=c['Desktop Entry']; assert s['Type']=='Application' and s['Name'] and s.getboolean('Hidden') and not s.getboolean('Terminal'); args=shlex.split(s['Exec']); assert args[0]==s['TryExec'] and '--fullscreen' in args and '--confirm-midi' not in args; print('desktop entry structure and safe launch defaults valid')"
stops: [Pi configuration change, runtime change, new dependency, device access]
```
