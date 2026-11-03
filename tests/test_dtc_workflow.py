"""Execute the firmware's real DTC callbacks, scan step and transmit function.

Host doubles replace Arduino/LVGL/TCP and the disconnect UI. This checks
control flow and parser integration, not ESP32 timing, pixels or real adapters.
All injected/forbidden commands remain inside this in-process mock.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "firmware/ObdDisplay/ObdDisplay.ino").read_text()


def section(start, end):
    return SRC.split(start, 1)[1].split(end, 1)[0]


PRELUDE = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "ReadOnlyPolicy.h"
#include "ObdIsoTp.h"
#include "ObdDtcNames.h"
#include "VehicleProfile.h"
#include "Arduino.h"
#include "src/ELMo/ELMo.h"
using obd_policy::Command;
static constexpr const obd_vehicle::Profile &VEHICLE=obd_vehicle::active;
struct lv_obj_t {std::string text;int scroll=99;};
struct lv_event_t {int code=1;};
constexpr int LV_EVENT_CLICKED=1,LV_ANIM_OFF=0;
int lv_event_get_code(lv_event_t *e){return e->code;}
void lv_label_set_text(lv_obj_t *o,const char *s){o->text=s;}
void lv_label_set_text_fmt(lv_obj_t *o,const char *fmt,...){
  char b[256];va_list a;va_start(a,fmt);vsnprintf(b,sizeof b,fmt,a);va_end(a);o->text=b;
}
lv_obj_t *lv_obj_get_parent(lv_obj_t *o){return o;}
void lv_obj_scroll_to_y(lv_obj_t *o,int y,int){o->scroll=y;}
lv_obj_t labels[5];
lv_obj_t *scanText=&labels[0],*dtcDetailText=&labels[1];
lv_obj_t *dtcModeText[]={&labels[2],&labels[3],&labels[4]};
bool adapterReady=true,dtcScanRequested=false,inspectionRequested=false,misfireRequested=false;
uint32_t diagGeneration=0,txCount=0,timeoutCount=0,inputErrorCount=0,lastLatencyMs=0;
uint16_t dtcScanCodes=0;
bool dtcScanUndecodable=false;
uint8_t dtcIndex=0;
String dtcReport;
obd_diag::DtcEncoding dtcEncoding=obd_diag::DtcEncoding::Can;
constexpr uint32_t CMD_TIMEOUT_MS=20;
std::function<void()> tickAction;
void uiTick(){auto action=std::move(tickAction);tickAction=nullptr;if(action)action();}
ELMo elm;
WiFiClient &adapter=elm.client;
void cancelTrimForOtherDiagnostic(){}
void sdEvent(const char *){}
namespace obd_log {size_t dtcRow(char *,size_t,uint32_t,const char *,const char *){return 0;}}
void sdWrite(char *,size_t){}
// The real disconnect UI is covered by test_session_invalidation.py.
// Here we check that the scan invokes it on transport failure.
unsigned disconnectCalls=0;
void markDisconnected(const char *){++disconnectCalls;adapterReady=false;
  dtcScanRequested=false;++diagGeneration;}
'''

SCENARIOS = r'''
int main(){
  lv_event_t click;
  // Invalid enum values never reach the sole wire path, including log output.
  String answer;
  assert(!transact(static_cast<Command>(255),answer));
  assert(adapter.sent.empty() && Serial.log.empty());
  onScanButton(&click);
  assert(dtcScanRequested && scanText->text=="Lese DTC...");
  assert(dtcDetailText->scroll==0);
  auto generation=diagGeneration;
  // Ten clicks while a reply is pending leave the original scan untouched.
  adapter.replies={"43 01 01 71\r>","47 00\r>","4A 00\r>"};
  tickAction=[&](){for(int i=0;i<10;++i)onScanButton(&click);};
  scanOneDtcMode();scanOneDtcMode();scanOneDtcMode();
  assert(diagGeneration==generation && !dtcScanRequested);
  assert((adapter.sent==std::vector<std::string>{"03","07","0A"}));
  assert(scanText->text=="1 Codes -> DETAILS");
  assert(std::strstr(dtcReport.c_str(),"P0171"));
  assert(Serial.log.find("OBD_TX,")!=std::string::npos);
  // NO DATA must never turn into a false all-clear.
  onScanButton(&click);
  adapter.replies={"43 00>","47 00>","NO DATA>"};
  for(int i=0;i<3;++i)scanOneDtcMode();
  assert(dtcScanUndecodable && scanText->text=="RAW -> DETAILS");
  // Cancel after a partial result, during the next transport transaction.
  onScanButton(&click);
  adapter.replies={"43 01 01 71>","47 01 03 01>"};
  scanOneDtcMode();
  tickAction=[&](){onDtcCancel(&click);};
  scanOneDtcMode();
  assert(!dtcScanRequested && dtcIndex==0 && dtcScanCodes==0);
  assert(scanText->text=="Abgebrochen: neu lesen");
  assert(dtcModeText[0]->text=="DTC: -- (abgebrochen)");
  assert(!std::strstr(dtcReport.c_str(),"P0171"));
  assert(!std::strstr(dtcReport.c_str(),"P0301"));
  assert(adapter.online && adapter.rx.empty()); // prompt drained, no interrupt
  generation=diagGeneration;
  onDtcCancel(&click); // idle cancel is harmless
  assert(diagGeneration==generation);
  // Cancel + explicit restart during an old reply: discard the old generation.
  onScanButton(&click);
  adapter.replies={"43 01 01 71>"};
  tickAction=[&](){onDtcCancel(&click);onScanButton(&click);};
  scanOneDtcMode();
  assert(dtcScanRequested && dtcIndex==0);
  assert(!std::strstr(dtcReport.c_str(),"P0171"));
  adapter.replies={"43 00>","47 00>","4A 00>"};
  for(int i=0;i<3;++i)scanOneDtcMode();
  assert(!dtcScanRequested && scanText->text=="Keine DTCs");
  // Missing prompt after cancellation closes the stream, never sends next mode.
  onScanButton(&click);
  adapter.replies={"43 01 01 71"};
  auto sent=adapter.sent.size();
  tickAction=[&](){onDtcCancel(&click);};
  scanOneDtcMode();
  assert(adapter.sent.size()==sent+1 && !adapter.online);
  assert(disconnectCalls==1 && timeoutCount==1 && !dtcScanRequested);
  // Reconnection alone has no callback that schedules a new DTC scan.
  adapter.online=true;adapterReady=true;
  assert(!dtcScanRequested);
  // Successful previously tested PID queries still use the same transmit path.
  for(auto c:{Command::Rpm,Command::Coolant,Command::Speed}){
    adapter.replies={"41 00>"};assert(transact(c,answer));
  }
  // Late bytes must not become the next PID reply; bounded stale-input drain.
  adapter.rx="old response>";
  adapter.replies={"41 0C 13 48>"};
  auto errors=inputErrorCount;
  assert(transact(Command::Rpm,answer));
  assert(answer.equals("41 0C 13 48") && inputErrorCount==errors+1);
  adapter.rx=std::string(1025,'X');
  auto before=adapter.sent.size();
  assert(!transact(Command::Rpm,answer));
  assert(!adapter.online && adapter.sent.size()==before);
  for(const auto &wire:adapter.sent)
    assert(obd_policy::allowedVehicleWire(wire.c_str()));
}
'''


class DtcWorkflowTest(unittest.TestCase):
    def test_real_callbacks_scan_and_transport_with_host_doubles(self):
        functions = (
            "static void cancelDtcScan() {" + section(
                "static void cancelDtcScan() {", "#endif\n\nstatic void onDtcCancel"),
            "static void onDtcCancel" + section(
                "static void onDtcCancel", "static void onScanButton"),
            "static void onScanButton" + section(
                "static void onScanButton", "\n\n#if OBD_CONNECTED_BUILD\nstatic void renderTrimPage"),
            "static bool transact(" + section(
                "static bool transact(", "// Once per second;"),
            "static void scanOneDtcMode()" + section(
                "static void scanOneDtcMode()", "static void scanInspectionStep()"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "dtc.cpp"
            binary = Path(tmp) / "dtc-test"
            source.write_text(PRELUDE + "\n".join(functions) + SCENARIOS)
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DOBD_CONNECTED_BUILD=1", "-DOBD_LAB_BUILD=1",
                "-I" + str(ROOT / "tests/host"),
                "-I" + str(ROOT / "firmware/ObdDisplay"), str(source), "-o", str(binary),
                str(ROOT / "firmware/ObdDisplay/src/ELMo/ELMo.cpp"),
            ], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_scan_superseding_actions_invalidate_partial_dtc_results(self):
        for start, end in (
            ("static void onTrimStart(", "static void onTrimCancel("),
            ("static void onInspectionButton(", "static void onMode06Button("),
            ("static void onMode06Button(", "static void buildUi()"),
        ):
            with self.subTest(handler=start):
                self.assertIn("cancelDtcScan();", section(start, end))


if __name__ == "__main__":
    unittest.main()
