#pragma once
// Strict bounded ELM327 ATH0 indexed ISO-TP DTC reassembly.
// ELM format: "00A\\r0:43040171030103\\r1:020303", headerless, no ECU IDs.
// Does NOT reconstruct interleaved ECU streams or transport-level CAN frames.
// No dynamic allocation, no ECU instructions, no Arduino dependencies.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "ObdDiagnostics.h"

namespace obd_isotp {

inline bool isHex(char c) { return obd_response::hexNibble(c)>=0; }

inline bool segmentBody(const char *str,std::size_t len,
                        std::uint8_t *output,std::size_t &size) {
  size=0;
  int first=-1;
  for(std::size_t i=0;i<len;++i){
    const char c=str[i];
    if(c==' '||c=='\t')continue;
    int n=obd_response::hexNibble(c);
    if(n<0)return false;
    if(first<0)first=n;
    else {
      if(size>=7)return false; // ELM indexed segments represent up to seven payload bytes.
      output[size++]=static_cast<std::uint8_t>((first<<4)|n);
      first=-1;
    }
  }
  return first<0 && size>0;
}
inline bool isLengthLine(const char *line,std::size_t len) {
  return len==3 && isHex(line[0]) && isHex(line[1]) && isHex(line[2]);
}
inline bool isIndexedLine(const char *line,std::size_t len) {
  return len>=3 && line[1]==':' && isHex(line[0]);
}
inline void absorb(obd_diag::DtcResult &dest,
                   const obd_diag::DtcResult &src) {
  if(src.malformed)dest.malformed=true;
  if(!src.gotAnswer)return;
  dest.gotAnswer=true;
  const unsigned total=unsigned(dest.ecuResponses)+src.ecuResponses;
  dest.ecuResponses=total>255?255:static_cast<std::uint8_t>(total);
  for(std::size_t i=0;i<src.count;++i) {
    bool dup=false;
    for(std::size_t k=0;k<dest.count;++k)
      if(std::strcmp(dest.entries[k].code,src.entries[i].code)==0){dup=true;break;}
    if(dup)continue;
    if(dest.count>=24){dest.malformed=true;return;}
    std::memcpy(dest.entries[dest.count++].code,src.entries[i].code,6);
  }
}

// Preserves the prior strict handling for headerless single-line CAN or
// Legacy replies. Adds ELM's *already formatted* indexed multiframe style.
// This does not send AT commands or alter adapter protocol configuration.
inline obd_diag::DtcResult parseDtcs(const char *response,
                                    std::uint8_t mode,
                                    obd_diag::DtcEncoding encoding) {
  obd_diag::DtcResult result;
  if(!response || (mode!=0x43&&mode!=0x47&&mode!=0x4A))
    return result;
  if(encoding!=obd_diag::DtcEncoding::Can)
    return obd_diag::parseDtcs(response,mode,encoding);

  std::uint8_t message[64]={};
  std::size_t used=0, expected=0;
  unsigned nextIndex=0;
  bool assembling=false;
  bool sawNoData=false;
  auto finish=[&]() {
    if(!assembling)return;
    if(used!=expected || used==0){
      result.malformed=true;
    }else{
      char plain[64*3+1]={};
      // Existing validated DTC CAN decoder expects complete bytes.
      std::size_t p=0;
      static constexpr char digits[]="0123456789ABCDEF";
      for(std::size_t i=0;i<used;++i){
        if(i)plain[p++]=' ';
        plain[p++]=digits[message[i]>>4];
        plain[p++]=digits[message[i]&0x0F];
      }
      plain[p]='\0';
      const auto r=obd_diag::parseDtcs(plain,mode,encoding);
      if(!r.gotAnswer)result.malformed=true;
      absorb(result,r);
    }
    assembling=false;used=expected=0;nextIndex=0;
  };
  auto parseLine=[&](const char *line,std::size_t len) {
    while(len && (line[len-1]==' '||line[len-1]=='\t'))--len;
    while(len && (*line==' '||*line=='\t')){++line;--len;}
    if(!len)return;
    if(isLengthLine(line,len)){
      finish();
      const int count=(obd_response::hexNibble(line[0])<<8) |
          (obd_response::hexNibble(line[1])<<4) |
           obd_response::hexNibble(line[2]);
      if(count<2 || count>64){result.malformed=true;return;}
      assembling=true;expected=static_cast<std::size_t>(count);
      used=0;nextIndex=0;
      return;
    }
    if(isIndexedLine(line,len)){
      if(!assembling){result.malformed=true;return;}
      const int idx=obd_response::hexNibble(line[0]);
      if(idx<0 || unsigned(idx)!=nextIndex || used>=expected){
        result.malformed=true;assembling=false;return;
      }
      std::uint8_t data[7]={};std::size_t n=0;
      if(!segmentBody(line+2,len-2,data,n) || n>expected-used){
        result.malformed=true;assembling=false;return;
      }
      std::memcpy(message+used,data,n);
      used+=n;
      nextIndex=(nextIndex+1)&0x0F;
      return;
    }
    finish();
    // A sequence/length or header marker must not be silently ignored as
    // a valid DTC reply. Other irrelevant echoed ELM commands are ignored.
    char plain[257]={};
    if(len>256){result.malformed=true;return;}
    std::memcpy(plain,line,len);
    plain[len]='\0';
    // Never present a partial successful ECU block as a *complete scan*
    // if the same ELM response also contains a link/transport failure.
    // Header-bearing raw CAN is not supported with the current ATH0 parser.
    if(std::strstr(plain,"NO DATA")){sawNoData=true;return;}
    static const char *fatal[]={
      "BUFFER FULL","CAN ERROR","BUS ERROR","DATA ERROR",
      "FB ERROR","STOPPED","UNABLE TO CONNECT","BUS INIT"};
    for(const char *term:fatal)
      if(std::strstr(plain,term)){result.malformed=true;return;}
    // Raw 11-bit or 29-bit CAN headers belong to ATH1 decoding,
    // which this intentionally restricted ATH0 parser does not do.
    bool header11 = len>=4 && plain[3]==' ';
    for(std::size_t k=0;k<3 && header11;++k)
      header11=isHex(plain[k]);
    bool header29 = len>=9 && plain[8]==' ';
    for(std::size_t k=0;k<8 && header29;++k)
      header29=isHex(plain[k]);
    if(header11 || header29){result.malformed=true;return;}
    const auto decoded=obd_diag::parseDtcs(plain,mode,encoding);
    absorb(result,decoded);
  };
  const char *begin=response;
  const char *ptr=response;
  for(std::size_t total=0;total<=1024;++total,++ptr){
    if(total==1024 || *ptr=='\0' || *ptr=='\n' || *ptr=='\r'){
      parseLine(begin,static_cast<std::size_t>(ptr-begin));
      if(total==1024){result.malformed=true;break;}
      if(*ptr=='\0')break;
      begin=ptr+1;
    }
  }
  finish();
  // A whole "NO DATA" response is not a valid empty DTC list,
  // but is not by itself malformed. Mixed partial success is incomplete.
  if(sawNoData && result.gotAnswer)result.malformed=true;
  return result;
}
} // namespace obd_isotp
