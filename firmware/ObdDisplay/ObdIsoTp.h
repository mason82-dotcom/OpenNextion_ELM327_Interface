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

// Bounded reassembly of one ELM ATH0/CAF1 indexed message ("00A", "0:..",
// "1:.."). Real adapters print the last consecutive frame UNTRIMMED: e.g.
// "00A / 0:470401180122 / 1:02232610000000" (10 bytes + 3 padding bytes).
// Extra bytes are accepted only on a full 7-byte consecutive frame (index
// != 0) that completes the message, i.e. the CAN frame padding of
// ISO 15765-2. Any shorter overrun stays malformed.
struct Assembler {
  std::uint8_t message[64]={};
  std::size_t used=0, expected=0;
  unsigned nextIndex=0;
  bool active=false;
  void reset(){active=false;used=expected=0;nextIndex=0;}
  bool begin(int count){
    reset();
    if(count<2 || count>64)return false;
    active=true;expected=static_cast<std::size_t>(count);
    return true;
  }
  bool complete() const {return active && used>0 && used==expected;}
  // line points at "<idx>:<payload>"; returns false when malformed.
  bool segment(const char *line,std::size_t len){
    if(!active)return false;
    const int idx=obd_response::hexNibble(line[0]);
    if(idx<0 || unsigned(idx)!=nextIndex || used>=expected){reset();return false;}
    std::uint8_t data[7]={};std::size_t n=0;
    if(!segmentBody(line+2,len-2,data,n)){reset();return false;}
    const std::size_t remaining=expected-used;
    if(n>remaining){
      const bool paddedLastFrame = idx!=0 || used!=0;
      if(!(paddedLastFrame && n==7)){reset();return false;}
      n=remaining; // drop ISO-TP frame padding, never data
    }
    std::memcpy(message+used,data,n);
    used+=n;
    nextIndex=(nextIndex+1)&0x0F;
    return true;
  }
};
inline int lengthOf(const char *line){
  return (obd_response::hexNibble(line[0])<<8) |
         (obd_response::hexNibble(line[1])<<4) |
          obd_response::hexNibble(line[2]);
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

  Assembler asm_;
  bool sawNoData=false;
  auto finish=[&]() {
    if(!asm_.active)return;
    if(!asm_.complete()){
      result.malformed=true;
    }else{
      char plain[64*3+1]={};
      // Existing validated DTC CAN decoder expects complete bytes.
      std::size_t p=0;
      static constexpr char digits[]="0123456789ABCDEF";
      for(std::size_t i=0;i<asm_.used;++i){
        if(i)plain[p++]=' ';
        plain[p++]=digits[asm_.message[i]>>4];
        plain[p++]=digits[asm_.message[i]&0x0F];
      }
      plain[p]='\0';
      const auto r=obd_diag::parseDtcs(plain,mode,encoding);
      if(!r.gotAnswer)result.malformed=true;
      absorb(result,r);
    }
    asm_.reset();
  };
  auto parseLine=[&](const char *line,std::size_t len) {
    while(len && (line[len-1]==' '||line[len-1]=='\t'))--len;
    while(len && (*line==' '||*line=='\t')){++line;--len;}
    if(!len)return;
    if(isLengthLine(line,len)){
      finish();
      if(!asm_.begin(lengthOf(line)))result.malformed=true;
      return;
    }
    if(isIndexedLine(line,len)){
      if(!asm_.segment(line,len))result.malformed=true;
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

// Mode 06 misfire counters (MID A2-A7) from a headerless CAN reply.
// A single MID with TID 0B+0C is 19 bytes, so real ISO 15765 adapters always
// answer in the indexed multiframe format. Any malformed or transport error
// line yields "not available"; nothing is guessed.
inline bool parseMisfire(const char *response,std::uint8_t cylinder,
                         obd_diag::Misfire &out){
  out={};
  if(!response || cylinder<1 || cylinder>6)return false;
  const std::uint8_t mid=std::uint8_t(0xA1+cylinder);
  Assembler asm_;
  bool bad=false;
  auto finish=[&](){
    if(!asm_.active)return;
    if(asm_.complete())
      obd_diag::accumulateMisfire(asm_.message,asm_.used,mid,out);
    else bad=true;
    asm_.reset();
  };
  auto parseLine=[&](const char *line,std::size_t len){
    while(len && (line[len-1]==' '||line[len-1]=='\t'))--len;
    while(len && (*line==' '||*line=='\t')){++line;--len;}
    if(!len)return;
    if(isLengthLine(line,len)){
      finish();
      if(!asm_.begin(lengthOf(line)))bad=true;
      return;
    }
    if(isIndexedLine(line,len)){
      if(!asm_.segment(line,len))bad=true;
      return;
    }
    finish();
    obd_response::Frame frame;
    if(obd_response::decodeLine(line,len,frame))
      obd_diag::accumulateMisfire(frame.data,frame.size,mid,out);
  };
  const char *begin=response;
  const char *ptr=response;
  for(std::size_t total=0;total<=1024;++total,++ptr){
    if(total==1024 || *ptr=='\0' || *ptr=='\n' || *ptr=='\r'){
      parseLine(begin,static_cast<std::size_t>(ptr-begin));
      if(total==1024){bad=true;break;}
      if(*ptr=='\0')break;
      begin=ptr+1;
    }
  }
  finish();
  if(bad){out={};return false;}
  return out.hasEwma||out.hasCurrent;
}
} // namespace obd_isotp
