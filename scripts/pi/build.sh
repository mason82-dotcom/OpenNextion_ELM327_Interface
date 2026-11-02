#!/usr/bin/env bash
# Clean ESP32-S3 firmware compile; NEVER performs upload/flash/vehicle access.
# Required LVGL compile settings must apply globally to library C/C++ units.
set -euo pipefail
export PATH="$HOME/.local/bin:$PATH"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SKETCH="$ROOT/firmware/ObdDisplay"
MODE="${1:-offline}"
case "$MODE" in
  offline)
    if [[ -f "$SKETCH/Config.h" ]]; then
      printf 'REFUSED: Config.h exists. Move it aside before OFFLINE build.\n' >&2
      exit 3
    fi
    ;;
  connected)
    if [[ ! -f "$SKETCH/Config.h" ]]; then
      printf 'REFUSED: connected build needs local Config.h (copy Config.h.example).\n' >&2
      exit 3
    fi
    ;;
  lab)
    if [[ -f "$SKETCH/Config.h" ]]; then
      printf 'REFUSED: lab build must not contain vehicle Config.h.\n' >&2
      exit 3
    fi
    if [[ ! -f "$SKETCH/LabConfig.h" ]]; then
      printf 'REFUSED: lab build needs local LabConfig.h.\n' >&2
      exit 3
    fi
    ;;
  *)
    printf 'Usage: bash scripts/pi/build.sh [offline|connected|lab]\n' >&2
    exit 2
    ;;
esac
if ! command -v arduino-cli >/dev/null 2>&1; then
  printf 'arduino-cli not installed; run bash scripts/pi/install-arduino-cli.sh\n' >&2
  exit 2
fi
if ! arduino-cli core list | grep -Eq '^esp32:esp32[[:space:]]+3[.]3[.]2'; then
  printf 'ESP32 core 3.3.2 missing; run bash scripts/pi/setup-toolchain.sh\n' >&2
  exit 2
fi
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
BUILD="$ROOT/build/firmware-$MODE"
mkdir -p "$ROOT/build/logs"
LOG="$ROOT/build/logs/firmware-$MODE-build.log"
LAB_FLAGS=''
if [[ "$MODE" == "lab" ]]; then
  LAB_FLAGS=' -DOBD_LAB_BUILD=1'
fi
printf 'Compiling %s firmware with global LVGL 8.3.11 flags.\n' "$MODE"
printf 'Artifact directory: %s\nLog: %s\n' "$BUILD" "$LOG"
if arduino-cli compile --clean --jobs 2 \
    --fqbn "$FQBN" \
    --build-property "compiler.c.extra_flags=-DLV_CONF_SKIP -DLV_COLOR_DEPTH=16 -DLV_FONT_MONTSERRAT_20=1 -DLV_FONT_MONTSERRAT_24=1${LAB_FLAGS}" \
    --build-property "compiler.cpp.extra_flags=-DLV_CONF_SKIP -DLV_COLOR_DEPTH=16 -DLV_FONT_MONTSERRAT_20=1 -DLV_FONT_MONTSERRAT_24=1${LAB_FLAGS}" \
    --build-path "$BUILD" \
    "$SKETCH" 2>&1 | tee "$LOG"; then
  printf '\nBUILD OK; NEVER FLASHED. Binaries:\n'
  find "$BUILD" -maxdepth 2 -type f \( -name '*.bin' -o -name '*.elf' \) -print
else
  printf '\nBUILD FAILED; no board was flashed.\n' >&2
  printf 'Log: %s\nRelevant errors:\n' "$LOG" >&2
  grep -m 15 -E 'fatal error:|error:|undefined reference|exit status' "$LOG" || true
  exit 1
fi
