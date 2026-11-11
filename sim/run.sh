#!/bin/sh
# Builds the LCD UI simulator and renders every scenario to sim/out/*.png (run in WSL or Linux).
# Needs g++, and a Python with Pillow to turn the images into PNG (ESPHome's has it):
#   PYTHON=~/esphome/venv/bin/python sh sim/run.sh
set -e
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-$HOME/esphome/venv/bin/python}"
bin="${TMPDIR:-/tmp}/hmi_ui_sim"
mkdir -p sim/out
rm -f sim/out/*.ppm sim/out/*.png sim/out/*.txt
g++ -std=c++17 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -o "$bin" \
  sim/sim.cpp components/hmi_ui/*.cpp
"$bin" sim/out
"$PYTHON" - <<'EOF'
import glob, os
from PIL import Image
for path in sorted(glob.glob("sim/out/*.ppm")):
    Image.open(path).save(path[:-4] + ".png")
    os.remove(path)
EOF
