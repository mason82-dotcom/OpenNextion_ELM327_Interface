"""Run actual manual inspection/Mode-06 handlers with ELMo and host doubles.

No screen layout, real ESP32 timing, actual Mercedes ECU or vehicle is tested.
"""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import elm327_lab_simulator as lab  # noqa: E402
from test_dtc_workflow import PRELUDE  # noqa: E402

SRC=(ROOT/"firmware/ObdDisplay/ObdDisplay.ino").read_text(encoding="utf-8")


def section(start,end):
    return start+SRC.split(start,1)[1].split(end,1)[0]


EXTRA=r'''
#include "ObdServiceData.h"
#include "ObdPidCatalog.h"
lv_obj_t diagnosticLabels[2];
lv_obj_t *inspectionText=&diagnosticLabels[0],*misfireText=&diagnosticLabels[1];
String inspectionReport,misfireReport;
uint8_t inspectionStep=0,misfireStep=0,misfireDiscoveryStep=0;
uint32_t inspectionInfoMask=0,inspectionFreezeMask=0,misfireMids[6]={};
bool inspectionFreezeAvailable=false,misfireMidsReady=false;
uint32_t pidMasks[3]={0x9FBF8003,0x8007A019,0x50000000};
bool pidSupported(uint8_t pid){return obd_pids::supported(pid,pidMasks);}
void cancelDtcScan(){dtcScanRequested=false;}
void showPage(uint8_t){}
namespace obd_log {
size_t mode06Row(char *,size_t,uint32_t,uint8_t,const char *,uint16_t,uint32_t){return 0;}
}
String trimNumber(double value,uint8_t decimals){
  char text[64];snprintf(text,sizeof text,"%.*f",int(decimals),value);return text;
}
'''

