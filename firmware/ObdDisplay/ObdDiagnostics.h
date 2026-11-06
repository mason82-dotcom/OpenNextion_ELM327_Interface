#pragma once
// Conservative, bounded native subset of ELM327 v1.0.9 Readiness/Mode06/DTC
// diagnostics. Only unambiguous headerless byte-aligned replies are decoded.
// All data is static, no heap allocations or ECU commands.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "ObdResponseParser.h"

namespace obd_diag {
struct Readiness {
  bool mil = false, compression = false;
  std::uint16_t dtcs = 0;
  std::uint8_t ecus = 0;
  std::uint8_t supportedContinuous = 0, openContinuous = 0;
  std::uint8_t supportedOther = 0, openOther = 0;
  int incompleteCount() const {
    int count = 0;
    for (int i=0;i<3;++i)
      if ((supportedContinuous & openContinuous & (1u<<i))!=0) ++count;
    for (int i=0;i<8;++i)
      if ((supportedOther & openOther & (1u<<i))!=0) ++count;
    return count;
  }
};
inline bool parseReadiness(const char *text, Readiness &out) {
  out = {};
  obd_response::forEachLineFrame(text, [&](const obd_response::Frame &f) {
    if (f.size < 6 || f.data[0]!=0x41 || f.data[1]!=0x01) return;
    if (out.ecus == 255) return;
    ++out.ecus;
    out.mil |= (f.data[2] & 0x80) != 0;
    out.dtcs += f.data[2] & 0x7F;
    out.compression |= (f.data[3] & 0x08) != 0;
    out.supportedContinuous |= f.data[3] & 7;
    out.openContinuous |= (f.data[3] >> 4) & 7;
    out.supportedOther |= f.data[4];
    out.openOther |= f.data[5] & f.data[4];
  });
  return out.ecus != 0;
}
inline const char *monitorName(std::size_t i, bool compression) {
  static constexpr const char *common[] =
      {"Aussetzer","Kraftstoff","Komponenten"};
  static constexpr const char *spark[] = {
      "Katalysator","Beheizter Kat","EVAP","Sekundaerluft",
      "Klimaanlage","Lambdasonden","Lambda-Heizung","AGR"};
  static constexpr const char *diesel[] = {
      "NMHC-Kat","NOx","Reserviert","Ladedruck",
      "Reserviert","Abgassensor","Partikelfilter","AGR/VVT"};
  if (i < 3) return common[i];
  if (i < 11) return compression ? diesel[i-3] : spark[i-3];
  return "";
}
enum class DtcEncoding : std::uint8_t { Can, Legacy, Unknown };
// Map an ELM327 ATDPN protocol id (e.g. "6", "A6", " 9 ", "A") to the DTC wire
// encoding. Mirrors the Mercedes-OBD2 Android app's isCanProtocol() so both
// code bases agree on which protocols decode as CAN:
//   - an "A" (auto) prefix is dropped only when a protocol digit follows it;
//   - '6'..'9' and 'A'..'C' are ISO 15765-4 / user-defined CAN  -> Can;
//   - '1'..'5' are the J1850 / ISO 9141 / KWP legacy buses       -> Legacy;
//   - anything else (incl. "0"/"A0" = no protocol negotiated yet) -> Unknown.
inline DtcEncoding encodingFromProtocol(const char *atdpn) {
  if (!atdpn) return DtcEncoding::Unknown;
  char buf[8];
  std::size_t n = 0;
  for (const char *p = atdpn; *p; ++p) {
    const char c = *p;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      if (n == 0) continue;  // skip leading whitespace
      break;                 // stop at trailing whitespace
    }
    if (n >= sizeof buf - 1) return DtcEncoding::Unknown;  // too long to be a protocol id
    buf[n++] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
  }
  buf[n] = '\0';
  const char *code = buf;
  if (n > 1 && code[0] == 'A') { ++code; --n; }  // strip auto-prefix, keep a bare "A"
  if (n != 1) return DtcEncoding::Unknown;
  const char ch = code[0];
  if ((ch >= '6' && ch <= '9') || (ch >= 'A' && ch <= 'C')) return DtcEncoding::Can;
  if (ch >= '1' && ch <= '5') return DtcEncoding::Legacy;
  return DtcEncoding::Unknown;
}
struct Dtc {
  char code[6] = {};
};
struct DtcResult {
  Dtc entries[24] = {};
  std::size_t count = 0;
  bool gotAnswer = false;
  bool malformed = false;
  std::uint8_t ecuResponses = 0;
};
inline void decodeDtc(std::uint8_t a, std::uint8_t b, char out[6]) {
  static constexpr char groups[] = "PCBU";
  static constexpr char digits[] = "0123456789ABCDEF";
  out[0]=groups[(a>>6)&3];
  out[1]=digits[(a>>4)&3];
  out[2]=digits[a&15];
  out[3]=digits[b>>4];
  out[4]=digits[b&15];
  out[5]='\0';
}
inline void addDtc(DtcResult &result, std::uint8_t a, std::uint8_t b) {
  if (a==0 && b==0) return;
  char code[6]; decodeDtc(a,b,code);
  for (std::size_t i=0;i<result.count;++i)
    if (std::strcmp(result.entries[i].code,code)==0) return;
  if (result.count >= 24) {result.malformed=true;return;}
  std::memcpy(result.entries[result.count++].code,code,6);
}
inline DtcResult parseDtcs(const char *text, std::uint8_t responseMode,
                          DtcEncoding encoding) {
  DtcResult result;
  if (responseMode!=0x43 && responseMode!=0x47 && responseMode!=0x4A)
    return result;
  obd_response::forEachLineFrame(text, [&](const obd_response::Frame &f) {
    if (f.size<2 || f.data[0]!=responseMode) return;
    if (encoding==DtcEncoding::Unknown) {
      // Only an unambiguous empty frame can be presented as empty without
      // knowing CAN vs legacy. All other frames remain raw.
      if (f.size==2 && f.data[1]==0) {result.gotAnswer=true; ++result.ecuResponses;}
      return;
    }
    std::size_t start=1, count=0;
    if (encoding==DtcEncoding::Can) {
      count=f.data[1]; start=2;
      if (count>24 || f.size < start+count*2) {result.malformed=true;return;}
    } else {
      // ISO9141/KWP/J1850 legacy frames: exactly three DTC pairs.
      if (f.size!=7) {result.malformed=true;return;}
      count=3;
    }
    result.gotAnswer=true;
    if (result.ecuResponses<255) ++result.ecuResponses;
    for (std::size_t i=0;i<count;++i)
      addDtc(result, f.data[start+i*2], f.data[start+i*2+1]);
  });
  return result;
}
struct Misfire {
  bool hasEwma=false, hasCurrent=false;
  std::uint16_t ewma=0,current=0;
  bool conflict=false;
};
// Mode 06 only: MID A2-A7, TID 0B/0C, UASID 24=raw counts.
// Works on one complete, headerless response message (single line or an
// ISO-TP message reassembled by obd_isotp::parseMisfire). No other scaling IDs.
inline void accumulateMisfire(const std::uint8_t *data, std::size_t size,
                              std::uint8_t mid, Misfire &out) {
  if (!data || size<10 || data[0]!=0x46 || (size-1)%9!=0) return;
  for(std::size_t i=1;i+8<size;i+=9){
    if(data[i]!=mid || data[i+2]!=0x24)continue;
    std::uint16_t value=(std::uint16_t(data[i+3])<<8)|data[i+4];
    if(data[i+1]==0x0B){
      if(out.hasEwma && out.ewma!=value)out.conflict=true;
      out.ewma=value;out.hasEwma=true;
    }
    if(data[i+1]==0x0C){
      if(out.hasCurrent && out.current!=value)out.conflict=true;
      out.current=value;out.hasCurrent=true;
    }
  }
}
// Single-line variant. NOTE: on CAN a Mode 06 reply is >= 10 bytes and thus
// always ISO-TP multiframe; real adapters need obd_isotp::parseMisfire.
inline bool parseMisfire(const char *response,std::uint8_t cylinder,Misfire &out){
  out={};
  if (cylinder < 1 || cylinder > 6) return false;
  const std::uint8_t mid=std::uint8_t(0xA1+cylinder);
  obd_response::forEachLineFrame(response,[&](const obd_response::Frame &f){
    accumulateMisfire(f.data,f.size,mid,out);
  });
  if(out.conflict){out={};return false;}
  return out.hasEwma||out.hasCurrent;
}
inline const char *m272Hint(const char *code) {
  if (!code) return nullptr;
  if (!std::strcmp(code,"P0016") || !std::strcmp(code,"P0017") ||
      !std::strcmp(code,"P0018") || !std::strcmp(code,"P0019"))
    return "M272: Versteller, Sensor/Stecker, Steuerzeiten und Steuertrieb pruefen.";
  if (!std::strcmp(code,"P0171") || !std::strcmp(code,"P0174") ||
      !std::strcmp(code,"P2187") || !std::strcmp(code,"P2189"))
    return "M272: Falschluft, KGE, MAF und Kraftstoffdruck pruefen.";
  if (!std::strcmp(code,"P0300") ||
      (!std::strncmp(code,"P030",4) && code[4]>='1' && code[4]<='6' && code[5]=='\0'))
    return "M272: Zuendkerze, Spule, Einspritzventil, Gemisch pruefen.";
  if (!std::strcmp(code,"P2004") || !std::strcmp(code,"P2005") ||
      !std::strcmp(code,"P2006") || !std::strcmp(code,"P2007"))
    return "M272: Saugrohrklappen, Hebel, Unterdruck pruefen.";
  if (!std::strcmp(code,"P0128")) return "M272: Thermostat und Warmlauf pruefen.";
  if (!std::strcmp(code,"P0101")) return "M272: Luftmassenmesser/Falschluft plausibilisieren.";
  return nullptr;
}
} // namespace obd_diag
