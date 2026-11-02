// Earliest-deadline-first PID scheduler, driven by a simulated clock that
// mirrors loop(): one request at a time, adapter latency, then a short gap.
#include "ObdPidCatalog.h"
#include <cassert>
#include <cstdint>
using namespace obd_pids;

// Lab simulator bitmaps: every catalog PID is supported.
static const std::uint32_t ALL[3] = {0x9FBB8001u, 0x8001A001u, 0x50000000u};

struct Run {
  unsigned polls[count] = {};
  std::uint32_t minGap[count], maxGap[count] = {}, last[count] = {};
  bool seen[count] = {};
  unsigned total = 0;
  Run() { for (auto &g : minGap) g = UINT32_MAX; }
};

static Run simulate(std::uint32_t start, std::uint32_t durationMs,
                    std::uint32_t latencyMs, const std::uint32_t masks[3]) {
  Scheduler s;
  Run r;
  std::uint32_t now = start;
  while (now - start < durationMs) {
    const int i = s.choose(now, masks);
    if (i < 0) { now += 5 + 25; continue; }   // loop delay + command gap
    if (r.seen[i]) {
      const std::uint32_t gap = now - r.last[i];
      if (gap < r.minGap[i]) r.minGap[i] = gap;
      if (gap > r.maxGap[i]) r.maxGap[i] = gap;
    }
    r.seen[i] = true; r.last[i] = now;
    ++r.polls[i]; ++r.total;
    now += latencyMs + 25;
  }
  return r;
}

int main() {
  const int rpm = indexOf(0x0C), coolant = indexOf(0x05);

  // Never-polled PIDs first, in catalog priority order; then idle bus.
  {
    Scheduler s;
    for (std::size_t k = 0; k < count; ++k) assert(s.choose(1000, ALL) == int(k));
    assert(s.choose(1000, ALL) == -1);
    assert(s.choose(1499, ALL) == -1);
    assert(s.choose(1500, ALL) == rpm);       // RPM interval 500 ms
    s.reset();
    assert(s.choose(1500, ALL) == 0);
  }

  // Typical CAN over WLAN: 40 ms per request. Intervals are honoured
  // and reached; the old fixed 350 ms cadence gave ~2.9 requests/s total.
  {
    const Run r = simulate(1000, 60000, 40, ALL);
    for (std::size_t i = 0; i < count; ++i) {
      assert(r.polls[i] >= 2);
      assert(r.minGap[i] >= catalog[i].intervalMs);
      // Worst case is the connect burst: one request for every PID.
      assert(r.maxGap[i] <= catalog[i].intervalMs + count * (40u + 25u));
    }
    assert(r.polls[rpm] >= 100 && r.polls[rpm] <= 120);   // ~2 Hz
    assert(r.polls[coolant] >= 11 && r.polls[coolant] <= 14);
    assert(r.total >= 450 && r.total <= 560);             // ~8-9/s, capped
  }

  // Medium load (150 ms/request): the most overdue PID wins, so RPM is not
  // diluted to the round-robin share of slow PIDs (old scheduler: 43/min).
  {
    const Run r = simulate(1000, 60000, 150, ALL);
    assert(r.polls[rpm] >= 48);
  }

  // Overloaded slow adapter (K-line/KWP): nothing starves, RPM leads.
  {
    const Run r = simulate(1000, 60000, 300, ALL);
    for (std::size_t i = 0; i < count; ++i) {
      assert(r.polls[i] >= 3);
      assert(r.maxGap[i] <= catalog[i].intervalMs + count * 325u);
      assert(r.polls[rpm] >= r.polls[i]);
    }
  }

  // millis() wraparound in the middle of a session.
  {
    const Run r = simulate(0xFFFFF000u, 20000, 40, ALL);
    assert(r.polls[rpm] >= 34 && r.polls[rpm] <= 41);
    for (std::size_t i = 0; i < count; ++i)
      if (r.seen[i]) assert(r.minGap[i] >= catalog[i].intervalMs);
  }

  // Unsupported PIDs are never requested.
  {
    const std::uint32_t basic[3] = {0x08180000u, 0, 0};  // 05, 0C, 0D
    const Run r = simulate(1000, 10000, 40, basic);
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint8_t pid = catalog[i].pid;
      if (pid != 0x05 && pid != 0x0C && pid != 0x0D) assert(r.polls[i] == 0);
    }
    assert(r.polls[rpm] >= 17);
  }
  return 0;
}
