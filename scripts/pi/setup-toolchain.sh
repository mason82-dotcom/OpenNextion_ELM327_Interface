#!/usr/bin/env bash
# Explicit opt-in: install pinned Arduino build prerequisites in user profile.
# No sudo, no board flashing, no vehicle communication.
set -euo pipefail
export PATH="$HOME/.local/bin:$PATH"
if [[ "$(uname -s)" != "Linux" || ! "$(uname -m)" =~ ^(aarch64|arm64)$ ]]; then
  printf 'Expected Raspberry Pi Linux aarch64/arm64; got %s.\n' "$(uname -m)" >&2
  exit 2
fi
if ! command -v arduino-cli >/dev/null 2>&1; then
  printf 'arduino-cli missing. Run bash scripts/pi/install-arduino-cli.sh\n' >&2
  exit 2
fi
printf 'Free space: '; df -h "$HOME" | tail -n 1
printf 'Installing pinned ESP32 core and display libraries (large downloads).\n'
ESP32_URL='https://espressif.github.io/arduino-esp32/package_esp32_index.json'
arduino-cli core update-index --additional-urls "$ESP32_URL"
arduino-cli core install 'esp32:esp32@3.3.2' --additional-urls "$ESP32_URL"
arduino-cli lib install 'GFX Library for Arduino@1.6.9'
arduino-cli lib install 'Adafruit CST8XX Library@1.1.1'
arduino-cli lib install 'Adafruit PCF8574@1.1.2'
arduino-cli lib install 'lvgl@8.3.11'
printf 'Toolchain installation finished. Build: bash scripts/pi/build.sh offline\n'
