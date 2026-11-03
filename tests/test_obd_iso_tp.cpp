#include "ObdIsoTp.h"
#include <cassert>
#include <cstring>
using obd_diag::DtcEncoding;
int main(){
  // Existing headerless short CAN answer remains compatible.
  auto shortCan=obd_isotp::parseDtcs("43 01 01 71",0x43,DtcEncoding::Can);
  assert(shortCan.gotAnswer&&!shortCan.malformed&&shortCan.count==1);
  assert(std::strcmp(shortCan.entries[0].code,"P0171")==0);
  // Complete ELM formatted 0:/1: indexed transport reply (10 bytes).
  const char *multi="00A\r0:43 04 01 71 03 01 03\r1:02 03 03";
  auto r=obd_isotp::parseDtcs(multi,0x43,DtcEncoding::Can);
  assert(r.gotAnswer&&!r.malformed&&r.count==4&&r.ecuResponses==1);
  assert(std::strcmp(r.entries[0].code,"P0171")==0);
  assert(std::strcmp(r.entries[1].code,"P0301")==0);
  assert(std::strcmp(r.entries[2].code,"P0302")==0);
  assert(std::strcmp(r.entries[3].code,"P0303")==0);
  // Two separate sequential response blocks: ECU IDs are NOT known with ATH0.
  auto multiple=obd_isotp::parseDtcs(
      "004\n0:43 01 01 74\n"
      "00A\n0:43 04 01 71 03 01 03\n1:02 03 03\n"
      "43 01 01 71",0x43,DtcEncoding::Can);
  assert(multiple.gotAnswer&&!multiple.malformed&&multiple.ecuResponses==3);
  assert(multiple.count==5&&std::strcmp(multiple.entries[0].code,"P0174")==0);
  // DTC 0A permanent may also be segmented and should only decode that mode.
  auto permanent=obd_isotp::parseDtcs(
      "004\n0:4A 01 01 71\n",0x4A,DtcEncoding::Can);
  assert(permanent.gotAnswer&&!permanent.malformed&&permanent.count==1);
  assert(std::strcmp(permanent.entries[0].code,"P0171")==0);
  // Neither empty responses nor protocol-unknown frames may generate DTCs.
  auto empty=obd_isotp::parseDtcs("43 00\r43 00",0x43,DtcEncoding::Can);
  assert(empty.gotAnswer&&empty.count==0&&!empty.malformed);
  auto unknown=obd_isotp::parseDtcs(multi,0x43,DtcEncoding::Unknown);
  assert(!unknown.gotAnswer&&unknown.count==0);
  auto legacy=obd_isotp::parseDtcs(
      "43 01 71 00 00 00 00",0x43,DtcEncoding::Legacy);
  assert(legacy.gotAnswer&&!legacy.malformed&&legacy.count==1);
  // Incomplete data must not produce an apparently trustworthy DTC list.
  const char *badCases[]={
    "00A\n0:43040171030103",                // no segment 1
    "00A\n0:43040171030103\n2:020303",     // segment skipped
    "00A\n1:43040171030103\n2:020303",     // wrong initial sequence
    "00A\n0:43040171030103\n1:0203GG",     // non-hex payload
    "00A\n0:43040171030103\n1:02030304",   // exceeds declared length
    "00A\n0:43040171030103\n0:020303",     // repeated segment zero
    "041\n0:43040171030103",                // more than bounded 64 bytes
    "0:43 01 01 71",                         // no preceding length
    "00A\n0:43 04 01 71 03 01 03\n1:02 03 03\n"
    "002\n0:43",                            // valid preceding, truncated later
    "00A\n0:44 04 01 71 03 01 03\n1:02 03 03" // wrong DTC mode
  };
  for(const auto *raw:badCases){
    auto v=obd_isotp::parseDtcs(raw,0x43,DtcEncoding::Can);
    assert(v.malformed);
  }
  // A valid partial reply plus an ELM transport error is not success.
  const auto linkErr=obd_isotp::parseDtcs(
      "43 01 01 71\nBUFFER FULL",0x43,DtcEncoding::Can);
  assert(linkErr.malformed);
  const auto mixedHeader=obd_isotp::parseDtcs(
      "43 01 01 71\n7E8 10 0A 43 04 01 71 03 01",
      0x43,DtcEncoding::Can);
  assert(mixedHeader.malformed);
  const auto header29=obd_isotp::parseDtcs(
      "18DAF110 10 0A 43 04 01 71 03 01",0x43,DtcEncoding::Can);
  assert(header29.malformed);

  auto noData=obd_isotp::parseDtcs("NO DATA",0x43,DtcEncoding::Can);
  assert(!noData.gotAnswer&&!noData.malformed);
  auto wrongHeader=obd_isotp::parseDtcs(
      "7E8 10 0A 43 04 01 71 03 01\n7E8 21 03 02 03 03",
      0x43,DtcEncoding::Can);
  assert(!wrongHeader.gotAnswer); // ATH1 header bytes unsupported and not guessed.
  // Real ELM327 (AndrOBD issue #78): first frame 6 bytes, last consecutive
  // frame printed with ISO-TP padding up to 7 bytes. Must decode.
  auto padded=obd_isotp::parseDtcs(
      "00A\r0:43 04 01 71 03 01\r1:03 02 03 03 00 00 00",0x43,DtcEncoding::Can);
  assert(padded.gotAnswer&&!padded.malformed&&padded.count==4);
  assert(std::strcmp(padded.entries[3].code,"P0303")==0);
  auto paddedAA=obd_isotp::parseDtcs(
      "00A\n0:430401710301\n1:03020303AAAAAA",0x43,DtcEncoding::Can);
  assert(paddedAA.gotAnswer&&!paddedAA.malformed&&paddedAA.count==4);
  // Padding never tolerated on a partial frame or after completion.
  const char *badPadding[]={
    "00A\n0:430401710301\n1:0302030300",          // 5 bytes: not a full frame
    "00A\n0:430401710301\n1:03020303000000\n2:00", // segment after completion
    "004\n0:43010174000000",                       // padding on first frame
  };
  for(const auto *raw:badPadding){
    auto v=obd_isotp::parseDtcs(raw,0x43,DtcEncoding::Can);
    assert(v.malformed);
  }
  // Mode 06: two TIDs = 19 bytes, always multiframe on CAN.
  obd_diag::Misfire m;
  assert(obd_isotp::parseMisfire(
      "013\r0:46A20B240004\r1:00000010A20C24\r2:00020000001000",1,m));
  assert(m.hasEwma&&m.ewma==4&&m.hasCurrent&&m.current==2);
  assert(!obd_isotp::parseMisfire(
      "013\r0:46A20B240004\r1:00000010A20C24",1,m)); // truncated
  assert(!obd_isotp::parseMisfire(
      "013\r0:46A20B240004\r1:00000010A20C24\r2:0002",1,m)); // short last
  assert(obd_isotp::parseMisfire("46 A2 0B 24 00 04 00 00 00 10",1,m)); // single line
  assert(!obd_isotp::parseMisfire(
      "013\r0:46A20B240004\r1:00000010A20C24\r2:00020000001000",2,m)); // other MID
  assert(!obd_isotp::parseMisfire("NO DATA",1,m));
  return 0;
}
