#pragma once
// The deterministic simulation. No Qt, no GUI, no clocks: World::step() advances
// exactly one fixed step (cfg::kSimStepSec). Same seed + same inputs = same hash.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "er/Config.hpp"
#include "er/Defs.hpp"
#include "er/Events.hpp"
#include "er/FairnessValidator.hpp"
#include "er/Rng.hpp"
#include "er/Types.hpp"

namespace er {

inline constexpr int kMaxEggs = 64;
inline constexpr int kMaxBirds = 8;

struct InputSnapshot {
  int8_t axis = 0;  // -1 left, 0 none, +1 right (keyboard model)
  bool hasMouse = false;
  float mouseX = 0.f;  // logical px; steers by braking-distance control
};

enum class RunStatus : uint8_t { Running, GameOver, Cleared };

struct Egg {
  bool alive = false;
  bool bonus = false;
  bool crossed = false;  // has crossed the mouth line (caught or passed)
  bool softMust = false;  // golden egg planned to be catchable alongside every must-catch egg
  uint8_t rimCount = 0;
  EggType type = EggType::Normal;
  int32_t id = 0;
  int32_t birdId = -1;
  float x = 0, y = 0, prevX = 0, prevY = 0, vx = 0, vy = 0;
  float gravityScale = 1.f;
  double landT = 0.0;        // predicted time the egg crosses the mouth line
  double knownT = 0.0;       // when the telegraph started (when the player could first know)
  double coverStartT = -1.0;  // start of the final unbroken "mouth covers egg" interval
};

struct BasketState {
  float x = cfg::kBasketStartXPx;
  float prevX = cfg::kBasketStartXPx;
  float vx = 0.f;
};

enum class BirdPhase : uint8_t { Idle, Moving, Telegraphing };

struct Plan {
  bool active = false;
  bool converted = false;  // turned into a bonus egg by the fairness repair
  bool softMust = false;   // bonus egg placed so a perfect player can catch it too
  EggType type = EggType::Normal;
  float x = 0.f;
  double releaseT = 0.0;
  double landT = 0.0;
  double knownT = 0.0;  // telegraph start
};

struct Bird {
  int id = 0;
  int spawnIndex = 0;  // index into LevelDef::birds
  float x = 0.f, prevX = 0.f;
  BirdPhase phase = BirdPhase::Idle;
  Plan plan;
  double lastReleaseT = 0.0;
  int dropCount = 0;
  float lastEggX = 0.f;
  bool finished = false;  // no more drops will be planned
};

struct RunResult {
  std::string levelId;
  uint64_t seed = 0;
  Preset preset = Preset::Normal;
  int64_t score = 0;
  int catches = 0;
  int perfect = 0, good = 0, close = 0, clutch = 0, golden = 0;
  int misses = 0;
  int bestCombo = 0;
  float maxMultiplier = 1.f;
  int halfHeartsLost = 0;
  int coins = 0;
  int xp = 0;
  double timeSec = 0.0;
  int repairs = 0;
  int wavesCleared = 0;
  int flawlessWaves = 0;
  int bestPerfectStreak = 0;
  int objectiveProgress = 0;
  bool objectiveMet = false;
  bool cleared = false;
  bool flawless = false;

  float accuracy() const {
    const int total = catches + misses;
    return total > 0 ? static_cast<float>(catches) / static_cast<float>(total) : 1.f;
  }
};

class World {
 public:
  World();

  void setSink(EventSink* sink) { sink_ = sink; }

  // The data/level references must outlive the World only until reset() returns:
  // the World copies what it needs.
  void reset(const GameData& data, const LevelDef& level, Preset preset, uint64_t seed);
  void step(const InputSnapshot& in);

  // ---- read-only state for views, bots and tests -------------------------
  RunStatus status() const { return status_; }
  double timeSec() const { return static_cast<double>(tick_) * cfg::kSimStepSec; }
  uint32_t tick() const { return tick_; }
  const BasketState& basket() const { return basket_; }
  const std::array<Egg, kMaxEggs>& eggs() const { return eggs_; }
  const std::vector<Bird>& birds() const { return birds_; }
  const GameData& data() const { return data_; }
  const LevelDef& level() const { return level_; }
  Preset preset() const { return preset_; }
  int halfHearts() const { return halfHearts_; }
  int maxHalfHearts() const { return maxHalfHearts_; }
  int combo() const { return combo_; }
  int64_t score() const { return result_.score; }
  float forgivenessPx() const { return level_.forgivenessPx; }
  int liveCoins() const { return rawCoins_; }
  int waveIndex() const { return waveIndex_; }
  double comboGraceSec() const { return graceT_; }
  int objectiveProgress() const;
  bool objectiveMet() const { return objectiveMet_; }
  double telegraphSecOf(const Bird& b) const;
  const RunResult& result() const { return result_; }
  uint64_t stateHash() const;
  int aliveEggCount() const;

 private:
  void publish(Event e);
  void stepBasket(const InputSnapshot& in);
  void stepBirds();
  void stepEggs();
  void stepCombo();
  void stepWaves();
  void finishWave();
  void checkObjective();
  void checkEnd();

  void planBird(Bird& b);
  EggType pickEggType();
  void releaseEgg(Bird& b);
  void catchEgg(Egg& e, float dxPx, double crossT);
  void missEgg(Egg& e);
  void breakCombo(int cause);
  void loseHearts(int halves, int birdId);
  void finishRun(bool cleared);
  int eggsPlanned_ = 0;
  int rawCoins_ = 0;

  GameData data_;
  LevelDef level_;
  Preset preset_ = Preset::Normal;
  Rng rng_;
  FairnessValidator validator_;
  std::vector<FairEvent> fairScratch_;
  EventSink* sink_ = nullptr;

  uint32_t tick_ = 0;
  uint32_t seq_ = 0;
  int32_t nextEggId_ = 1;
  RunStatus status_ = RunStatus::Running;
  BasketState basket_;
  std::array<Egg, kMaxEggs> eggs_{};
  std::vector<Bird> birds_;

  int halfHearts_ = 6;
  int maxHalfHearts_ = 10;
  int combo_ = 0;
  double graceT_ = 0.0;
  int clutchBonusesInBlock_ = 0;
  int clutchBlockIndex_ = 0;
  int waveIndex_ = 0;
  int waveHeartsLost_ = 0;
  int waveResolved_ = 0;
  int perfectStreak_ = 0;
  std::array<int, kMaxBirds> birdStreak_{};
  std::array<int, kMaxBirds> bestBirdStreak_{};
  bool objectiveMet_ = false;
  bool forgiveUsed_ = false;
  RunResult result_;
};

}  // namespace er
