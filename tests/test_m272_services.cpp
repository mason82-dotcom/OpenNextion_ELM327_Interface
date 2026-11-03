#include "ObdServiceData.h"
#include "ReadOnlyPolicy.h"
#include <cassert>
#include <cstring>
#include <string>

int main(int argc,char **argv) {
  if(argc==3) {
    // Exact simulator VIN fixture, fed by the Python test over argv.
    char vin[18];
    assert(obd_service::vin(argv[1],vin));
    assert(std::strcmp(vin,argv[2])==0);
    return 0;
  }
  std::uint32_t mask=0;
  assert(obd_service::bitmap("0600\r46 00 00 00 00 01",0x46,0,mask) && mask==1);
  assert(obd_service::bitmap("46 A0 7E 00 00 00",0x46,0xA0,mask));
  std::uint32_t mids[6]={0,0,0,0,0,mask};
  for(unsigned i=0xA2;i<=0xA7;++i) assert(obd_service::supported(i,mids,6));
  assert(!obd_service::supported(0xA1,mids,6));
  assert(!obd_service::supported(0,mids,6));
  assert(!obd_service::supported(0xFF,mids,6));
  assert(!obd_service::bitmap("46 A0 7E 00 00 00\nNO DATA",0x46,0xA0,mask));
  assert(mask==0);
  assert(!obd_service::bitmap("46 A0 7E 00 00",0x46,0xA0,mask));
  assert(!obd_service::bitmap("46 00 7E 00 00 00",0x46,0xA0,mask));
  assert(!obd_service::bitmap("46 A0 7E 00 00 00\nCAN ERROR",0x46,0xA0,mask));
  assert(obd_service::bitmap("49 00 40 00 00 00",0x49,0,mask));
  assert(obd_service::supported(2,&mask,1));
  // ELM327 datasheet p44: complete 20-byte CAN VIN response.
  const char *raw="014\r0:49 02 01 31 44 34\r1:47 50 30 30 52 35 35\r2:42 31 32 33 34 35 36";
  char vin[18];
  assert(obd_service::vin(raw,vin) && std::strcmp(vin,"1D4GP00R55B123456")==0);
  assert(obd_service::vin((std::string(raw)+"\n"+raw).c_str(),vin));
  const char *bad[]={
    "014\r0:49 02 01 31 44 34\r1:47 50 30 30 52 35 35", // truncated
    "014\r0:49 02 01 31 44 34\r2:47 50 30 30 52 35 35", // bad sequence
    "49 02 01 31 44 34", // short
    "7E8 10 14 49 02 01 31 44 34", // raw CAN not supported
    "49 02 02 31 44 34 47 50 30 30 52 35 35 42 31 32 33 34 35 36", // wrong count
    "49 02 01 49 44 34 47 50 30 30 52 35 35 42 31 32 33 34 35 36", // forbidden I
    "NO DATA"
  };
  for(const auto *s:bad) {assert(!obd_service::vin(s,vin));assert(!vin[0]);}
  assert(!obd_service::vin((std::string(raw)+"\nBUFFER FULL").c_str(),vin));
  const char *other="49 02 01 32 44 34 47 50 30 30 52 35 35 42 31 32 33 34 35 36";
  assert(!obd_service::vin((std::string(raw)+"\n"+other).c_str(),vin)); // conflicting ECUs
  assert(!obd_service::vin(std::string(1025,'A').c_str(),vin));

  std::uint8_t bytes[2]={};
  assert(obd_service::freeze("42 0C 00 13 48",0x0C,bytes,2));
  assert(bytes[0]==0x13 && bytes[1]==0x48);
  assert(!obd_service::freeze("42 0C 01 13 48",0x0C,bytes,2));
  assert(!obd_service::freeze("42 0C 00 13 48\nNO DATA",0x0C,bytes,2));
  assert(!obd_service::freeze("42 0C 00 13 48\n42 0C 00 27 10",0x0C,bytes,2));
  assert(obd_service::freezeBitmap("42 00 00 5F BF 80 00",mask));
  assert(mask==0x5FBF8000);
  for(const char *wire:{"020000","020E00","020600","0900","0902","06A0"})
    assert(obd_policy::allowedVehicleWire(wire));
  for(const char *wire:{"020C01","02FF00","0904","0900\r04","04","08","221234"})
    assert(!obd_policy::allowedVehicleWire(wire));
}
