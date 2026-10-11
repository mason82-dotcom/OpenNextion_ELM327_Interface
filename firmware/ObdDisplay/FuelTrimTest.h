#pragma once
// Passive two-phase STFT/LTFT diagnosis, ported from Android FuelTrimTest.
// Pure C++17: no Arduino, ECU writes, actuator control, threads or heap.
// Every sample must be supplied exactly once after a successfully decoded PID.
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace obd_trim {
enum class Stage : std::uint8_t {
  Idle, WaitIdle, CollectIdle, WaitHigh, CollectHigh, Done, Failed
};
static constexpr std::uint32_t IDLE_STABLE_MS=3000;
static constexpr std::uint32_t IDLE_MEASURE_MS=20000;
static constexpr std::uint32_t HIGH_STABLE_MS=3000;
static constexpr std::uint32_t HIGH_MEASURE_MS=15000;
static constexpr std::uint32_t MAX_EXTRA_MS=30000;
static constexpr std::uint32_t FRESH_LIMIT_MS=3500;
static constexpr std::uint16_t MIN_TRIM_SAMPLES=3;
static constexpr std::uint8_t SENSOR_PIDS[6]={0x06,0x07,0x08,0x09,0x10,0x0B};

struct Channel {
  double total=0.0;
  std::uint16_t count=0;
  void clear() {total=0.0;count=0;}
  void add(double value) {if(count<65535){total+=value;++count;}}
  double mean() const {return count ? total/count : NAN;}
};

struct Phase {
  Channel channels[6];
  void clear(){for(auto &c:channels)c.clear();}
  bool enough() const {
    for(std::size_t i=0;i<4;++i)
      if(channels[i].count<MIN_TRIM_SAMPLES)return false;
    return true;
  }
  double mean(std::size_t i) const {
    return i<6 ? channels[i].mean() : NAN;
  }
  double bankTotal(std::size_t bank) const {
    return bank==0 ? mean(0)+mean(1) : mean(2)+mean(3);
  }
};

class Test {
  Stage stage_=Stage::Idle;
  Phase idle_,high_;
  std::uint32_t since_=0,stable_=0,now_=0;
  bool armed_=false;
  double rpm_=NAN,speed_=NAN;
  std::uint32_t rpmAt_=0,speedAt_=0;
  bool rpmKnown_=false,speedKnown_=false;
public:
  Stage stage() const {return stage_;}
  bool running() const {
    return stage_==Stage::WaitIdle || stage_==Stage::CollectIdle ||
           stage_==Stage::WaitHigh || stage_==Stage::CollectHigh;
  }
  const Phase &idle() const {return idle_;}
  const Phase &high() const {return high_;}
  void cancel() {
    stage_=Stage::Idle;idle_.clear();high_.clear();
    armed_=false;rpmKnown_=speedKnown_=false;
  }
  void start() {
    idle_.clear();high_.clear();
    stage_=Stage::WaitIdle;armed_=false;
    // Fresh live readings are re-established by the main polling loop.
    rpmKnown_=speedKnown_=false;
  }
  std::uint32_t elapsed(std::uint32_t now) const {
    return (stage_==Stage::CollectIdle||stage_==Stage::CollectHigh)?
        now-since_:0;
  }
  // Called only after a distinct successful Mode 01 transaction. Thus a
  // previously cached LTFT cannot be counted twice.
  void sample(std::uint8_t pid,double value,std::uint32_t now) {
    if(!running() || !std::isfinite(value))return;
    if(pid==0x0C){rpm_=value;rpmAt_=now;rpmKnown_=true;return;}
    if(pid==0x0D){speed_=value;speedAt_=now;speedKnown_=true;return;}
    if((stage_!=Stage::CollectIdle && stage_!=Stage::CollectHigh) ||
       !inRange(now,stage_==Stage::CollectIdle))return;
    for(std::size_t i=0;i<6;++i){
      if(pid==SENSOR_PIDS[i]){
        (stage_==Stage::CollectIdle?idle_:high_).channels[i].add(value);
        break;
      }
    }
  }
  bool inRange(std::uint32_t now,bool idle) const {
    if(!rpmKnown_ || !speedKnown_ ||
       now-rpmAt_>FRESH_LIMIT_MS || now-speedAt_>FRESH_LIMIT_MS ||
       speed_<0 || speed_>=3)return false;
    return idle ? rpm_>=500 && rpm_<=1100 : rpm_>=2100 && rpm_<=2900;
  }
  std::uint32_t remainingMs(std::uint32_t now) const {
    std::uint32_t goal=0,passed=0;
    switch(stage_){
      case Stage::WaitIdle: case Stage::WaitHigh:
        if(!armed_)return 3000;
        goal=3000;passed=now-stable_;break;
      case Stage::CollectIdle:
        goal=IDLE_MEASURE_MS;passed=now-since_;break;
      case Stage::CollectHigh:
        goal=HIGH_MEASURE_MS;passed=now-since_;break;
      default:return 0;
    }
    return passed<goal ? goal-passed:0;
  }
  bool tick(std::uint32_t now) {
    now_=now;
    const Stage before=stage_;
    if(stage_==Stage::WaitIdle || stage_==Stage::WaitHigh){
      const bool idle=stage_==Stage::WaitIdle;
      const bool stable= inRange(now,idle) &&
          (idle ? rpm_>=550 && rpm_<=1000 : rpm_>=2200 && rpm_<=2800);
      if(!stable){armed_=false;return false;}
      if(!armed_){stable_=now;armed_=true;}
      if(now-stable_>=3000){
        stage_=idle?Stage::CollectIdle:Stage::CollectHigh;
        (idle?idle_:high_).clear();
        since_=now;armed_=false;
      }
    } else if(stage_==Stage::CollectIdle || stage_==Stage::CollectHigh){
      const bool idle=stage_==Stage::CollectIdle;
      if(!inRange(now,idle)){
        (idle?idle_:high_).clear();
        stage_=idle?Stage::WaitIdle:Stage::WaitHigh;
        armed_=false;
        return true;
      }
      const std::uint32_t elapsed=now-since_;
      const std::uint32_t base=idle?IDLE_MEASURE_MS:HIGH_MEASURE_MS;
      Phase &phase=idle?idle_:high_;
      if(elapsed>=base){
        if(phase.enough()) {
          stage_=idle?Stage::WaitHigh:Stage::Done;
          armed_=false;
        } else if(elapsed>=base+MAX_EXTRA_MS){
          stage_=Stage::Failed;
        }
      }
    }
    return stage_!=before;
  }
  // Heuristic indicators from the Android implementation. No diagnosis claim.
  bool bankDifference() const {
    return std::fabs(idle_.bankTotal(0)-idle_.bankTotal(1))>=5.0 ||
           std::fabs(high_.bankTotal(0)-high_.bankTotal(1))>=5.0;
  }
  bool likelyVacuumPattern() const {
    for(std::size_t b=0;b<2;++b)
      if(idle_.bankTotal(b)>10.0 &&
         high_.bankTotal(b)<idle_.bankTotal(b)-5.0)return true;
    return false;
  }
  bool highPositiveBothPhases() const {
    for(std::size_t b=0;b<2;++b)
      if(idle_.bankTotal(b)>10.0 && high_.bankTotal(b)>10.0)return true;
    return false;
  }
  bool negativeLarge() const {
    for(std::size_t b=0;b<2;++b)
      if(idle_.bankTotal(b)<-10.0||high_.bankTotal(b)<-10.0)return true;
    return false;
  }
};
} // namespace obd_trim
