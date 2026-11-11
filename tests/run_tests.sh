#!/bin/sh
# Builds and runs the host tests: components/hmi_bms/hmi_protocol.* and the LCD UI (needs g++, e.g. in WSL).
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/hmi_protocol_test"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -o "$out" hmi_protocol_test.cpp ../components/hmi_bms/hmi_protocol.cpp
"$out"

# The LCD UI's behaviour: taps, pop-ups, what it asks the device to do
out_ui="${TMPDIR:-/tmp}/hmi_ui_test"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined   -o "$out_ui" ui_test.cpp ../components/hmi_ui/ui*.cpp ../components/hmi_ui/page_*.cpp
"$out_ui"
