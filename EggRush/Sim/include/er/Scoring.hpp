#pragma once
// Unified scoring rules (Section 65).
//   score = floor(base * quality * depth * combo * powerup) + clutchBonus
#include <cstdint>

#include "er/Types.hpp"

namespace er::scoring {

// Combo multiplier: +0.25 per 5 up to combo 50, then +0.125 per 5, capped at 8.0.
float comboMultiplier(int combo);

// u = |egg.x - basket.centerX| / (mouth / 2)
CatchQuality classifyQuality(float u);
float qualityMult(CatchQuality q);

struct Depth {
  float mult = 1.f;
  bool clutch = false;
};
// tau = seconds between "first cover" and the egg reaching the mouth line.
Depth depthFromTau(double tauSec);

struct CatchScore {
  int64_t score = 0;
  bool clutch = false;
  float multiplier = 1.f;  // the combo multiplier that was used
};

// clutchBonusAllowed is false once the per-block cap has been used up.
CatchScore scoreCatch(int baseScore, CatchQuality q, double tauSec, int comboAfter,
                      float powerupMult, bool clutchBonusAllowed);

}  // namespace er::scoring
