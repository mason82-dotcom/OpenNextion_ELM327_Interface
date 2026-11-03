#pragma once
// In-process test doubles, never used in firmware builds.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

inline uint32_t clockMs = 0;
inline uint32_t millis() { return clockMs; }
inline void delay(unsigned n) { clockMs += n; }

class String {
  std::string text;
public:
  inline static bool failReserve = false, failConcat = false;
  String() = default;
  String(const char *s): text(s) {}
  String(std::string s): text(std::move(s)) {}
  String(unsigned long n): text(std::to_string(n)) {}
  const char *c_str() const { return text.c_str(); }
  size_t length() const { return text.size(); }
  bool reserve(size_t n) { if (failReserve) return false; text.reserve(n); return true; }
  bool concat(char c) { if (failConcat) return false; text += c; return true; }
  int indexOf(const char *s) const {
    auto p = text.find(s); return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  String substring(size_t a, size_t b) const { return text.substr(a, b-a); }
  bool equals(const char *s) const { return text == s; }
  void replace(char a, char b) { for (auto &c:text) if (c==a) c=b; }
  void trim() {
    auto a=text.find_first_not_of(" \r\n\t");
    text=a==std::string::npos ? "" : text.substr(a,text.find_last_not_of(" \r\n\t")-a+1);
  }
  String &operator+=(const String &s) { text+=s.text; return *this; }
  String &operator+=(char c) { text+=c; return *this; }
  friend String operator+(String a, const String &b) { return a+=b; }
};

struct SerialDouble {
  std::string log;
  explicit operator bool() const { return true; }
  void begin(int) {}
  void end() {}
  void print(const char *s) { log+=s; }
  void println(const char *s) { log+=s; log+='\n'; }
  void println(const String &s) { println(s.c_str()); }
  void printf(const char *fmt, ...) {
    char buffer[2048]; va_list args; va_start(args,fmt);
    vsnprintf(buffer,sizeof buffer,fmt,args); va_end(args); log+=buffer;
  }
};
inline SerialDouble Serial;
