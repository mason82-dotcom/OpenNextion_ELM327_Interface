#pragma once
#include <cstdint>

// Compile-time closed allowlist. Do not add generic "send raw" or user-entered
// commands to the vehicle interface. A future write-mode is a separate firmware.
namespace obd_policy {
enum class Command : std::uint8_t {
  AdapterReset, AdapterEchoOff, AdapterLinesOff, AdapterSpacesOff,
  AdapterHeadersOff, AdapterAutoProtocol, AdapterProtocolId, SupportedPids00,
  Rpm, Coolant, Speed, StoredDtcs, PendingDtcs, PermanentDtcs,
  Load,
  Throttle,
  Maf,
  Map,
  Intake,
  Stft1,
  Ltft1,
  Stft2,
  Ltft2,
  EcuVoltage,
  LambdaTarget,
  Baro,
  Readiness,
  SupportedPids20,
  SupportedPids40,
  MilDistance,
  Warmups,
  ClearDistance,
  FreezeDtc,
  MisfireCyl1,
  MisfireCyl2,
  MisfireCyl3,
  MisfireCyl4,
  MisfireCyl5,
  MisfireCyl6,
  CommandCount  // sentinel, not a command
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
    case Command::Load:
    case Command::Throttle:
    case Command::Maf:
    case Command::Map:
    case Command::Intake:
    case Command::Stft1:
    case Command::Ltft1:
    case Command::Stft2:
    case Command::Ltft2:
    case Command::EcuVoltage:
    case Command::LambdaTarget:
    case Command::Baro:
    case Command::Readiness:
    case Command::SupportedPids20:
    case Command::SupportedPids40:
    case Command::MilDistance:
    case Command::Warmups:
    case Command::ClearDistance:
    case Command::FreezeDtc:
    case Command::MisfireCyl1:
    case Command::MisfireCyl2:
    case Command::MisfireCyl3:
    case Command::MisfireCyl4:
    case Command::MisfireCyl5:
    case Command::MisfireCyl6:
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
    case Command::AdapterProtocolId: return "ATDPN";
    case Command::SupportedPids00: return "0100";
    case Command::Rpm: return "010C";
    case Command::Coolant: return "0105";
    case Command::Speed: return "010D";
    case Command::Load: return "0104";
    case Command::Throttle: return "0111";
    case Command::Maf: return "0110";
    case Command::Map: return "010B";
    case Command::Intake: return "010F";
    case Command::Stft1: return "0106";
    case Command::Ltft1: return "0107";
    case Command::Stft2: return "0108";
    case Command::Ltft2: return "0109";
    case Command::EcuVoltage: return "0142";
    case Command::LambdaTarget: return "0144";
    case Command::Baro: return "0133";
    case Command::Readiness: return "0101";
    case Command::SupportedPids20: return "0120";
    case Command::SupportedPids40: return "0140";
    case Command::MilDistance: return "0121";
    case Command::Warmups: return "0130";
    case Command::ClearDistance: return "0131";
    case Command::FreezeDtc: return "020200";
    case Command::MisfireCyl1: return "06A2";
    case Command::MisfireCyl2: return "06A3";
    case Command::MisfireCyl3: return "06A4";
    case Command::MisfireCyl4: return "06A5";
    case Command::MisfireCyl5: return "06A6";
    case Command::MisfireCyl6: return "06A7";
    case Command::StoredDtcs: return "03";
    case Command::PendingDtcs: return "07";
    case Command::PermanentDtcs: return "0A";
    default: return nullptr;
  }
}

// Compile-time audit of every wire string. Adapter commands must be one of
// the exact AT setup strings; vehicle reads must be uppercase hex in one of
// the read-only service shapes: 01PP, 020200 (freeze frame 0), 03/07/0A,
// 06MM. Anything else (04/08/14, UDS 1x/2x/3x, ATSH, ATMA, CR/LF) fails.
constexpr bool sameText(const char *a, const char *b) {
  while (*a && *a == *b) { ++a; ++b; }
  return *a == *b;
}
constexpr std::uint8_t textLength(const char *s) {
  std::uint8_t n = 0;
  while (s[n] && n < 16) ++n;
  return n;
}
constexpr bool upperHex(const char *s) {
  for (; *s; ++s)
    if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'F'))) return false;
  return true;
}
constexpr bool allowedAdapterWire(const char *w) {
  constexpr const char *setup[] = {
      "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATDPN"};
  for (const char *a : setup)
    if (sameText(w, a)) return true;
  return false;
}
constexpr bool allowedVehicleWire(const char *w) {
  if (!w || !upperHex(w)) return false;
  const std::uint8_t n = textLength(w);
  if (n < 2) return false;
  if (w[0] != '0') return false;
  switch (w[1]) {
    case '1': return n == 4;                 // Mode 01 current data
    case '2': return sameText(w, "020200");  // Mode 02 frame 00, PID 02 only
    case '3': case '7': case 'A': return n == 2;  // DTC lists
    case '6': return n == 4;                 // Mode 06 on-board monitor
    default: return false;
  }
}
constexpr bool allowedWire(Command command) {
  const char *w = wireCommand(command);
  if (!w) return false;
  return isVehicleRead(command) ? allowedVehicleWire(w)
                                : allowedAdapterWire(w);
}
constexpr bool everyCommandAllowed() {
  for (unsigned i = 0; i < static_cast<unsigned>(Command::CommandCount); ++i)
    if (!allowedWire(static_cast<Command>(i))) return false;
  return true;
}
static_assert(everyCommandAllowed(),
              "ReadOnlyPolicy: a wire command is outside the read-only shapes");
static_assert(wireCommand(Command::CommandCount) == nullptr,
              "ReadOnlyPolicy: sentinel must not map to a wire command");
} // namespace obd_policy
