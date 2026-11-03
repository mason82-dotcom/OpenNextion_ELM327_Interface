#include "VehicleProfile.h"
#include "ObdIsoTp.h"
#include "ReadOnlyPolicy.h"
#include <cassert>
#include <cstring>
using namespace obd_vehicle;
int main(){
  static_assert(active.cylinders==6 && active.banks==2, "C350 M272: V6");
  assert(std::strstr(active.name,"C350")!=nullptr);
  assert(std::strstr(active.name,"S204") && std::strstr(active.name,"272.961"));
  assert(active.bus==ExpectedBus::Can && active.m272Hints);
  assert(bankOf(active,1)==1 && bankOf(active,3)==1 && bankOf(active,4)==2 && bankOf(active,6)==2);
  assert(busMatches(active,true) && !busMatches(active,false));
  assert(idlePlausible(active,650) && !idlePlausible(active,2500) && !idlePlausible(active,0));
  // Every profile cylinder maps to an allowlisted Mode 06 wire command.
  const obd_policy::Command mis[]={
    obd_policy::Command::MisfireCyl1,obd_policy::Command::MisfireCyl2,
    obd_policy::Command::MisfireCyl3,obd_policy::Command::MisfireCyl4,
    obd_policy::Command::MisfireCyl5,obd_policy::Command::MisfireCyl6};
  for(unsigned c=0;c<active.cylinders;++c) assert(obd_policy::allowedWire(mis[c]));
  // Simulator Mode 06 fixture shape decodes per cylinder.
  obd_diag::Misfire m;
  assert(obd_isotp::parseMisfire("013\r0:46 A7 0B 24 00 00\r1:00 00 00 10 A7 0C 24\r"
                                 "2:00 00 00 00 00 10 00",6,m));
  assert(m.hasEwma && m.hasCurrent && m.ewma==0 && m.current==0);
  return 0;
}
