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
  upstream sentinel strings; neither send overload is used by firmware.
- The source includes its adjacent header instead of a global `<ELMo.h>`.

The firmware API additionally provides typed `begin(Command, now, timeout)`
and nonblocking `poll(now)`, owning the same `ObdTransaction.h` engine used
for incremental DTC receive on main. It accepts only the closed policy,
uses a fixed 1025-byte buffer and consumes at most 64 bytes per poll. The
1024-byte limit counts all input bytes, including discarded control bytes.
Stale-input removal is bounded to 64 bytes; excess closes without transmitting.
The synchronous firmware wrapper and async DTC path share this one slot.
Generic send refuses to run while the typed slot is occupied; typed begin
refuses during a generic send. This vendored integration depends on the
adjacent project headers and is not a standalone upstream replacement.

The generic legacy library API itself is not a vehicle-command policy.
Firmware never calls automatic `initialize()` (which would bypass the LAB
identity gate). It accesses `elm.client` for connection lifecycle only.

Local tests run the actual ELMo source against Arduino/WiFiClient host doubles.
They do not establish ESP32 timing, real adapter compatibility or vehicle safety.
