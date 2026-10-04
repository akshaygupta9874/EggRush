#pragma once
// Headless balance bots (Section 67). A bot can only press left/right (or nothing)
// through the same movement model a player uses, and it only "knows" an egg after
// its telegraph started plus a reaction delay. No teleporting, no cheating.
#include <vector>

#include "er/Rng.hpp"
#include "er/World.hpp"

namespace er {

struct BotProfile {
  const char* name;
  float reactionSec;  // mean reaction delay (lognormal, 20% spread)
  float aimSigmaPx;   // gaussian aim error per target
};

// The oracle sees every telegraph instantly and aims perfectly. If it can fail, the level is unfair.
inline constexpr BotProfile kOracleBot{"oracle", 0.0f, 0.f};
inline constexpr BotProfile kNoviceBot{"novice", 0.40f, 35.f};
inline constexpr BotProfile kAverageBot{"average", 0.25f, 18.f};
inline constexpr BotProfile kExpertBot{"expert", 0.15f, 8.f};

class Bot {
 public:
  Bot(BotProfile profile, uint64_t seed) : profile_(profile), rng_(seed, 99) { targets_.reserve(32); }
  InputSnapshot decide(const World& w);

 private:
  struct Target {
    int64_t key = 0;
    int birdId = -1;
    double landT = 0.0;
    double knownAt = 0.0;
    float x = 0.f;
    float aimX = 0.f;
    bool must = true;
    bool seen = false;
  };
  Target& touch(int64_t key, int birdId, float x, double landT, bool must, double now);
  // Where to stand while waiting for `best`: a point that also catches every egg landing
  // within 0.35 s of it, shifted just far enough toward the next egg to make that one reachable.
  float chooseAim(const Target& best, double now) const;

  BotProfile profile_;
  Rng rng_;
  std::vector<Target> targets_;
};

struct BotRunStats {
  RunResult result;
  int telegraphs = 0;
  int repairs = 0;
};

// Plays one full run headlessly with a bot. maxSeconds guards against endless runs.
BotRunStats simulateRun(const GameData& data, const LevelDef& level, Preset preset, uint64_t seed,
                        const BotProfile& profile, double maxSeconds = 600.0);

// Wilson score lower bound (95%) used by the CI balance gates.
double wilsonLowerBound(int successes, int trials, double z = 1.96);

}  // namespace er
