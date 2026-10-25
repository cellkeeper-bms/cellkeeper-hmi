#!/bin/sh
# Builds and runs the host tests for components/hmi_bms/hmi_protocol.* (needs g++, e.g. in WSL).
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/hmi_protocol_test"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -o "$out" hmi_protocol_test.cpp ../components/hmi_bms/hmi_protocol.cpp
"$out"