SCENARIOS=r'''
int main(){
  lv_event_t click;
  onMode06Button(&click);
  // Only cylinders 1 and 6 announced: four others never go on the wire.
  adapter.replies={"46 00 00 00 00 01>","46 20 00 00 00 01>",
    "46 40 00 00 00 01>","46 60 00 00 00 01>","46 80 00 00 00 01>",
    "46 A0 42 00 00 00>","46 A2 0B 24 00 04 00 00 FF FF>",
    "46 A7 0B 24 00 02 00 00 FF FF>"};
  for(unsigned i=0;i<12;++i)scanMisfireStep();
  assert(!misfireRequested);
  assert((adapter.sent==std::vector<std::string>{
      "0600","0620","0640","0660","0680","06A0","06A2","06A7"}));
  assert(std::strstr(misfireReport.c_str(),"Zyl. 2: MID nicht unterstuetzt"));
  assert(std::strstr(misfireReport.c_str(),"EWMA 4"));
  assert(std::strstr(misfireReport.c_str(),"Zyl. 6 (B2): EWMA 2"));

  onMode06Button(&click); adapter.sent.clear();
  adapter.replies={"46 00 00 00 00 00>"};
  for(unsigned i=0;i<7;++i)scanMisfireStep();
  assert(adapter.sent.size()==1 && !misfireRequested);
  assert(!std::strstr(misfireReport.c_str(),"EWMA 0"));
  onMode06Button(&click); adapter.sent.clear();adapter.replies={"NO DATA>"};
  scanMisfireStep();
  assert(adapter.sent.size()==1 && !misfireRequested && adapter.online);

  // A new diagnostic during capability reception cannot commit an old mask.
  onMode06Button(&click);adapter.sent.clear();adapter.replies={"46 00 00 00 00 01>"};
  tickAction=[&](){onInspectionButton(&click);};
  scanMisfireStep();
  assert(inspectionRequested && !misfireRequested && misfireDiscoveryStep==0);
  assert(misfireMids[0]==0);
  dtcEncoding=obd_diag::DtcEncoding::Legacy;
  onMode06Button(&click);adapter.sent.clear();scanMisfireStep();
  assert(adapter.sent.empty() && !misfireRequested);
  dtcEncoding=obd_diag::DtcEncoding::Can;

  // Full nonzero freeze-frame DTC, advertised VIN, and all frame-00 PIDs.
  onInspectionButton(&click);adapter.sent.clear();adapter.replies=fullInspectionReplies;
  for(unsigned i=0;inspectionRequested && i<30;++i)scanInspectionStep();
  assert(!inspectionRequested && adapter.replies.empty());
  assert(adapter.sent.size()==24 && adapter.sent[4]=="020200");
  assert(adapter.sent[8]=="0900" && adapter.sent[9]=="0902");
  assert(adapter.sent[10]=="020000" && adapter.sent[23]=="020E00");
  assert(std::strstr(inspectionReport.c_str(),"FIN: WDD2042561F123456"));
  assert(std::strstr(inspectionReport.c_str(),"FF Drehzahl: 1234 rpm"));
  assert(std::strstr(inspectionReport.c_str(),"FF LTFT B1: 7.8 %"));
  assert(Serial.log.find("WDD204")==std::string::npos); // display-only VIN

  // The live bitmap advertises all PIDs, but frame 00 advertises only RPM.
  // Snapshot queries must use their own bitmap rather than the live catalog.
  onInspectionButton(&click);adapter.sent.clear();
  adapter.replies=std::deque<std::string>(fullInspectionReplies.begin(),fullInspectionReplies.begin()+10);
  adapter.replies.push_back("42 00 00 40 10 00 00>");
  adapter.replies.push_back("42 0C 00 13 48>");
  for(unsigned i=0;inspectionRequested && i<30;++i)scanInspectionStep();
  assert(!inspectionRequested && adapter.sent.size()==12 && adapter.replies.empty());
  assert(adapter.sent.back()=="020C00");
  assert(std::strstr(inspectionReport.c_str(),"FF Motorlast: nicht unterstuetzt / n/v"));

  // No stored snapshot and no advertised VIN: neither is queried speculatively.
  onInspectionButton(&click);adapter.sent.clear();
  adapter.replies={"41 01 00 07 25 00>","41 21 00 00>","41 30 05>",
    "41 31 01 2C>","42 02 00 00 00>","43 00>","47 00>","4A 00>",
    "49 00 00 00 00 00>"};
  for(unsigned i=0;inspectionRequested && i<30;++i)scanInspectionStep();
  assert(!inspectionRequested && adapter.sent.size()==9 && adapter.replies.empty());
  for(const auto &wire:adapter.sent)assert(wire!="0902" && wire!="020000" && wire!="020C00");

  // A cancelled/replaced inspection must not publish the old VIN response.
  onInspectionButton(&click);inspectionStep=9;inspectionInfoMask=0x40000000;
  adapter.replies={fullInspectionReplies[9]};
  tickAction=[&](){onMode06Button(&click);};
  scanInspectionStep();
  assert(misfireRequested && !inspectionRequested);
  assert(!std::strstr(inspectionReport.c_str(),"FIN: WDD204"));
}
'''


class M272WorkflowTest(unittest.TestCase):
    def test_actual_manual_diagnostics_and_capability_gates(self):
        commands=("0101","0121","0130","0131","020200","03","07","0A",
                  "0900","0902","020000","020400","020500","020C00","020D00",
                  "020600","020700","020800","020900","021000","020B00",
                  "020F00","021100","020E00")
        replies=",".join(json.dumps(lab.answer_for(c,dtc_fixture="PADDED")+">") for c in commands)
        functions=(
            section("static void onInspectionButton(","static void onMode06Button("),
            section("static void onMode06Button(","static void buildUi()"),
            section("static bool startTransaction(","// Once per second;"),
            section("static void scanInspectionStep()","static void scanMisfireStep()"),
            section("static void scanMisfireStep()","#endif // OBD_CONNECTED_BUILD"),
        )
        # Use the same integer PID formatting as the real library; no credentials.
        program=PRELUDE+EXTRA+"\n".join(functions)
        program+="\nstd::deque<std::string> fullInspectionReplies={"+replies+"};\n"+SCENARIOS
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp)/"workflow.cpp"
            binary=Path(tmp)/"workflow-test"
            source.write_text(program,encoding="utf-8")
            result=subprocess.run([
                "g++","-std=c++17","-Wall","-Wextra","-Werror",
                "-DOBD_CONNECTED_BUILD=1","-DOBD_LAB_BUILD=1",
                "-I"+str(ROOT/"tests/host"),"-I"+str(ROOT/"firmware/ObdDisplay"),
                str(source),str(ROOT/"firmware/ObdDisplay/src/ELMo/ELMo.cpp"),
                "-o",str(binary),
            ],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)


if __name__=="__main__":
    unittest.main()
