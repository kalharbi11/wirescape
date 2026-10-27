#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
sudo apt-get update
sudo apt-get install -y git cmake ninja-build build-essential pkg-config python3-dev python3-venv libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libasound2-dev libgtk-3-dev libcurl4-openssl-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev libjack-jackd2-dev
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements.txt
if [ ! -f surge/CMakeLists.txt ]; then git clone --depth 1 https://github.com/surge-synthesizer/surge.git surge; fi
(cd surge && git submodule update --init --recursive --depth 1)
cmake -S surge -B surge/ignore/bpy -G Ninja -DSURGE_BUILD_PYTHON_BINDINGS=ON -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE="$(which python)"
cmake --build surge/ignore/bpy --target surgepy --parallel 2
mkdir -p surgepy_bin
find surge/ignore/bpy -name 'surgepy*.so' -exec cp {} surgepy_bin/ \;
python -c 'import surge_path, surgepy; print("surgepy imported OK")'
echo 'Start with: source .venv/bin/activate && python ui.py'
