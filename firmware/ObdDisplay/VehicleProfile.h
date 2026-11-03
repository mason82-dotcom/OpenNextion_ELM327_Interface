#pragma once
// Compile-time vehicle profile. Pure C++17 (tested on Linux), ASCII only.
// A profile changes PRESENTATION and plausibility checks only: it never adds
// a wire command, never forces an ELM protocol (ATSP0 stays) and never
// enables a write service. Select with -DOBD_VEHICLE_GENERIC for no profile.
#include <cstdint>

namespace obd_vehicle {
enum class ExpectedBus : std::uint8_t { Any, Can };

struct Profile {
  const char *name;          // shown on the INFO page and in the SD log
  std::uint8_t cylinders;    // Mode 06 misfire MIDs A2.. (max 6, allowlist)
  std::uint8_t banks;        // 1 or 2: fuel-trim test needs B2 PIDs if 2
  std::uint8_t bank1Last;    // last cylinder of bank 1 (M272: 1-3 | 4-6)
  std::uint16_t chartMaxRpm; // GRAPH y-axis
  std::uint16_t idleMinRpm;  // plausibility window for the KOER hint
  std::uint16_t idleMaxRpm;
  ExpectedBus bus;           // checked against ATDPN, never forced
  bool m272Hints;            // M272-specific DTC hints in DETAILS
};

#if defined(OBD_VEHICLE_GENERIC)
inline constexpr Profile active{
    "Generisch OBD-II", 6, 2, 3, 7000, 500, 1200, ExpectedBus::Any, false};
#else
// Mercedes-Benz C350 Estate (S204), rear-wheel drive confirmed by owner.
// Type 204.256, M272.961, 3498 cm3, 200 kW; model year 2008.
// Manufacturer reference: docs/M272-S204-DIAGNOSTICS.md.
// Bank 1 = cylinders 1-3, bank 2 = cylinders 4-6.
// Idle window is an application heuristic, not a Mercedes service limit.
inline constexpr Profile active{
    "C350 S204 M272.961 (2008)", 6, 2, 3, 7000, 550, 1000, ExpectedBus::Can,
    true};
#endif

static_assert(active.cylinders >= 1 && active.cylinders <= 6,
              "Mode 06 allowlist covers MID A2-A7 (cylinders 1-6) only");
static_assert(active.banks == 1 || active.banks == 2, "1 or 2 banks");
static_assert(active.bank1Last >= 1 && active.bank1Last <= active.cylinders,
              "bank split outside cylinder count");
static_assert(active.idleMinRpm < active.idleMaxRpm &&
              active.idleMaxRpm < active.chartMaxRpm, "rpm windows");

// ATDPN payload after stripping "A": '6'..'9' = ISO 15765-4 CAN.
inline constexpr bool busMatches(const Profile &p, bool isCan) {
  return p.bus == ExpectedBus::Any || isCan;
}
inline constexpr std::uint8_t bankOf(const Profile &p, std::uint8_t cyl) {
  return cyl <= p.bank1Last ? 1 : 2;
}
inline constexpr bool idlePlausible(const Profile &p, int rpm) {
  return rpm >= p.idleMinRpm && rpm <= p.idleMaxRpm;
}
} // namespace obd_vehicle
