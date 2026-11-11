#!/bin/bash
# Checks the repo without a device (run in WSL, where ESPHome is installed under ~/esphome):
#   tools/check.sh            host tests, config check and entity lists against tests/entities/
#   tools/check.sh --update   the same, rewriting tests/entities/ (commit the change with its reason)
#   tools/check.sh --build    the same, then compile every device config
# The entity lists are what Home Assistant sees of each device. A refactor must not change them.
set -u
cd "$(dirname "$0")/.."
ESPHOME="${ESPHOME:-$HOME/.local/bin/esphome-wsl}"
PYTHON="${PYTHON:-$HOME/esphome/venv/bin/python}"
DEVICES="guition-jc1060p470c-esp32-p4 shed-guition-jc1060p470c-esp32-p4 desk-guition-jc1060p470c-esp32-p4
waveshare-esp32-p4-wifi6-touch-lcd-7b desk-waveshare-esp32-p4-wifi6-touch-lcd-7b"
mode="${1:-}"
fail=0

echo "== host tests"
sh tests/run_tests.sh || fail=1

mkdir -p tests/entities
for d in $DEVICES; do
  echo "== $d"
  tmp="$(mktemp)"
  if ! "$ESPHOME" config "$d.yaml" > "$tmp.cfg" 2> "$tmp.err"; then
    echo "config failed:"; tail -20 "$tmp.err"; fail=1; continue
  fi
  "$PYTHON" tools/entities.py < "$tmp.cfg" > "$tmp" || { fail=1; continue; }
  if [ "$mode" = "--update" ]; then
    cp "$tmp" "tests/entities/$d.txt"
    echo "$(wc -l < "$tmp") entities written"
  elif ! diff -u "tests/entities/$d.txt" "$tmp"; then
    echo "entities changed (tools/check.sh --update if intended)"; fail=1
  else
    echo "$(wc -l < "$tmp") entities, unchanged"
  fi
  rm -f "$tmp" "$tmp.cfg" "$tmp.err"
done

if [ "$mode" = "--build" ] && [ "$fail" = 0 ]; then
  for d in $DEVICES; do
    echo "== build $d"
    "$ESPHOME" compile "$d.yaml" > "/tmp/build-$d.log" 2>&1 || { echo "build failed, see /tmp/build-$d.log"; tail -15 "/tmp/build-$d.log"; fail=1; }
  done
fi

[ "$fail" = 0 ] && echo "OK" || echo "FAILED"
exit "$fail"
