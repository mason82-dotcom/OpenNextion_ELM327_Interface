#include "Rtc8563.h"
#include <cassert>
#include <cstring>
using namespace rtc8563;

int main() {
  assert(toBcd(59) == 0x59 && fromBcd(0x59) == 59 && toBcd(0) == 0);

  // Epoch conversion against known instants.
  assert(daysFromCivil(1970, 1, 1) == 0);
  DateTime t{2026, 10, 11, 10, 52, 3};
  assert(toEpoch(t) == 1791715923);
  const DateTime back = fromEpoch(1791715923);
  assert(back.year == 2026 && back.month == 10 && back.day == 11 &&
         back.hour == 10 && back.minute == 52 && back.second == 3);
  const DateTime leap = fromEpoch(toEpoch({2028, 2, 29, 23, 59, 59}));
  assert(leap.month == 2 && leap.day == 29 && leap.second == 59);
  for (std::int64_t e = 946684800; e < 4102444800; e += 86400 * 37 + 3601)
    assert(toEpoch(fromEpoch(e)) == e);  // 2000..2099 round trip

  // Register round trip, weekday, VL handling.
  std::uint8_t regs[7] = {};
  assert(encode(t, regs));
  assert(regs[0] == 0x03 && regs[1] == 0x52 && regs[2] == 0x10 &&
         regs[3] == 0x11 && regs[5] == 0x10 && regs[6] == 0x26);
  assert(regs[4] == 0);  // 2026-10-11 is a Sunday
  DateTime decoded;
  assert(decode(regs, decoded) && toEpoch(decoded) == toEpoch(t));
  regs[0] |= VL_BIT;  // oscillator stopped (no battery, first power-up)
  assert(!decode(regs, decoded));
  regs[0] &= 0x7F;
  // Ignore unused high bits the chip may return (century, reserved).
  regs[5] |= 0x80; regs[3] |= 0xC0;
  assert(decode(regs, decoded) && decoded.month == 10 && decoded.day == 11);
  // Garbage is rejected, never turned into a date.
  const std::uint8_t garbage[7] = {0x00, 0x00, 0x00, 0x31, 0x00, 0x02, 0x26};
  assert(!decode(garbage, decoded));  // 31 February
  const std::uint8_t zeros[7] = {};
  assert(!decode(zeros, decoded));    // day 0
  assert(!encode({1999, 1, 1, 0, 0, 0}, regs) && !encode({2026, 13, 1, 0, 0, 0}, regs));

  char iso[32];
  assert(isoUtc(iso, sizeof iso, 1791715923, 7));
  assert(std::strcmp(iso, "2026-10-11T10:52:03.007Z") == 0);
  assert(isoUtc(iso, 10, 1791715923, 7) == 0);
  assert(isoUtc(iso, sizeof iso, 1791715923, 1000) == 0);
  assert(MIN_VALID_EPOCH == toEpoch({2026, 1, 1, 0, 0, 0}));
  return 0;
}
