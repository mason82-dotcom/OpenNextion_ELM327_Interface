#include "ObdDiagnostics.h"
#include <cassert>
#include <cstring>
using namespace obd_diag;
int main(){
  Readiness r;
  assert(parseReadiness("41 01 00 07 25 00\n",r));
  assert(r.ecus==1&&!r.mil&&r.dtcs==0&&r.incompleteCount()==0);
  assert(parseReadiness("41 01 81 07 25 01\n41 01 01 07 25 04",r));
  assert(r.ecus==2&&r.mil&&r.dtcs==2&&r.incompleteCount()==2);
  assert(!parseReadiness("NO DATA",r));
  char code[6];decodeDtc(0x01,0x71,code);assert(std::strcmp(code,"P0171")==0);
  decodeDtc(0xC1,0x23,code);assert(std::strcmp(code,"U0123")==0);
  auto a=parseDtcs("43 01 01 71",0x43,DtcEncoding::Can);
  assert(a.gotAnswer&&a.count==1&&!a.malformed&&std::strcmp(a.entries[0].code,"P0171")==0);
  auto b=parseDtcs("43 01 71 00 00 00 00",0x43,DtcEncoding::Legacy);
  assert(b.gotAnswer&&b.count==1&&std::strcmp(b.entries[0].code,"P0171")==0);
  auto c=parseDtcs("4A 00",0x4A,DtcEncoding::Unknown);
  assert(c.gotAnswer&&c.count==0);
  auto u=parseDtcs("43 01 01 71",0x43,DtcEncoding::Unknown);
  assert(!u.gotAnswer&&u.count==0);
  auto bad=parseDtcs("43 02 01 71",0x43,DtcEncoding::Can);
  assert(!bad.gotAnswer&&bad.malformed);
  auto dupe=parseDtcs("43 01 01 71\r43 01 01 71",0x43,DtcEncoding::Can);
  assert(dupe.gotAnswer&&dupe.count==1&&dupe.ecuResponses==2);
  Misfire m;
  assert(parseMisfire("46 A2 0B 24 00 04 00 00 00 10",1,m));
  assert(m.hasEwma && m.ewma==4 && !m.hasCurrent);
  assert(!parseMisfire("46 A2 0B 01 00 04 00 00 00 10",1,m));
  assert(!parseMisfire("NO DATA",1,m));
  assert(m272Hint("P0017") && m272Hint("P0171"));
  assert(m272Hint("PFFFF")==nullptr);
  return 0;
}
