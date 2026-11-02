#pragma once
// Offline descriptions adopted from the Android ELM327 project's generic DTC
// dictionary. Never infer a code meaning for manufacturer-specific DTCs.
#include <cstring>
namespace obd_dtc_names {
struct Entry {const char *code;const char *de;};
static constexpr Entry entries[]={
  {"P0016","KW/NW-Korrelation Bank 1 Sensor A"},
  {"P0017","KW/NW-Korrelation Bank 1 Sensor B"},
  {"P0018","KW/NW-Korrelation Bank 2 Sensor A"},
  {"P0019","KW/NW-Korrelation Bank 2 Sensor B"},
  {"P0100","Luftmassenmesser Stromkreis"},
  {"P0101","Luftmassenmesser Bereich/Funktion"},
  {"P0102","Luftmassenmesser Signal niedrig"},
  {"P0103","Luftmassenmesser Signal hoch"},
  {"P0104","Luftmassenmesser Signal sporadisch"},
  {"P0171","Gemisch mager Bank 1"},
  {"P0172","Gemisch fett Bank 1"},
  {"P0174","Gemisch mager Bank 2"},
  {"P0175","Gemisch fett Bank 2"},
  {"P0300","Mehrfach-/zufaellige Aussetzer"},
  {"P0301","Aussetzer Zylinder 1"},
  {"P0302","Aussetzer Zylinder 2"},
  {"P0303","Aussetzer Zylinder 3"},
  {"P0304","Aussetzer Zylinder 4"},
  {"P0305","Aussetzer Zylinder 5"},
  {"P0306","Aussetzer Zylinder 6"},
  {"P0420","Katalysator unter Schwelle Bank 1"},
  {"P0430","Katalysator unter Schwelle Bank 2"}
};
inline const char *describe(const char *code){
  if(!code)return nullptr;
  for(const auto &entry:entries)
    if(std::strcmp(code,entry.code)==0)return entry.de;
  return nullptr;
}
}
