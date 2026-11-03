#pragma once
#include "Arduino.h"
#include <algorithm>
#include <cassert>
#include <deque>
#include <string>
#include <vector>

struct IPAddress { IPAddress() = default; IPAddress(int,int,int,int) {} };
struct WiFiClient {
  bool online = true, shortWrite = false, eofAfterBody = false;
  uint32_t replyAt = 0;
  std::deque<std::string> replies;
  std::vector<std::string> sent;
  std::string rx;
  bool connect(IPAddress,int) { online=true; return true; }
  bool connected() { return online; }
  size_t write(const uint8_t *data, size_t n) {
    assert(rx.empty() && n>1 && data[n-1]=='\r');
    // ELMo must not append LF (which may repeat an ELM command).
    assert(std::count(data,data+n,'\r')==1);
    sent.emplace_back(reinterpret_cast<const char *>(data), n-1);
    if (!replies.empty()) { rx=replies.front(); replies.pop_front(); }
    return shortWrite ? n-1 : n;
  }
  int available() { return clockMs>=replyAt ? static_cast<int>(rx.size()) : 0; }
  int read() {
    if (!available()) return -1;
    unsigned char c=static_cast<unsigned char>(rx.front()); rx.erase(0,1);
    if (eofAfterBody && rx.empty()) online=false;
    return c;
  }
  void stop() { online=false; rx.clear(); }
};
