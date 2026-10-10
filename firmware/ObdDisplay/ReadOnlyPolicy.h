#pragma once
#include <cstdint>

// Compile-time closed allowlist. Do not add generic "send raw" or user-entered
// commands to the vehicle interface. A future write-mode is a separate firmware.
namespace obd_policy {
enum class Command : std::uint8_t {
  AdapterReset, AdapterEchoOff, AdapterLinesOff, AdapterSpacesOff,
  AdapterHeadersOff, AdapterAutoProtocol, SupportedPids00,
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
  MisfireSupport,
  MisfireCyl1,
  MisfireCyl2,
  MisfireCyl3,
  MisfireCyl4,
  MisfireCyl5,
  MisfireCyl6
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
    case Command::MisfireSupport:
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
    case Command::MisfireSupport: return "06A0";
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
} // namespace obd_policy
