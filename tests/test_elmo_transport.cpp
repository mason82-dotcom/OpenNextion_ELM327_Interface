#include "src/ELMo/ELMo.h"
#include "ObdIsoTp.h"
#include <cassert>
#include <cstring>
#include <string>

static unsigned ticks = 0;
static void tick() { ++ticks; }
static ELMo *active = nullptr;
static void nestedSend() {
  ++ticks;
  String untouched("unchanged");
  assert(!active->send("0105", untouched, 20));
  assert(untouched.equals("unchanged"));
}

int main() {
  using Status = ELMo::Status;
  ELMo elm;
  String answer;
  // Managed send works on an already connected socket, without initialize()
  // or any implicit 0100/AT sequence. The default legacy API stays uninitialized.
  assert(elm.send(String("010C")).equals("NOT INITIALIZED"));
  elm.client.replies={"010C\r\nSEARCHING...\r41 0C 13 48\r>"};
  assert(elm.send("010C",answer,20,tick));
  assert(elm.lastStatus()==Status::Ok && ticks>0);
  assert(answer.equals("010C\n\nSEARCHING...\n41 0C 13 48\n"));
  assert(elm.client.sent.size()==1 && elm.client.sent[0]=="010C");
  uint8_t bytes[2];
  assert(obd_response::readPid(answer.c_str(),0x0C,bytes,2));
  assert(bytes[0]==0x13 && bytes[1]==0x48);

  // Keep complete responses including mixed NO DATA / success for the parser.
  // Collapsing to the upstream NO DATA sentinel would hide partial DTC data.
  elm.client.replies={"43 01 01 71\rNO DATA\r>"};
  assert(elm.send("03",answer,20));
  auto dtcs=obd_isotp::parseDtcs(answer.c_str(),0x43,obd_diag::DtcEncoding::Can);
  assert(dtcs.malformed);
  elm.client.replies={"00A\r0:43 04 01 71 03 01\r1:03 02 03 03 00 00 00\r>"};
  assert(elm.send("03",answer,20));
  dtcs=obd_isotp::parseDtcs(answer.c_str(),0x43,obd_diag::DtcEncoding::Can);
  assert(!dtcs.malformed && dtcs.count==4);

  // Actual delayed reply and millis() rollover, UI gets called while waiting.
  clockMs=0; ticks=0; elm.client.replyAt=12;
  elm.client.replies={"NO DATA>"};
  assert(elm.send("010C",answer,20,tick));
  assert(clockMs>=12 && ticks>=7 && answer.equals("NO DATA"));
  elm.client.replyAt=0; clockMs=UINT32_MAX-5;
  elm.client.replies={"incomplete"};
  assert(!elm.send("07",answer,12,tick));
  assert(elm.lastStatus()==Status::Timeout && !elm.client.connected());
  auto sent=elm.client.sent.size();
  assert(!elm.send("0A",answer,20));
  assert(elm.client.sent.size()==sent && elm.lastStatus()==Status::Disconnected);

  // Oversized raw input, including non-printable bytes, cannot grow the heap
  // or keep the receiver busy forever. Exactly 1024 bytes plus prompt fits.
  for (char c : {'A','\1'}) {
    elm.client.online=true;
    elm.client.replies={std::string(1025,c)+">"};
    assert(!elm.send("03",answer,100));
    assert(elm.lastStatus()==Status::Overflow && !elm.client.online);
    assert(answer.length()<=1024);
  }
  elm.client.online=true;
  elm.client.replies={std::string(1024,'A')+">"};
  assert(elm.send("03",answer,100) && answer.length()==1024);
  elm.client.replies={"43 00"}; elm.client.eofAfterBody=true;
  assert(!elm.send("03",answer,20));
  assert(elm.lastStatus()==Status::Disconnected && !elm.client.online);
  elm.client.eofAfterBody=false;

  // Short writes fail immediately and close the stream; no retry or next mode.
  elm.client.online=true; elm.client.shortWrite=true;
  elm.client.replies={"43 00>"};
  assert(!elm.send("03",answer,20));
  assert(elm.lastStatus()==Status::WriteError && !elm.client.online);
  elm.client.shortWrite=false; elm.client.online=true;
  sent=elm.client.sent.size();
  for (const char *bad : {"", "010C\r04", "010C\n", "\1", "012345678901234567890123456789012"}) {
    assert(!elm.send(bad,answer,20));
    assert(elm.lastStatus()==Status::InvalidCommand);
  }
  assert(!elm.send(nullptr,answer,20));
  assert(!elm.send("010C",answer,0));
  assert(elm.client.sent.size()==sent);

  String::failReserve=true;
  assert(!elm.send("03",answer,20));
  assert(elm.lastStatus()==Status::OutOfMemory && !elm.client.online);
  String::failReserve=false; elm.client.online=true;
  String::failConcat=true; elm.client.replies={"43 00>"};
  assert(!elm.send("03",answer,20));
  assert(elm.lastStatus()==Status::OutOfMemory && !elm.client.online);
  String::failConcat=false; elm.client.online=true;

  // Reentrant UI callbacks cannot send another request or overwrite status.
  active=&elm; sent=elm.client.sent.size();
  elm.client.replies={"43 00>"};
  assert(elm.send("03",answer,20,nestedSend));
  assert(elm.client.sent.size()==sent+1 && elm.lastStatus()==Status::Ok);
  assert(answer.equals("43 00"));
  // Typed incremental requests share one slot, and generic send cannot bypass it.
  elm.client.replies={"43 00>"};
  assert(elm.begin(obd_policy::Command::StoredDtcs,millis(),20));
  sent=elm.client.sent.size();
  assert(!elm.send("07",answer,20));
  assert(!elm.begin(obd_policy::Command::PendingDtcs,millis(),20));
  assert(elm.client.sent.size()==sent);
  elm.poll(millis());
  assert(elm.transaction.state()==obd_transport::State::Ready);
  assert(std::strcmp(elm.transaction.reply(),"43 00")==0);
  elm.transaction.reset();
  assert(!elm.begin(static_cast<obd_policy::Command>(255),millis(),20));
  assert(elm.client.sent.size()==sent);
  elm.transaction.reset();
  sent=elm.client.sent.size();
  assert(elm.stop());
  assert(!elm.client.online && elm.client.sent.size()==sent);
}
