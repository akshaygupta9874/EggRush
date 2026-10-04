#pragma once
// Events are FACTS in the past tense (Section 42 / Section 66). The Sim emits them
// into an EventSink; the game forwards them to Qt signals, tests record them.
#include <cstdint>
#include <cstring>
#include <vector>

#include "er/Types.hpp"

namespace er {

enum class EventType : uint8_t {
  RunStarted,
  EggTelegraphed,  // bird started its lay animation (x, tau = lead seconds)
  EggReleased,
  CatchMade,
  EggRimBounced,
  EggMissed,
  ComboChanged,
  ComboBroken,
  HeartLost,
  HeartGained,
  SpawnRepaired,  // value: 1 = delayed, 3 = converted to a bonus egg; tau = delay seconds
  WaveCleared,    // value = wave index, flags bit0 = flawless, scoreDelta = coin bonus
  ObjectiveMet,   // the level's Objective star was earned
  LevelCleared,
  GameOver,
};

enum CatchFlag : uint32_t {
  kFlagClutch = 1u << 0,
  kFlagRescued = 1u << 1,  // caught after a rim bounce
  kFlagBonusEgg = 1u << 2,
};

struct Event {
  EventType type = EventType::RunStarted;
  uint32_t tick = 0;
  uint32_t seq = 0;
  int32_t eggId = -1;
  int32_t birdId = -1;
  EggType eggType = EggType::Normal;
  CatchQuality quality = CatchQuality::Close;
  uint32_t flags = 0;
  float x = 0.f;
  float y = 0.f;
  int32_t combo = 0;
  float multiplier = 1.f;
  int64_t scoreDelta = 0;
  int32_t value = 0;
  float tau = 0.f;  // seconds of "first cover" lead for catches
};

class EventSink {
 public:
  virtual ~EventSink() = default;
  virtual void onEvent(const Event& e) = 0;
};

// Records every event and keeps a running FNV-1a hash (used by determinism tests).
class EventLog final : public EventSink {
 public:
  void onEvent(const Event& e) override {
    events.push_back(e);
    mix(static_cast<uint64_t>(e.type));
    mix(e.tick);
    mix(static_cast<uint64_t>(static_cast<int64_t>(e.eggId)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(e.birdId)));
    mix(static_cast<uint64_t>(e.eggType));
    mix(static_cast<uint64_t>(e.quality));
    mix(e.flags);
    mix(bits(e.x));
    mix(bits(e.y));
    mix(static_cast<uint64_t>(static_cast<int64_t>(e.combo)));
    mix(static_cast<uint64_t>(e.scoreDelta));
    mix(static_cast<uint64_t>(static_cast<int64_t>(e.value)));
  }

  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += (e.type == t) ? 1 : 0;
    return n;
  }

  std::vector<Event> events;
  uint64_t hash = 1469598103934665603ULL;

 private:
  static uint64_t bits(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    return u;
  }
  void mix(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      hash ^= (v >> (8 * i)) & 0xffu;
      hash *= 1099511628211ULL;
    }
  }
};

}  // namespace er
