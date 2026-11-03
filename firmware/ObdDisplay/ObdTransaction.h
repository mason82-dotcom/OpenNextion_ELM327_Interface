#pragma once
// Single-slot ELM transaction. No Arduino, heap, threads or raw command API.
// poll() consumes at most 64 bytes and never waits for additional input.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "ReadOnlyPolicy.h"

namespace obd_transport {
enum class State : std::uint8_t {
  Idle, Pending, Ready, Timeout, Disconnected, Overflow, WriteFailed,
  StaleInput, Rejected
};
class Transaction {
  State state_ = State::Idle;
  char reply_[1025] = {};
  std::size_t size_ = 0, discarded_ = 0;
  std::uint32_t started_ = 0, timeout_ = 0, latency_ = 0;
  bool writeAttempted_ = false;
public:
  static constexpr std::size_t BYTE_BUDGET = 64;
  State state() const {return state_;}
  bool pending() const {return state_ == State::Pending;}
  const char *reply() const {return reply_;}
  std::size_t discarded() const {return discarded_;}
  std::uint32_t latency() const {return latency_;}
  bool writeAttempted() const {return writeAttempted_;}
  void reset() {state_=State::Idle;size_=discarded_=0;reply_[0]='\0';writeAttempted_=false;}

  template<class Io>
  bool begin(Io &io, obd_policy::Command command, std::uint32_t now,
             std::uint32_t timeout) {
    if (state_ != State::Idle) return false; // never replace an in-flight reply
    const char *wire = obd_policy::wireCommand(command);
    if (!wire || !obd_policy::allowedWire(command) || timeout == 0 ||
        timeout > 0x7FFFFFFFU) {state_=State::Rejected;return false;}
    if (!io.connected()) {state_=State::Disconnected;return false;}
    for (; discarded_ < BYTE_BUDGET && io.available(); ++discarded_)
      io.read();
    if (io.available()) {
      io.stop();state_=State::StaleInput;return false;
    }
    char frame[12];
    const int n=std::snprintf(frame,sizeof frame,"%s\r",wire);
    if (n<=0 || n>=static_cast<int>(sizeof frame)) {
      state_=State::Rejected;return false;
    }
    started_=now;timeout_=timeout;latency_=0;
    writeAttempted_=true;
    if (io.write(reinterpret_cast<const std::uint8_t *>(frame),
                 static_cast<std::size_t>(n)) != static_cast<std::size_t>(n)) {
      io.stop();state_=State::WriteFailed;return false;
    }
    state_=State::Pending;
    return true;
  }

  template<class Io>
  void poll(Io &io, std::uint32_t now) {
    if (!pending()) return;
    latency_=now-started_; // millis rollover
    if (latency_>=timeout_) {io.stop();state_=State::Timeout;return;}
    for (std::size_t n=0;n<BYTE_BUDGET && io.available();++n) {
      const int c=io.read();
      if (c<0) break;
      if (c=='>') {state_=State::Ready;return;}
      if (size_>=sizeof reply_-1) {io.stop();state_=State::Overflow;return;}
      if (c=='\r' || c=='\n') reply_[size_++]='\n';
      else if (c>=32 && c<=126) reply_[size_++]=static_cast<char>(c);
      reply_[size_]='\0';
    }
    if (!io.connected() && !io.available()) {
      io.stop();state_=State::Disconnected;
    }
  }
};
} // namespace obd_transport
