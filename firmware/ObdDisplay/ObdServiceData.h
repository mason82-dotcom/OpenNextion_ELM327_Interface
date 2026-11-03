#pragma once
// Bounded, headerless SAE service data: 06 support, 09 VIN, 02 frame 00.
// Sources/formats and intentionally unsupported variants: M272-S204-DIAGNOSTICS.md.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "ObdIsoTp.h"

namespace obd_service {
inline bool equals(const char *s, std::size_t n, const char *other) {
  return other && std::strlen(other)==n && std::memcmp(s,other,n)==0;
}

// Each visitor must accept the complete message or reject it. A malformed
// second ECU block or mixed NO DATA invalidates the entire result.
template<class Visitor>
bool messages(const char *response, const char *echo, Visitor visit) {
  if (!response) return false;
  obd_isotp::Assembler assembly;
  bool good=true, seen=false;
  auto deliver=[&](const std::uint8_t *data,std::size_t n) {
    seen=true;
    if (!visit(data,n)) good=false;
  };
  auto finish=[&]() {
    if (!assembly.active) return;
    if (!assembly.complete()) good=false;
    else deliver(assembly.message,assembly.used);
    assembly.reset();
  };
  auto line=[&](const char *s,std::size_t n) {
    while(n && (*s==' ' || *s=='\t')) {++s;--n;}
    while(n && (s[n-1]==' ' || s[n-1]=='\t')) --n;
    if (!n) return;
    if (obd_isotp::isLengthLine(s,n)) {
      finish();
      if (!assembly.begin(obd_isotp::lengthOf(s))) good=false;
    } else if (obd_isotp::isIndexedLine(s,n)) {
      if (!assembly.segment(s,n)) good=false;
    } else {
      finish();
      if (equals(s,n,echo) || equals(s,n,"SEARCHING...")) return;
      obd_response::Frame frame;
      // CAN headers, transport errors, NO DATA, arbitrary text: reject.
      if (!obd_response::decodeLine(s,n,frame)) good=false;
      else deliver(frame.data,frame.size);
    }
  };
  const char *start=response;
  const char *p=response;
  for(std::size_t n=0;n<=1024;++n,++p) {
    if(n==1024 || !*p || *p=='\r' || *p=='\n') {
      line(start,static_cast<std::size_t>(p-start));
      if(n==1024) {good=false;break;}
      if(!*p) break;
      start=p+1;
    }
  }
  finish();
  return good && seen;
}

inline bool bitmap(const char *response,std::uint8_t service,
                   std::uint8_t base,std::uint32_t &mask) {
  mask=0;
  if ((service!=0x46 && service!=0x49) || (base%32)!=0) return false;
  char echo[5];
  std::snprintf(echo,sizeof echo,"%02X%02X",unsigned(service-0x40),unsigned(base));
  std::uint32_t candidate=0;
  const bool ok=messages(response,echo,[&](const std::uint8_t *data,std::size_t n) {
    if(n!=6 || data[0]!=service || data[1]!=base) return false;
    std::uint32_t value=0;
    for(unsigned i=2;i<6;++i) value=(value<<8)|data[i];
    candidate|=value;
    return true;
  });
  if(ok) mask=candidate;
  return ok;
}

inline bool supported(std::uint8_t id,const std::uint32_t *masks,std::size_t count) {
  if(!masks || id==0 || std::size_t((id-1)/32)>=count) return false;
  return (masks[(id-1)/32] & (std::uint32_t(1)<<(31-(id-1)%32)))!=0;
}

inline bool vinCharacter(std::uint8_t c) {
  return (c>='0' && c<='9') || (c>='A' && c<='Z' && c!='I' && c!='O' && c!='Q');
}
// CAN format: 49 02 01 followed by exactly 17 VIN characters. No legacy
// J1850 sequence guessing, no ECU address attribution and no VIN logging.
inline bool vin(const char *response,char (&out)[18]) {
  out[0]='\0';
  char candidate[18]={};
  bool have=false;
  const bool ok=messages(response,"0902",[&](const std::uint8_t *data,std::size_t n) {
    if(n!=20 || data[0]!=0x49 || data[1]!=0x02 || data[2]!=1) return false;
    for(unsigned i=0;i<17;++i) if(!vinCharacter(data[i+3])) return false;
    if(have && std::memcmp(candidate,data+3,17)!=0) return false;
    std::memcpy(candidate,data+3,17);
    have=true;
    return true;
  });
  if(ok && have) std::memcpy(out,candidate,sizeof candidate);
  return ok && have;
}

// Reject conflicting duplicate ECU values and errors mixed into a snapshot.
inline bool freeze(const char *response,std::uint8_t pid,
                   std::uint8_t *out,std::size_t count) {
  if(!out || !count || count>4) return false;
  std::uint8_t candidate[4]={};
  bool have=false;
  char echo[7];
  std::snprintf(echo,sizeof echo,"02%02X00",unsigned(pid));
  const bool ok=messages(response,echo,[&](const std::uint8_t *data,std::size_t n) {
    if(n!=3+count || data[0]!=0x42 || data[1]!=pid || data[2]!=0) return false;
    if(have && std::memcmp(candidate,data+3,count)!=0) return false;
    std::memcpy(candidate,data+3,count);
    have=true;
    return true;
  });
  if(ok && have) std::memcpy(out,candidate,count);
  return ok && have;
}
inline bool freezeBitmap(const char *response,std::uint32_t &mask) {
  mask=0;
  std::uint8_t bytes[4];
  if(!freeze(response,0,bytes,4)) return false;
  for(auto b:bytes) mask=(mask<<8)|b;
  return true;
}
} // namespace obd_service
