#pragma once
// MicroSD CSV log format (v0.3). Pure C++17: no Arduino types, no heap, so
// formatting, file naming and session numbering are tested on Linux.
// There is no RTC time yet: rows carry millis() since boot; the session
// number orders files across power cycles.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace obd_log {
static constexpr std::uint32_t ROTATE_BYTES = 4UL * 1024UL * 1024UL;
static constexpr std::uint32_t FLUSH_INTERVAL_MS = 1000;
static constexpr std::uint16_t MAX_SESSION = 9999;
static constexpr std::uint8_t MAX_PART = 99;
static constexpr const char *DIRECTORY = "/obd";
static constexpr const char *COLUMNS = "ms,pid,label,value,unit,latency_ms\n";

// Returns the length written, or 0 if the buffer is too small (never a
// truncated row: a half line would corrupt the CSV).
inline std::size_t finish(int written, std::size_t size) {
  return (written > 0 && static_cast<std::size_t>(written) < size)
      ? static_cast<std::size_t>(written) : 0;
}

inline bool csvSafe(const char *text) {
  if (!text) return false;
  for (; *text; ++text)
    if (*text == ',' || *text == '"' || *text == '\n' || *text == '\r')
      return false;
  return true;
}

inline std::size_t header(char *out, std::size_t size, const char *source,
                          std::uint16_t session, std::uint8_t part) {
  if (!out || size == 0 || !csvSafe(source)) return 0;
  return finish(std::snprintf(out, size,
      "# OpenNextion OBD log v1,source=%s,session=%u,part=%u,time=millis_since_boot\n%s",
      source, static_cast<unsigned>(session), static_cast<unsigned>(part),
      COLUMNS), size);
}

inline std::size_t pidRow(char *out, std::size_t size, std::uint32_t ms,
                          std::uint8_t pid, const char *label, double value,
                          std::uint8_t decimals, const char *unit,
                          std::uint32_t latencyMs) {
  if (!out || size == 0 || !std::isfinite(value) || decimals > 6 ||
      !csvSafe(label) || !csvSafe(unit)) return 0;
  return finish(std::snprintf(out, size, "%lu,01%02X,%s,%.*f,%s,%lu\n",
      static_cast<unsigned long>(ms), static_cast<unsigned>(pid), label,
      static_cast<int>(decimals), value, unit,
      static_cast<unsigned long>(latencyMs)), size);
}

// mode: "03", "07" or "0A"; code: decoded DTC such as "P0171".
inline std::size_t dtcRow(char *out, std::size_t size, std::uint32_t ms,
                          const char *mode, const char *code) {
  if (!out || size == 0 || !csvSafe(mode) || !csvSafe(code)) return 0;
  return finish(std::snprintf(out, size, "%lu,%s,DTC,%s,,\n",
      static_cast<unsigned long>(ms), mode, code), size);
}

// Event rows start with '#' so CSV tools can skip them as comments.
inline std::size_t eventRow(char *out, std::size_t size, std::uint32_t ms,
                            const char *text) {
  if (!out || size == 0 || !csvSafe(text)) return 0;
  return finish(std::snprintf(out, size, "# %lu %s\n",
      static_cast<unsigned long>(ms), text), size);
}

inline std::size_t fileName(char *out, std::size_t size,
                            std::uint16_t session, std::uint8_t part) {
  if (!out || session == 0 || session > MAX_SESSION ||
      part == 0 || part > MAX_PART) return 0;
  return finish(std::snprintf(out, size, "%s/S%04u_P%02u.csv", DIRECTORY,
      static_cast<unsigned>(session), static_cast<unsigned>(part)), size);
}

// "S0012_P03.csv" (directory part optional) -> 12; anything else -> 0.
inline std::uint16_t sessionOf(const char *name) {
  if (!name) return 0;
  const char *base = name;
  for (const char *p = name; *p; ++p) if (*p == '/') base = p + 1;
  if (base[0] != 'S') return 0;
  unsigned session = 0;
  for (int i = 1; i <= 4; ++i) {
    if (base[i] < '0' || base[i] > '9') return 0;
    session = session * 10 + static_cast<unsigned>(base[i] - '0');
  }
  if (base[5] != '_' || base[6] != 'P' || base[7] < '0' || base[7] > '9' ||
      base[8] < '0' || base[8] > '9') return 0;
  const char *ext = base + 9;
  if (ext[0] != '.' || ext[1] != 'c' || ext[2] != 's' || ext[3] != 'v' ||
      ext[4] != '\0') return 0;
  return session <= MAX_SESSION ? static_cast<std::uint16_t>(session) : 0;
}

// Next session after the highest one found; 0 when the numbers ran out
// (never overwrite an older session).
inline std::uint16_t nextSession(std::uint16_t highestFound) {
  return highestFound >= MAX_SESSION ? 0
      : static_cast<std::uint16_t>(highestFound + 1);
}

inline bool shouldRotate(std::uint32_t bytesInFile, std::size_t nextRow) {
  return bytesInFile > 0 && bytesInFile + nextRow > ROTATE_BYTES;
}
} // namespace obd_log
