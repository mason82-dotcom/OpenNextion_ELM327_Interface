# ELMo 1.1.2 — local transport integration

Upstream: https://github.com/rudydelorenzo/ELMo
Pinned revision: `59a9df47486b2ebf0c3be80fb05876e8dc8f39bf` (2021-04-25).
Original files: `src/ELMo.h`, `src/ELMo.cpp`, `library.properties`, `LICENSE`.
Copyright Rudy de Lorenzo, MIT; see the included LICENSE.

This is an explicitly modified, vendored copy. Arduino compiles sketch `src/`
recursively, so neither the Pi setup script nor CI installs another ELMo copy.
The relative header include also avoids selecting an unrelated installed version.

Local changes from upstream:

- `send(const char*, String&, uint32_t, void(*)())` supports a caller-managed
  TCP/initialization sequence, per-request millisecond deadlines and an idle
  callback. It sends exactly one command plus CR in one checked write.
- A common bounded receive loop replaces both upstream receive loops. Maximum
  1024 received bytes before the prompt, including discarded control bytes;
  CR/LF normalize to LF, printable ASCII remains, other bytes are discarded.
  Bursts are limited to 64 bytes between callback/yield/deadline checks.
- Status distinguishes timeout, lost connection, oversized reply, short write,
  invalid command and allocation failure. Incomplete streams are closed.
- Empty commands, embedded control characters and commands above 32 characters
  are rejected. A callback cannot reenter a running send; it returns false
  without modifying the outer transaction's reply or status. Not thread-safe.
- `stop()` closes the socket without the upstream implicit `AT WS` command.
- The legacy String-returning API uses the same receive loop. It retains the
  upstream sentinel strings; firmware uses only the bool-returning overload.
- The source includes its adjacent header instead of a global `<ELMo.h>`.

The generic library itself is not a vehicle-command policy. Firmware keeps
`transact(Command, ...)` as its only command entry point and checks
`ReadOnlyPolicy.h` before calling ELMo. It uses `elm.client` solely for socket
lifecycle and bounded stale-input removal. It never calls the legacy automatic
`initialize()` (which sends commands and does not enforce the LAB identity gate).

Local tests run the actual ELMo source against Arduino/WiFiClient host doubles.
They do not establish ESP32 timing, real adapter compatibility or vehicle safety.
