#include "ReadOnlyPolicy.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace obd_policy;
static_assert(isVehicleRead(Command::Rpm), "RPM must be read-only");
static_assert(isVehicleRead(Command::StoredDtcs), "Stored DTC read allowed");
static_assert(isVehicleRead(Command::PermanentDtcs), "Permanent DTC read allowed");
static_assert(!isVehicleRead(Command::AdapterReset), "ATZ isn't an ECU read");
static_assert(wireCommand(static_cast<Command>(255)) == nullptr, "Unknown commands fail closed");
int main() {
  const Command allowed[] = {
      Command::AdapterReset, Command::AdapterEchoOff, Command::AdapterLinesOff,
      Command::AdapterSpacesOff, Command::AdapterHeadersOff, Command::AdapterAutoProtocol,
      Command::AdapterProtocolId,
      Command::SupportedPids00, Command::Rpm, Command::Coolant,
      Command::Speed, Command::StoredDtcs, Command::PendingDtcs, Command::PermanentDtcs};
  for (const auto c : allowed) {
    const char *wire = wireCommand(c);
    assert(wire && *wire);
    assert(std::strchr(wire, '\r') == nullptr);
    assert(std::strchr(wire, '\n') == nullptr);
    if (isVehicleRead(c)) {
      assert(std::strcmp(wire, "04") != 0 && std::strcmp(wire, "08") != 0);
      assert(wire[0] != '2' && wire[0] != '3'); // no UDS security/write/routines
      assert(std::strncmp(wire, "AT", 2) != 0);
    } else {
      assert(std::strncmp(wire, "AT", 2) == 0);
    }
  }
  assert(wireCommand(static_cast<Command>(253)) == nullptr);
  // Every enum value passes the compile-time shape audit.
  for (unsigned i = 0; i < static_cast<unsigned>(Command::CommandCount); ++i)
    assert(allowedWire(static_cast<Command>(i)));
  // The audit itself rejects writes, clears, UDS and adapter reconfiguration.
  for (const char *bad : {"04", "08", "14", "2E", "27", "31", "3E", "10",
                          "0104X", "010", "01000", "0202", "020201", "0300",
                          "06A", "0a", "010c", "0100\r", "", "AT", "ATSH7E0",
                          "ATMA", "ATCRA7E8", "ATZ ", "ATWS", "ATSP6", "ATD"})
    assert(!allowedVehicleWire(bad) && !allowedAdapterWire(bad));
  for (const char *good : {"0100", "010C", "020200", "03", "07", "0A", "06A2"})
    assert(allowedVehicleWire(good) && !allowedAdapterWire(good));
  for (const char *good : {"ATZ", "ATE0", "ATSP0", "ATDPN"})
    assert(allowedAdapterWire(good) && !allowedVehicleWire(good));
}
