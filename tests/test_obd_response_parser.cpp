#include "ObdResponseParser.h"
#include <cassert>
#include <cstdint>
using namespace obd_response;
int main() {
  std::uint8_t bytes[4] = {};
  std::uint32_t mask = 0;
  assert(readPid("41 0C 13 48\r", 0x0C, bytes, 2));
  assert(bytes[0] == 0x13 && bytes[1] == 0x48); // 1234 rpm
  assert(readPid("010C\rSEARCHING...\r410C1348\r", 0x0C, bytes, 2));
  assert(bytes[0] == 0x13 && bytes[1] == 0x48);
  assert(readPid("NO DATA\r41 05 82\r", 0x05, bytes, 1));
  assert(bytes[0] == 0x82); // 90 degrees C
  assert(readPid("410D00\n", 0x0D, bytes, 1) && bytes[0] == 0);
  assert(!readPid("NO DATA\n", 0x0C, bytes, 2));
  assert(!readPid("410C1", 0x0C, bytes, 2));
  assert(!readPid("41 0C 13", 0x0C, bytes, 2));
  assert(!readPid("SEARCHING...410C1348", 0x0C, bytes, 2));
  assert(!readPid("7E8 04 41 0C 13 48", 0x0C, bytes, 2));
  assert(!readPid("01 0C\r41 0D 00", 0x0C, bytes, 2));
  assert(!readPid("41 0C 13 \n 48", 0x0C, bytes, 2));
  assert(!readPid("41 0C 1G 48", 0x0C, bytes, 2));
  assert(!readPid("04 41 0C 13 48", 0x0C, bytes, 2));
  assert(!readPid("410C1348", 0x0C, nullptr, 2));
  assert(!readPid(nullptr, 0x0C, bytes, 2));
  assert(supportedPids00("0100\r410008180000\r", mask) && mask == 0x08180000u);
  assert(supportedPids00("41 00 08 00 00 00\r41 00 00 18 00 00", mask));
  assert(mask == 0x08180000u);
  assert(!supportedPids00("NO DATA\n", mask));
  assert(!supportedPids00("7E8 06 41 00 08 18 00 00", mask));
  assert(mask == 0);
  const char broken[] = "41 0C 13 48\nERROR 41 0C FF FF\n";
  assert(readPid(broken, 0x0C, bytes, 2) && bytes[0] == 0x13);
  Frame f;
  assert(!decodeLine("41 00 ABC", 9, f));
  return 0;
}
