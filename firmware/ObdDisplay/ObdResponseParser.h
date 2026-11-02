#pragma once
// Narrow SAE J1979 mode 01 decoder for ELM327 text replies.
// Deliberately rejects malformed lines and CAN headers instead of guessing.
// No Arduino types, no dynamic allocation: the same parser is tested on Linux.
#include <cstddef>
#include <cstdint>

namespace obd_response {
struct Frame {
  std::uint8_t data[64] = {};
  std::size_t size = 0;
};

inline int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Each whitespace-delimited token must contain pairs of hex digits.
// Echos ("010C"), "SEARCHING...", "NO DATA", headers and malformed inputs
// cannot be combined to form an apparently valid mode 01 reply.
inline bool decodeLine(const char *text, std::size_t length, Frame &frame) {
  frame.size = 0;
  if (!text || length == 0 || length > 256) return false;
  std::size_t pos = 0;
  while (pos < length) {
    while (pos < length && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    if (pos >= length) break;
    const std::size_t start = pos;
    while (pos < length && text[pos] != ' ' && text[pos] != '\t') {
      if (hexNibble(text[pos]) < 0) return false;
      ++pos;
    }
    const std::size_t digits = pos - start;
    if (digits == 0 || (digits & 1) != 0 ||
        digits / 2 > 64 - frame.size) return false;
    for (std::size_t i = start; i < pos; i += 2) {
      frame.data[frame.size++] = static_cast<std::uint8_t>(
          (hexNibble(text[i]) << 4) | hexNibble(text[i + 1]));
    }
  }
  return frame.size != 0;
}

template <class Visitor>
inline void forEachLineFrame(const char *response, Visitor visit) {
  if (!response) return;
  const char *begin = response;
  const char *cursor = response;
  // Fixed bound even for unterminated input (network layer max 1024).
  for (std::size_t total = 0; total <= 1024; ++total, ++cursor) {
    if (total == 1024 || *cursor == '\0' ||
        *cursor == '\n' || *cursor == '\r') {
      Frame frame;
      if (decodeLine(begin, static_cast<std::size_t>(cursor - begin), frame)) {
        visit(frame);
      }
      if (total == 1024 || *cursor == '\0') break;
      begin = cursor + 1;
    }
  }
}

inline bool readPid(const char *response, std::uint8_t pid,
                    std::uint8_t *out, std::size_t count) {
  if (!out || count == 0 || count > 32) return false;
  bool matched = false;
  forEachLineFrame(response, [&](const Frame &frame) {
    if (matched || frame.size < 2 + count ||
        frame.data[0] != 0x41 || frame.data[1] != pid) return;
    for (std::size_t i = 0; i < count; ++i) out[i] = frame.data[2 + i];
    matched = true;
  });
  return matched;
}

// Multiple ECU responses may report different supported-PID bitmaps.
inline bool supportedPids00(const char *response, std::uint32_t &outMask) {
  outMask = 0;
  bool matched = false;
  forEachLineFrame(response, [&](const Frame &frame) {
    if (frame.size < 6 || frame.data[0] != 0x41 ||
        frame.data[1] != 0x00) return;
    std::uint32_t mask = 0;
    for (int i = 2; i < 6; ++i) mask = (mask << 8) | frame.data[i];
    outMask |= mask;
    matched = true;
  });
  return matched;
}
} // namespace obd_response
