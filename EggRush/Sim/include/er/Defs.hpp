#pragma once
// Plain data definitions. Content/ fills these from JSON; tests build them in code.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "er/Config.hpp"
#include "er/Types.hpp"

namespace er {

struct EggDef {
  std::string id;
  EggType type = EggType::Normal;
  float gravityScale = 1.f;
  float launchVelPxPerSec = 0.f;
  int baseScore = 10;
  int coins = 1;
  float threatUnits = 1.f;
  int missHalfHearts = 1;  // cost in half hearts when missed
  bool bonus = false;      // never validated as must-catch, never costs hearts or combo
};

struct BirdDef {
  std::string id;
  std::string displayName;
  float moveSpeedPxPerSec = 1800.f;
  float telegraphSec = 0.45f;
};

struct LevelBird {
  int birdDef = 0;           // index into GameData::birds
  float intervalSec = 2.6f;  // mean gap between this bird's drops
  float intervalJitter = 0.2f;
  float startOffsetSec = 0.f;
  int laneMin = 0;
  int laneMax = cfg::kSlotCount - 1;
  float maxHopPx = 100000.f;  // max distance between this bird's consecutive eggs
};

struct ScriptedEgg {
  int index = 0;  // 0-based index of the planned egg, over all birds
  EggType type = EggType::Golden;
};

enum class ObjectiveType : uint8_t { None, CatchEggs, PerfectStreak, GoldenCatches, ComboReach, BirdStreak };

// Section 68 "Objective" star. birdIndex is an index into LevelDef::birds (BirdStreak only).
struct LevelObjective {
  ObjectiveType type = ObjectiveType::None;
  int target = 0;
  int birdIndex = 0;
};

struct LevelDef {
  std::string id = "1-1";
  std::string name = "First Catch";
  int world = 1;
  float durationSec = 60.f;
  float firstDropSec = cfg::kFirstDropSec;
  float forgivenessPx = cfg::kDefaultForgivenessPx;
  float coinMultiplier = 1.f;
  double parScore = 0.0;  // filled by `eggrush_bot --calibrate`
  std::vector<LevelBird> birds;
  std::array<float, kEggTypeCount> eggMix{1.f, 0.f, 0.f, 0.f};
  std::vector<ScriptedEgg> scripted;
  LevelObjective objective;
  bool forgiveFirstMiss = false;  // Section 53: the first miss is shown, then healed
};

struct GameData {
  std::array<EggDef, kEggTypeCount> eggs;
  std::vector<BirdDef> birds;
};

// Built-in defaults so the Sim (and its tests) work without any JSON.
inline GameData defaultGameData() {
  GameData d;
  d.eggs[0] = {"normal", EggType::Normal, 1.0f, 0.f, 10, 1, 1.0f, 1, false};
  d.eggs[1] = {"golden", EggType::Golden, 1.0f, 0.f, 100, 10, 0.3f, 0, true};
  d.eggs[2] = {"fast", EggType::Fast, 1.6f, 500.f, 15, 1, 1.5f, 1, false};
  d.eggs[3] = {"heavy", EggType::Heavy, 1.3f, 0.f, 20, 2, 1.4f, 2, false};
  d.birds.push_back({"hen", "Farm Hen", 1800.f, 0.45f});
  d.birds.push_back({"moving", "Moving Bird", 180.f, 0.45f});
  d.birds.push_back({"fast", "Fast Bird", 320.f, 0.45f});
  return d;
}

}  // namespace er
