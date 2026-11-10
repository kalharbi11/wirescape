<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Codex cloud setup

Setup-script field for this environment:
`cd /workspace/wirescape && bash tools/cloud/setup.sh`.
The image supplies Python 3.12+ and GCC/Clang. The script creates an ignored
`.foundation-venv`, installs CI-pinned CMake/Ninja and warms the headless build.
Activate `.foundation-venv/bin/activate` in each subsequent development shell.
Qt, PortAudio, real devices and privileged installation are not required.

Allow HTTPS to `github.com`, `api.github.com`, `objects.githubusercontent.com`,
`pypi.org` and `files.pythonhosted.org`. GitHub Actions log downloads may redirect
to `productionresultssa*.blob.core.windows.net`; because environment settings
require exact hostnames or leading wildcards, add the exact redirect hostname
reported by a blocked download rather than a partial wildcard. Preserve existing
network rules. `tools/ci_logs.py` uses existing `GITHUB_TOKEN` or `GH_TOKEN`
authentication; never enter credential values in chat or checked-in files.

If the proxy blocks `codeload.github.com` (FetchContent tarballs return 403),
`setup.sh` runs `tools/cloud/git_deps.py`, which clones each native dependency at
its pinned SHA into `build/deps-cache/`, applies the Macro Oscillator patches and
writes `build/deps-cache/fetchcontent.cmake`; `tools/dev.py build` passes it to
CMake with `-C`. Run `python tools/cloud/git_deps.py --clean` to go back to normal
downloads. When bumping a pinned SHA in a CMakeLists, update `git_deps.py` too.
