#pragma once
#include <cstdint>

// Compile-time closed allowlist. Do not add generic "send raw" or user-entered
// commands to the vehicle interface. A future write-mode is a separate firmware.
namespace obd_policy {
enum class Command : std::uint8_t {
  AdapterReset, AdapterEchoOff, AdapterLinesOff, AdapterSpacesOff,
  AdapterHeadersOff, AdapterAutoProtocol, SupportedPids00,
  Rpm, Coolant, Speed, StoredDtcs, PendingDtcs, PermanentDtcs
};

constexpr bool isVehicleRead(Command command) {
  switch (command) {
    case Command::SupportedPids00:
    case Command::Rpm:
    case Command::Coolant:
    case Command::Speed:
    case Command::StoredDtcs:
    case Command::PendingDtcs:
    case Command::PermanentDtcs:
      return true;
    default:
      return false;
  }
}

// Every returned string is static, auditable and narrowly scoped.
// The AT commands configure the *adapter*, not the vehicle ECU.
constexpr const char* wireCommand(Command command) {
  switch (command) {
    case Command::AdapterReset: return "ATZ";
    case Command::AdapterEchoOff: return "ATE0";
    case Command::AdapterLinesOff: return "ATL0";
    case Command::AdapterSpacesOff: return "ATS0";
    case Command::AdapterHeadersOff: return "ATH0";
    case Command::AdapterAutoProtocol: return "ATSP0";
    case Command::SupportedPids00: return "0100";
    case Command::Rpm: return "010C";
    case Command::Coolant: return "0105";
    case Command::Speed: return "010D";
    case Command::StoredDtcs: return "03";
    case Command::PendingDtcs: return "07";
    case Command::PermanentDtcs: return "0A";
    default: return nullptr;
  }
}
} // namespace obd_policy
