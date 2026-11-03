#include "ObdTransaction.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
using namespace obd_transport;
using obd_policy::Command;
struct Io {
  bool online=true, shortWrite=false;
  unsigned reads=0, stops=0;
  std::string rx;
  std::vector<std::string> sent;
  bool connected(){return online;}
  int available(){return static_cast<int>(rx.size());}
  int read(){++reads;if(rx.empty())return -1;char c=rx.front();rx.erase(0,1);return c;}
  std::size_t write(const std::uint8_t *data,std::size_t n){
    sent.emplace_back(reinterpret_cast<const char *>(data),n);return shortWrite?n-1:n;
  }
  void stop(){online=false;++stops;rx.clear();}
};
int main(){
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::StoredDtcs,0,100));
    io.rx=std::string(1025,'\1')+">";
    while(t.pending())t.poll(io,1);
    assert(t.state()==State::Overflow && !io.online && std::strlen(t.reply())==0);
  }
  {
    Io io;Transaction t;
    assert(!t.begin(io,static_cast<Command>(255),0,100));
    assert(t.state()==State::Rejected && io.sent.empty() && !t.writeAttempted());
    t.reset();assert(!t.begin(io,Command::StoredDtcs,0,0));
    assert(io.sent.empty());
  }
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::StoredDtcs,100,3500));
    assert(io.sent[0]=="03\r");
    assert(!t.begin(io,Command::PendingDtcs,101,3500));
    assert(io.sent.size()==1 && t.pending());
    t.poll(io,101);assert(t.pending()); // no data, no spin and no disconnect
    for(char c:std::string("43 00\r>")){
      io.rx+=c;t.poll(io,120);
    }
    assert(t.state()==State::Ready && std::strcmp(t.reply(),"43 00\n")==0);
    assert(t.latency()==20 && io.online);
  }
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::Rpm,0,100));
    io.rx=std::string(200,'X');t.poll(io,1);
    assert(io.reads==Transaction::BYTE_BUDGET && t.pending());
    assert(std::strlen(t.reply())==64); // fixed work budget under continuous RX
    io.online=false;io.rx.clear();t.poll(io,2);
    assert(t.state()==State::Disconnected && io.stops==1);
  }
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::StoredDtcs,0,100));
    io.rx=std::string(1024,'A')+">";
    while(t.pending())t.poll(io,1);
    assert(t.state()==State::Ready && std::strlen(t.reply())==1024);
  }
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::StoredDtcs,0,100));
    io.rx=std::string(1025,'A');
    while(t.pending())t.poll(io,1);
    assert(t.state()==State::Overflow && !io.online);
  }
  {
    Io io;Transaction t;
    assert(t.begin(io,Command::StoredDtcs,0xFFFFFFF0U,100));
    io.rx="43 00";t.poll(io,0x10U);assert(t.pending());
    t.poll(io,0x54U);assert(t.state()==State::Timeout && !io.online);
    auto reads=io.reads;t.poll(io,0x55U);assert(io.reads==reads);
    assert(!t.begin(io,Command::PendingDtcs,0x56U,100));
    t.reset();io.online=true;
    assert(t.begin(io,Command::PendingDtcs,0x60U,100));
    assert(io.sent.size()==2 && io.sent.back()=="07\r");
  }
  {
    Io io;Transaction t;io.rx="old>";
    assert(t.begin(io,Command::Coolant,0,100));
    assert(t.discarded()==4 && io.rx.empty() && io.sent[0]=="0105\r");
  }
  {
    Io io;Transaction t;io.rx=std::string(65,'X');
    assert(!t.begin(io,Command::Rpm,0,100));
    assert(io.reads==64 && t.state()==State::StaleInput && io.sent.empty() && !io.online);
  }
  {
    Io io;Transaction t;io.shortWrite=true;
    assert(!t.begin(io,Command::Speed,0,100));
    assert(t.state()==State::WriteFailed && t.writeAttempted() && !io.online);
  }
}
