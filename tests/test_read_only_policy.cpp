#include "ReadOnlyPolicy.h"
#include <cassert>
#include <cstring>
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
}
