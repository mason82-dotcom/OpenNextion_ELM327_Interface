// Cross-ported DTC decode vectors from the Mercedes-OBD2 Android app
// (repo mason82-dotcom/ELM327, obdcore/.../ObdParserDtcTest.java), driven
// against this firmware's decoder (obd_isotp::parseDtcs + obd_diag).
//
// The Android app was exercised on real hardware (OnePlus 13R, ELM327 WiFi,
// Mercedes C350 W204 M272); these vectors encode the real ELM327 reply shapes
// it validated: CAN count byte, ISO-TP indexed multiframe ("00A" / "0:" / "1:"),
// padding bytes, the "43"-in-data trap, the SEARCHING... prefix, legacy 7-byte
// frames, multiple ECU blocks, and the B/C/U DTC families.
//
// Deliberate divergence from the Android port: this firmware fixes the wire
// encoding from the negotiated protocol (ATDPN) via encodingFromProtocol(),
// never by guessing CAN vs legacy from the DTC bytes. The byte-level vectors
// are therefore driven with the explicit encoding the protocol step would set.
#include "ObdIsoTp.h"
#include <cassert>
#include <string>
#include <vector>

using obd_diag::DtcEncoding;

static std::vector<std::string> codes(const char *raw, std::uint8_t mode,
                                      DtcEncoding enc, bool expectClean = true) {
  const obd_diag::DtcResult r = obd_isotp::parseDtcs(raw, mode, enc);
  if (expectClean) {
    assert(!r.malformed && "decoder flagged a verified vector as malformed");
  }
  std::vector<std::string> out;
  for (std::size_t i = 0; i < r.count; ++i) out.emplace_back(r.entries[i].code);
  return out;
}

static std::vector<std::string> L(std::initializer_list<const char *> codes) {
  return std::vector<std::string>(codes.begin(), codes.end());
}

int main() {
  // ---------- CAN (ISO 15765-4) ----------
  assert(codes("43010171", 0x43, DtcEncoding::Can) == L({"P0171"}));
  assert(codes("4302017101 74", 0x43, DtcEncoding::Can) == L({"P0171", "P0174"}));
  // "43" inside the data must not be mistaken for a frame marker.
  assert(codes("43010043", 0x43, DtcEncoding::Can) == L({"P0043"}));
  // Count byte 0 -> answered, no codes.
  assert(codes("4300", 0x43, DtcEncoding::Can).empty());
  // Padding after the counted payload is ignored.
  assert(codes("43010171AAAAAA", 0x43, DtcEncoding::Can) == L({"P0171"}));
  // ISO-TP indexed multiframe; last frame carries CAN padding that is dropped.
  assert(codes("00A\r0:43040171017403\r1:00C1230000CCCC\r\r>", 0x43,
               DtcEncoding::Can) == L({"P0171", "P0174", "P0300", "U0123"}));
  // Two ECU blocks, duplicate code removed.
  assert(codes("43010171\r4302017101A0", 0x43, DtcEncoding::Can) ==
         L({"P0171", "P01A0"}));
  // Pending (07 -> 0x47) and permanent (0A -> 0x4A).
  assert(codes("47010420", 0x47, DtcEncoding::Can) == L({"P0420"}));
  assert(codes("4A010420", 0x4A, DtcEncoding::Can) == L({"P0420"}));
  // SEARCHING... prefix line is ignored.
  assert(codes("SEARCHING...\r43 01 01 71\r", 0x43, DtcEncoding::Can) ==
         L({"P0171"}));
  // B / C / U families via an indexed multiframe.
  assert(codes("008\r0:430352348001\r1:C100CCCCCCCCCC", 0x43,
               DtcEncoding::Can) == L({"C1234", "B0001", "U0100"}));

  // ---------- Legacy (J1850 / ISO 9141 / KWP) ----------
  assert(codes("43017100000000", 0x43, DtcEncoding::Legacy) == L({"P0171"}));
  assert(codes("43 01 71 01 74 03 00", 0x43, DtcEncoding::Legacy) ==
         L({"P0171", "P0174", "P0300"}));
  assert(codes("43 01 71 01 74 03 00\r43 04 20 00 00 00 00\r", 0x43,
               DtcEncoding::Legacy) == L({"P0171", "P0174", "P0300", "P0420"}));
  // "43" inside the data of a legacy frame is a code, not a split point.
  assert(codes("43 01 71 00 43 00 00", 0x43, DtcEncoding::Legacy) ==
         L({"P0171", "P0043"}));

  // ---------- Empty / no data ----------
  assert(codes(nullptr, 0x43, DtcEncoding::Can).empty());
  assert(codes("NO DATA", 0x43, DtcEncoding::Can, /*expectClean=*/false).empty());

  // ---------- Protocol -> encoding (ATDPN), mirrors isCanProtocol ----------
  using obd_diag::encodingFromProtocol;
  assert(encodingFromProtocol("6") == DtcEncoding::Can);
  assert(encodingFromProtocol("A6") == DtcEncoding::Can);
  assert(encodingFromProtocol(" 9 ") == DtcEncoding::Can);
  assert(encodingFromProtocol("A") == DtcEncoding::Can);   // user-defined CAN
  assert(encodingFromProtocol("C") == DtcEncoding::Can);   // user CAN 11-bit
  assert(encodingFromProtocol("A3") == DtcEncoding::Legacy);
  assert(encodingFromProtocol("5") == DtcEncoding::Legacy);
  assert(encodingFromProtocol("0") == DtcEncoding::Unknown);
  assert(encodingFromProtocol("A0") == DtcEncoding::Unknown);
  assert(encodingFromProtocol("?") == DtcEncoding::Unknown);
  assert(encodingFromProtocol(nullptr) == DtcEncoding::Unknown);

  return 0;
}
