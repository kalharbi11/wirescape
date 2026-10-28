#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Headless tools only: no Qt, device access, or new third-party sources.
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

install_system_packages() {
    if ! command -v apt-get >/dev/null; then
        printf '%s\n' "Missing prerequisites: $*. Install them with the image's package manager." >&2
        exit 1
    fi
    local -a privilege=()
    if (( EUID != 0 )); then
        if command -v sudo >/dev/null && sudo -n true 2>/dev/null; then
            privilege=(sudo -n)
        else
            printf '%s\n' "Missing prerequisites: $*. Ask the image administrator to install them; passwordless sudo is unavailable." >&2
            exit 1
        fi
    fi
    "${privilege[@]}" apt-get update
    "${privilege[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "$@"
}

missing=()
command -v c++ >/dev/null || missing+=(g++)
command -v cc >/dev/null || missing+=(gcc)
command -v python3 >/dev/null || missing+=(python3 python3-venv)
if ((${#missing[@]})); then
    install_system_packages "${missing[@]}"
fi
python_bin="${PYTHON:-python3}"
"$python_bin" -c 'import sys; assert sys.version_info >= (3, 12), "Python 3.12 or newer is required"'

# A local venv works in non-root images and avoids modifying system Python.
if ! "$python_bin" -m venv .foundation-venv; then
    install_system_packages python3-venv
    "$python_bin" -m venv .foundation-venv
fi
source .foundation-venv/bin/activate
export PIP_CACHE_DIR="$repo_root/.tools/pip-cache"
# Match CI. tools/ and the headless CTest suite otherwise use Python's stdlib;
# NumPy experiments, Valgrind/sanitizers, Qt and PortAudio are optional.
python -m pip install --disable-pip-version-check cmake==4.2.3 ninja==1.13.0

printf '#include <optional>\nint main() { std::optional<int> n{17}; return *n != 17; }\n' |
    "${CXX:-c++}" -std=c++17 -x c++ -fsyntax-only -
cmake --version
ninja --version
python tools/dev.py doctor
printf '\nSetup complete. In each development shell, run:\n'
printf '  cd %q && source .foundation-venv/bin/activate\n' "$repo_root"
printf '  python tools/dev.py build --headless\n  python tools/dev.py test\n'
