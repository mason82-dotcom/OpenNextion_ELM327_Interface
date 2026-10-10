#!/usr/bin/env bash
# Safe read-only preflight for the OpenNextion ELM327 firmware on Pi 5.
set -euo pipefail
export PATH="$HOME/.local/bin:$PATH"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
printf '\n== OpenNextion ELM327 Pi build preflight ==\n'
printf 'Host: '; hostname
printf 'Architecture: '; uname -m
printf 'Kernel: '; uname -sr
printf 'Project: %s\n' "$ROOT"
if [[ -f "$ROOT/firmware/ObdDisplay/Config.h" ]]; then
  printf 'Connected Config.h exists locally. Offline build will refuse it.\n'
else
  printf 'Offline mode: Config.h missing, no WiFi/OBD at runtime.\n'
fi
if ! command -v arduino-cli >/dev/null; then
  printf 'Arduino CLI missing: bash scripts/pi/install-arduino-cli.sh\n'
  exit 2
fi
arduino-cli version
printf '\n== Installed board cores ==\n'
arduino-cli core list
printf '\n== Relevant display libraries ==\n'
arduino-cli lib list | grep -Ei '(^Name|GFX Library for Arduino|Adafruit CST8XX|Adafruit PCF8574|^lvgl)' || true
printf '\n== Disk free ==\n'
df -h "$HOME" | tail -n 1
printf '\nRead-only preflight finished: nothing was flashed.\n'
