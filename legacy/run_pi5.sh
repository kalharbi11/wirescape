#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [ -f .venv/bin/activate ]; then . .venv/bin/activate; fi
python -m wirescape "$@"
