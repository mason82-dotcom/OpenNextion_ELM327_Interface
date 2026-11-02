#include "FuelTrimTest.h"
#include <cassert>
#include <cmath>
using namespace obd_trim;
static void status(Test &t,int rpm,std::uint32_t tm){
  t.sample(0x0C,rpm,tm);
  t.sample(0x0D,0,tm);
}
static void trims(Test &t,std::uint32_t tm,int copies=1,double stft=1,double ltft=2){
  for(int k=0;k<copies;++k){
    t.sample(0x06,stft,tm+k);
    t.sample(0x07,ltft,tm+k);
    t.sample(0x08,stft,tm+k);
    t.sample(0x09,ltft,tm+k);
    t.sample(0x10,5.0,tm+k);
    t.sample(0x0B,40.0,tm+k);
  }
}
int main(){
  Test t;
  assert(t.stage()==Stage::Idle);
  t.start();
  t.tick(0);assert(t.stage()==Stage::WaitIdle);
  status(t,800,100);
  t.tick(100);t.tick(3099);assert(t.stage()==Stage::WaitIdle);
  status(t,800,3100);t.tick(3100);assert(t.stage()==Stage::CollectIdle);
  // Old STFT value does not count: only explicitly supplied fresh replies.
  t.tick(4000);assert(t.idle().channels[0].count==0);
  trims(t,4100,3);
  assert(t.idle().channels[0].count==3);
  assert(t.idle().mean(0)==1);
  status(t,800,18000);t.tick(18000);
  status(t,800,23100);t.tick(23100);
  assert(t.stage()==Stage::WaitHigh);
  // Need 3 s stable near 2500 RPM.
  status(t,2500,23200);t.tick(23200);
  status(t,2500,26200);t.tick(26200);
  assert(t.stage()==Stage::CollectHigh);
  trims(t,26250,3,1,2);
  status(t,2500,35000);t.tick(35000);
  status(t,2500,41200);t.tick(41200);
  assert(t.stage()==Stage::Done);
  assert(t.idle().bankTotal(0)==3&&t.high().bankTotal(1)==3);
  assert(!t.bankDifference()&&!t.likelyVacuumPattern());
  assert(!t.highPositiveBothPhases()&&!t.negativeLarge());
  // Fault: speed too high restarts collection, discards old values.
  t.start();status(t,850,50000);t.tick(50000);
  status(t,850,53000);t.tick(53000);
  assert(t.stage()==Stage::CollectIdle);
  trims(t,53100,3);
  t.sample(0x0D,4,53200);t.tick(53200);
  assert(t.stage()==Stage::WaitIdle&&t.idle().channels[0].count==0);
  // Missing RPM/speed freshness also resets (not old cached data).
  status(t,800,60000);t.tick(60000);
  t.tick(63600);assert(t.stage()==Stage::WaitIdle);
  // Insufficient fresh LTFT ends with FAILED (no fake report).
  t.cancel();t.start();
  status(t,800,70000);t.tick(70000);
  status(t,800,73000);t.tick(73000);
  for(unsigned now=75000;now<=124000;now+=1000){
    status(t,800,now);
    t.sample(0x06,0,now);
    t.sample(0x08,0,now);
    t.tick(now);
  }
  assert(t.stage()==Stage::Failed);
  t.cancel();assert(!t.running()&&t.stage()==Stage::Idle);
  // No data ever means it cannot advance even after time.
  t.start();t.tick(999999);assert(t.stage()==Stage::WaitIdle);
  return 0;
}
