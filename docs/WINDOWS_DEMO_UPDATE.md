<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Update Wirescape on Windows

To update to a new version (1.0 → 1.1 → …), see
**[INSTALL.md → Update to a later version](INSTALL.md#update-to-a-later-version)**.
In short, build the version's commit in a new folder, test it, and keep the old folder
until the new one passes.

The old Qt package and shortcut flow (`dev.py package`, `startup-test`,
`launch --qt-prefix`, `install_windows_shortcuts.ps1`) is retired. Don't
use it for 1.0 or later. It is still in git history.
