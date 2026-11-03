#include "ObdPidCatalog.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <initializer_list>
using namespace obd_pids;
int main() {
  assert(count == 21);
  for (const Item &p : catalog) {
    assert(obd_policy::isVehicleRead(p.command));
    const char *cmd = obd_policy::wireCommand(p.command);
    assert(cmd && std::strlen(cmd) == 4 && cmd[0] == '0' && cmd[1] == '1');
  }
  const Item &rpm=catalog[indexOf(0x0C)];
  std::uint8_t v2[2]={0x13,0x48};
  double value=0;
  assert(decode(rpm, v2, 2, value) && value == 1234);
  assert(!decode(rpm, v2, 1, value));
  const Item &coolant=catalog[indexOf(0x05)];
  assert(decode(coolant, v2, 1, value) && value == -21);
  const Item &stft=catalog[indexOf(0x06)];
  v2[0]=0x80; assert(decode(stft,v2,1,value) && value==0);
  v2[0]=0x90; assert(decode(stft,v2,1,value) && std::fabs(value-12.5)<0.001);
  const Item &maf=catalog[indexOf(0x10)];
  v2[0]=0x01;v2[1]=0xF4;assert(decode(maf,v2,2,value)&&value==5.0);
  const Item &volt=catalog[indexOf(0x42)];
  v2[0]=0x34;v2[1]=0xBC;assert(decode(volt,v2,2,value)&&std::fabs(value-13.5)<1e-6);
  const Item &lambda=catalog[indexOf(0x44)];
  v2[0]=0x80;v2[1]=0;assert(decode(lambda,v2,2,value)&&value==1.0);
  std::uint32_t mask[3]={0x08180001u,0,0};
  // Published SAE scaling; timing is not a camshaft actual/target angle.
  v2[0]=0; assert(decode(catalog[indexOf(0x0E)],v2,1,value) && value==-64);
  v2[0]=255; assert(decode(catalog[indexOf(0x0E)],v2,1,value) && value==63.5);
  v2[0]=0x01;v2[1]=0x2C;
  assert(decode(catalog[indexOf(0x1F)],v2,2,value) && value==300);
  v2[0]=0x1B;v2[1]=0x58;
  assert(decode(catalog[indexOf(0x3C)],v2,2,value) && value==660);
  v2[0]=0;v2[1]=0;
  assert(decode(catalog[indexOf(0x3D)],v2,2,value) && value==-40);
  v2[0]=0xFF;
  assert(decode(catalog[indexOf(0x2E)],v2,1,value) && value==100);
  assert(decode(catalog[indexOf(0x2F)],v2,1,value) && value==100);
  for (uint8_t pid : {0x04,0x05,0x0C,0x0D,0x06,0x07,0x08,0x09,0x10,0x0B,0x0F,0x11,0x0E})
    assert(indexOf(pid)>=0 && bytesNeeded(catalog[indexOf(pid)].formula)<=2);
  assert(supported(0x05,mask)&&supported(0x0C,mask)&&supported(0x0D,mask));
  assert(!supported(0x42,mask)&&!supported(0,mask));
  mask[1]=0x00002000u;mask[2]=0x40000000u; // 0x33 and 0x42
  assert(supported(0x33,mask) && supported(0x42,mask));
  Scheduler schedule;
  int idx=schedule.choose(100,mask);
  assert(idx>=0 && supported(catalog[idx].pid,mask));
  bool sawRpm=false;for(int k=0;k<32;k++){int i=schedule.choose(200+k*250,mask);
    if(i>=0 && catalog[i].pid==0x0C)sawRpm=true;
  }
  assert(sawRpm);
  assert(obd_policy::wireCommand(static_cast<obd_policy::Command>(254)) == nullptr);
  return 0;
}
