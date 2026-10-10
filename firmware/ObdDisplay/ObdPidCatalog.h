#pragma once
// Port of the Android ELM327 v1.0.9 ObdPid formulas and polling priorities.
// Every firmware command is a typed, fixed literal from ReadOnlyPolicy.h.
#include <cstddef>
#include <cstdint>
#include "ReadOnlyPolicy.h"

namespace obd_pids {
enum class Formula : std::uint8_t {
  Rpm, Speed, Temperature, Percent, Pressure, MassAirFlow,
  FuelTrim, Voltage, EquivalentRatio
};
struct Item {
  std::uint8_t pid;
  obd_policy::Command command;
  const char *title;
  const char *unit;
  Formula formula;
  std::uint8_t decimals;
  std::uint16_t intervalMs; // minimum interval, not a promise of sampling rate
};
constexpr Item catalog[] = {
  {0x0C, obd_policy::Command::Rpm, "Drehzahl", "rpm", Formula::Rpm, 0, 500},
  {0x0D, obd_policy::Command::Speed, "Tempo", "km/h", Formula::Speed, 0, 950},
  {0x05, obd_policy::Command::Coolant, "Kuehlmittel", "C", Formula::Temperature, 0, 4500},
  {0x04, obd_policy::Command::Load, "Motorlast", "%", Formula::Percent, 1, 1700},
  {0x11, obd_policy::Command::Throttle, "Drossel", "%", Formula::Percent, 1, 1700},
  {0x10, obd_policy::Command::Maf, "MAF", "g/s", Formula::MassAirFlow, 2, 1100},
  {0x0B, obd_policy::Command::Map, "MAP", "kPa", Formula::Pressure, 0, 1500},
  {0x0F, obd_policy::Command::Intake, "Ansaugluft", "C", Formula::Temperature, 0, 6000},
  {0x06, obd_policy::Command::Stft1, "STFT B1", "%", Formula::FuelTrim, 1, 1600},
  {0x07, obd_policy::Command::Ltft1, "LTFT B1", "%", Formula::FuelTrim, 1, 4500},
  {0x08, obd_policy::Command::Stft2, "STFT B2", "%", Formula::FuelTrim, 1, 1600},
  {0x09, obd_policy::Command::Ltft2, "LTFT B2", "%", Formula::FuelTrim, 1, 4500},
  {0x42, obd_policy::Command::EcuVoltage, "ECU-Spannung", "V", Formula::Voltage, 3, 6000},
  {0x44, obd_policy::Command::LambdaTarget, "Soll-Lambda", "lambda", Formula::EquivalentRatio, 3, 1600},
  {0x33, obd_policy::Command::Baro, "Umgebungsdruck", "kPa", Formula::Pressure, 0, 6500},
};
constexpr std::size_t count = sizeof(catalog)/sizeof(catalog[0]);
constexpr int bytesNeeded(Formula f) {
  switch (f) {
    case Formula::Rpm:
    case Formula::MassAirFlow:
    case Formula::Voltage:
    case Formula::EquivalentRatio: return 2;
    default: return 1;
  }
}
inline bool decode(const Item &item, const std::uint8_t *bytes,
                   std::size_t size, double &out) {
  if (!bytes || size < static_cast<std::size_t>(bytesNeeded(item.formula)))
    return false;
  const double a = bytes[0], b = size > 1 ? bytes[1] : 0;
  switch (item.formula) {
    case Formula::Rpm: out = (256.0*a+b)/4.0; break;
    case Formula::Speed: out = a; break;
    case Formula::Temperature: out = a-40; break;
    case Formula::Percent: out = a*100.0/255.0; break;
    case Formula::Pressure: out = a; break;
    case Formula::MassAirFlow: out = (256*a+b)/100.0; break;
    case Formula::FuelTrim: out = (a-128.0)*100.0/128.0; break;
    case Formula::Voltage: out = (256*a+b)/1000.0; break;
    case Formula::EquivalentRatio: out = (256*a+b)/32768.0; break;
  }
  return true;
}
inline bool supported(std::uint8_t pid, const std::uint32_t masks[3]) {
  if (!masks || pid == 0 || pid > 0x60) return false;
  const unsigned index = (pid-1)/32;
  const unsigned pos = (pid-1)%32;
  return (masks[index] & (std::uint32_t(1) << (31-pos))) != 0;
}
inline int indexOf(std::uint8_t pid) {
  for (std::size_t i=0;i<count;++i) if (catalog[i].pid==pid) return int(i);
  return -1;
}
// Priority reflects Android PollSchedule: faster engine / trim data, slower
// coolant/long-term adaptations. Never send more than one ECU command at once.
class Scheduler {
  std::uint32_t last_[count] = {};
  std::size_t cursor_ = 0;
public:
  void reset() { for (auto &x:last_) x=0; cursor_=0; }
  int choose(std::uint32_t now, const std::uint32_t masks[3]) {
    for (std::size_t n=0;n<count;++n) {
      const std::size_t i=(cursor_+n)%count;
      const Item &item=catalog[i];
      if (!supported(item.pid,masks)) continue;
      const std::uint32_t interval=item.intervalMs;
      if (last_[i] == 0 || static_cast<std::uint32_t>(now-last_[i]) >= interval) {
        last_[i] = now == 0 ? 1 : now;
        cursor_ = (i+1)%count;
        return static_cast<int>(i);
      }
    }
    return -1;
  }
};
} // namespace obd_pids
