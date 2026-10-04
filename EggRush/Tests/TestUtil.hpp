#pragma once
// Shared helpers for the test suite.
#include <QString>

#include "er/Bot.hpp"
#include "er/Content.hpp"
#include "er/World.hpp"

namespace ertest {

inline const er::ContentDb& realContent() {
  static const er::ContentResult res = er::ContentLoader::load(QStringLiteral(ER_DATA_DIR));
  return res.db;
}

// A small hand-built level with one hen. No JSON needed.
inline er::LevelDef basicLevel(float durationSec = 30.f, float interval = 2.0f) {
  er::LevelDef l;
  l.id = "test";
  l.name = "Test";
  l.durationSec = durationSec;
  er::LevelBird b;
  b.intervalSec = interval;
  b.laneMin = 0;
  b.laneMax = er::cfg::kSlotCount - 1;
  l.birds.push_back(b);
  return l;
}

}  // namespace ertest
