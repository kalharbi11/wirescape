#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
echo 'WIRESCAPE / Raspberry Pi 5 - host dependencies only.'
if ! command -v python3 >/dev/null; then echo 'Python 3 is required' >&2; exit 1; fi
if command -v apt-get >/dev/null; then
 echo 'Install system packages manually if needed: sudo apt install python3-venv python3-dev libportaudio2 portaudio19-dev libasound2-dev'
fi
python3 -m venv --system-site-packages .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python -m unittest discover -s tests -q
cat <<'EOF'
Wirescape host dependencies installed.
Next: build surgepy for Pi ARM64 using modules/chaos/build_pi5.sh or point
SURGEPY_DIR to a compatible previously compiled ARM64 surgepy .so file.
No Windows .pyd can run on a Raspberry Pi.
Run with: ./run_pi5.sh
EOF
