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
  TimingAdvance, EngineRuntime, EvapPurge, FuelLevel, CatalystB1, CatalystB2,
  SupportedMids00, SupportedMids20, SupportedMids40,
  SupportedMids60, SupportedMids80, SupportedMidsA0,
  VehicleInfoSupport, VehicleVin,
  FreezeSupport, FreezeLoad, FreezeCoolant, FreezeRpm, FreezeSpeed,
  FreezeStft1, FreezeLtft1, FreezeStft2, FreezeLtft2,
  FreezeMaf, FreezeMap, FreezeIntake, FreezeThrottle, FreezeTiming,
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
    case Command::TimingAdvance: case Command::EngineRuntime:
    case Command::EvapPurge: case Command::FuelLevel:
    case Command::CatalystB1: case Command::CatalystB2:
    case Command::SupportedMids00: case Command::SupportedMids20:
    case Command::SupportedMids40: case Command::SupportedMids60:
    case Command::SupportedMids80: case Command::SupportedMidsA0:
    case Command::VehicleInfoSupport: case Command::VehicleVin:
    case Command::FreezeSupport: case Command::FreezeLoad:
    case Command::FreezeCoolant: case Command::FreezeRpm: case Command::FreezeSpeed:
    case Command::FreezeStft1: case Command::FreezeLtft1:
    case Command::FreezeStft2: case Command::FreezeLtft2:
    case Command::FreezeMaf: case Command::FreezeMap: case Command::FreezeIntake:
    case Command::FreezeThrottle: case Command::FreezeTiming:
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
    case Command::TimingAdvance: return "010E";
    case Command::EngineRuntime: return "011F";
    case Command::EvapPurge: return "012E";
    case Command::FuelLevel: return "012F";
    case Command::CatalystB1: return "013C";
    case Command::CatalystB2: return "013D";
    case Command::SupportedMids00: return "0600";
    case Command::SupportedMids20: return "0620";
    case Command::SupportedMids40: return "0640";
    case Command::SupportedMids60: return "0660";
    case Command::SupportedMids80: return "0680";
    case Command::SupportedMidsA0: return "06A0";
    case Command::VehicleInfoSupport: return "0900";
    case Command::VehicleVin: return "0902";
    case Command::FreezeSupport: return "020000";
    case Command::FreezeLoad: return "020400";
    case Command::FreezeCoolant: return "020500";
    case Command::FreezeRpm: return "020C00";
    case Command::FreezeSpeed: return "020D00";
    case Command::FreezeStft1: return "020600";
    case Command::FreezeLtft1: return "020700";
    case Command::FreezeStft2: return "020800";
    case Command::FreezeLtft2: return "020900";
    case Command::FreezeMaf: return "021000";
    case Command::FreezeMap: return "020B00";
    case Command::FreezeIntake: return "020F00";
    case Command::FreezeThrottle: return "021100";
    case Command::FreezeTiming: return "020E00";
    default: return nullptr;
  }
}

// Compile-time audit of every wire string. Adapter commands must be one of
// the exact AT setup strings; vehicle reads must be uppercase hex in one of
// the read-only service shapes: 01PP, listed 02PP00 (freeze frame 0),
// 03/07/0A, 06MM, 0900/0902. Other services, frames and raw commands fail.
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
    case '2': {
      constexpr const char *freeze[] = {
          "020000", "020200", "020400", "020500", "020C00", "020D00",
          "020600", "020700", "020800", "020900", "021000", "020B00",
          "020F00", "021100", "020E00"};
      for (const char *f : freeze) if (sameText(w, f)) return true;
      return false;
    }
    case '3': case '7': case 'A': return n == 2;  // DTC lists
    case '6': return n == 4;                 // Mode 06 on-board monitor
    case '9': return sameText(w, "0900") || sameText(w, "0902");
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
