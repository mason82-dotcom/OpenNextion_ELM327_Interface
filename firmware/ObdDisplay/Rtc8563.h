#pragma once
// BLX8563 / PCF8563 real-time clock (vendor schematic V1.2, U3 on the shared
// I2C bus). Pure C++17 register and calendar logic, tested on Linux; the
// firmware only moves the 7 time registers over Wire. The clock keeps UTC.
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace rtc8563 {
static constexpr std::uint8_t I2C_ADDRESS = 0x51;
static constexpr std::uint8_t REG_SECONDS = 0x02;  // 7 registers: 0x02..0x08
static constexpr std::uint8_t VL_BIT = 0x80;       // oscillator stopped: time lost
// Anything before this is an unset clock, never a real measurement date.
static constexpr std::int64_t MIN_VALID_EPOCH = 1767225600;  // 2026-01-01T00:00:00Z

struct DateTime {
  int year = 2000, month = 1, day = 1, hour = 0, minute = 0, second = 0;
};

inline std::uint8_t toBcd(int value) {
  return static_cast<std::uint8_t>(((value / 10) << 4) | (value % 10));
}
inline int fromBcd(std::uint8_t value) {
  return ((value >> 4) & 0x0F) * 10 + (value & 0x0F);
}

inline bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
inline int daysInMonth(int y, int m) {
  static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && isLeap(y) ? 29 : days[m - 1];
}
inline bool plausible(const DateTime &t) {
  return t.year >= 2000 && t.year <= 2099 && t.month >= 1 && t.month <= 12 &&
         t.day >= 1 && t.day <= daysInMonth(t.year, t.month) &&
         t.hour >= 0 && t.hour <= 23 && t.minute >= 0 && t.minute <= 59 &&
         t.second >= 0 && t.second <= 59;
}

// Howard Hinnant's days_from_civil / civil_from_days (proleptic Gregorian).
inline std::int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}
inline std::int64_t toEpoch(const DateTime &t) {
  return daysFromCivil(t.year, t.month, t.day) * 86400 +
         t.hour * 3600 + t.minute * 60 + t.second;
}
inline DateTime fromEpoch(std::int64_t epoch) {
  std::int64_t days = epoch / 86400;
  std::int64_t secs = epoch % 86400;
  if (secs < 0) { secs += 86400; --days; }
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  DateTime t;
  t.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  t.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  t.year = static_cast<int>(static_cast<std::int64_t>(yoe) + era * 400 + (t.month <= 2));
  t.hour = static_cast<int>(secs / 3600);
  t.minute = static_cast<int>((secs % 3600) / 60);
  t.second = static_cast<int>(secs % 60);
  return t;
}

// regs[0..6] = registers 0x02..0x08. Returns false when the oscillator-stop
// flag is set or the content is not a plausible 20xx date.
inline bool decode(const std::uint8_t regs[7], DateTime &out) {
  if (!regs || (regs[0] & VL_BIT)) return false;
  DateTime t;
  t.second = fromBcd(regs[0] & 0x7F);
  t.minute = fromBcd(regs[1] & 0x7F);
  t.hour = fromBcd(regs[2] & 0x3F);
  t.day = fromBcd(regs[3] & 0x3F);
  t.month = fromBcd(regs[5] & 0x1F);
  t.year = 2000 + fromBcd(regs[6]);
  if (!plausible(t)) return false;
  out = t;
  return true;
}

// Fills registers 0x02..0x08; clears VL (time valid) and the century bit.
inline bool encode(const DateTime &t, std::uint8_t regs[7]) {
  if (!regs || !plausible(t)) return false;
  const int weekday = static_cast<int>(((daysFromCivil(t.year, t.month, t.day) % 7) + 11) % 7);
  regs[0] = toBcd(t.second);
  regs[1] = toBcd(t.minute);
  regs[2] = toBcd(t.hour);
  regs[3] = toBcd(t.day);
  regs[4] = static_cast<std::uint8_t>(weekday);  // 0 = Sunday
  regs[5] = toBcd(t.month);
  regs[6] = toBcd(t.year - 2000);
  return true;
}

// "2026-10-11T10:52:03.123Z"; 0 if the buffer is too small.
inline std::size_t isoUtc(char *out, std::size_t size, std::int64_t epoch,
                          unsigned millis) {
  if (!out || size == 0 || millis > 999) return 0;
  const DateTime t = fromEpoch(epoch);
  const int n = std::snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
      t.year, t.month, t.day, t.hour, t.minute, t.second, millis);
  return (n > 0 && static_cast<std::size_t>(n) < size) ? static_cast<std::size_t>(n) : 0;
}
} // namespace rtc8563
