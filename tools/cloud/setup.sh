#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
bash tools/ci/codex_cloud_setup.sh
source .foundation-venv/bin/activate
python tools/dev.py build --headless
