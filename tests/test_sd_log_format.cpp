#include "SdLogFormat.h"
#include "ObdPidCatalog.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <initializer_list>
using namespace obd_log;

int main() {
  char buf[160];
  // Header: source marker, session/part, column line.
  std::size_t n = header(buf, sizeof buf, "LAB_SIMULATED", 12, 1);
  assert(n == std::strlen(buf));
  assert(std::strstr(buf, "source=LAB_SIMULATED,session=12,part=1"));
  assert(std::strstr(buf, "\nms,pid,label,value,unit,latency_ms\n"));
  assert(header(buf, sizeof buf, "BAD,SOURCE", 1, 1) == 0);

  // PID row matches the existing USB "CSV," serial format minus the prefix.
  n = pidRow(buf, sizeof buf, 123456, 0x0C, "Drehzahl", 1234.0, 0, "rpm", 41);
  assert(n && std::strcmp(buf, "123456,010C,Drehzahl,1234,rpm,41\n") == 0);
  pidRow(buf, sizeof buf, 7, 0x42, "ECU-Spannung", 13.5, 3, "V", 50);
  assert(std::strcmp(buf, "7,0142,ECU-Spannung,13.500,V,50\n") == 0);
  pidRow(buf, sizeof buf, 7, 0x07, "LTFT B1", 7.8125, 1, "%", 9);
  assert(std::strcmp(buf, "7,0107,LTFT B1,7.8,%,9\n") == 0);
  assert(pidRow(buf, sizeof buf, 1, 0x0C, "Drehzahl", NAN, 0, "rpm", 1) == 0);
  assert(pidRow(buf, sizeof buf, 1, 0x0C, "a,b", 1.0, 0, "rpm", 1) == 0);
  assert(pidRow(buf, 10, 123456, 0x0C, "Drehzahl", 1234.0, 0, "rpm", 41) == 0);
  // Every catalog title and unit is CSV-safe, so no live row is dropped.
  for (const auto &item : obd_pids::catalog)
    assert(csvSafe(item.title) && csvSafe(item.unit));

  assert(dtcRow(buf, sizeof buf, 99, "03", "P0171"));
  assert(std::strcmp(buf, "99,03,DTC,P0171,,\n") == 0);
  assert(eventRow(buf, sizeof buf, 5, "adapter connected CAN=1"));
  assert(std::strcmp(buf, "# 5 adapter connected CAN=1\n") == 0);
  assert(eventRow(buf, sizeof buf, 5, "bad\nline") == 0);

  // File names and session discovery.
  assert(fileName(buf, sizeof buf, 12, 3) && std::strcmp(buf, "/obd/S0012_P03.csv") == 0);
  assert(fileName(buf, sizeof buf, 0, 1) == 0);
  assert(fileName(buf, sizeof buf, 10000, 1) == 0);
  assert(fileName(buf, sizeof buf, 1, 100) == 0);
  assert(sessionOf("S0012_P03.csv") == 12);
  assert(sessionOf("/obd/S9999_P01.csv") == 9999);
  for (const char *other : {"S12_P03.csv", "S0012_P03.txt", "X0012_P03.csv",
                            "S0012-P03.csv", "S0012_P03.csv.bak", "", "S00a2_P01.csv"})
    assert(sessionOf(other) == 0);
  assert(nextSession(0) == 1 && nextSession(41) == 42);
  assert(nextSession(MAX_SESSION) == 0);  // never wrap onto old sessions

  assert(!shouldRotate(0, 64));
  assert(!shouldRotate(ROTATE_BYTES - 64, 64));
  assert(shouldRotate(ROTATE_BYTES - 63, 64));
  return 0;
}
