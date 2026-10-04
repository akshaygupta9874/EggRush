#include "er/Scoring.hpp"

#include <algorithm>
#include <cmath>

#include "er/Config.hpp"

namespace er::scoring {

float comboMultiplier(int combo) {
  if (combo < 0) combo = 0;
  float m;
  if (combo <= 50) {
    m = 1.0f + 0.25f * static_cast<float>(combo / 5);
  } else {
    m = 3.5f + 0.125f * static_cast<float>((combo - 50) / 5);
  }
  return std::min(m, cfg::kComboMultCap);
}

CatchQuality classifyQuality(float u) {
  if (u <= cfg::kPerfectU) return CatchQuality::Perfect;
  if (u <= cfg::kGoodU) return CatchQuality::Good;
  return CatchQuality::Close;
}

float qualityMult(CatchQuality q) {
  switch (q) {
    case CatchQuality::Close: return 0.6f;
    case CatchQuality::Good: return 1.0f;
    case CatchQuality::Perfect: return 1.5f;
  }
  return 1.0f;
}

Depth depthFromTau(double tauSec) {
  if (tauSec <= cfg::kClutchWindowSec) return {2.0f, true};
  if (tauSec < 0.25) return {1.5f, false};
  if (tauSec < 0.55) return {1.2f, false};
  return {1.0f, false};
}

CatchScore scoreCatch(int baseScore, CatchQuality q, double tauSec, int comboAfter,
                      float powerupMult, bool clutchBonusAllowed) {
  const Depth d = depthFromTau(tauSec);
  const float qd = std::min(qualityMult(q) * d.mult, cfg::kQualityDepthCap);
  const float cm = comboMultiplier(comboAfter);
  // double + tiny epsilon so 10 * 1.5 * 1.2 * 1.75 style products floor predictably
  const double raw = static_cast<double>(baseScore) * qd * cm * powerupMult;
  CatchScore out;
  out.score = static_cast<int64_t>(std::floor(raw + 1e-6));
  out.clutch = d.clutch;
  out.multiplier = cm;
  if (d.clutch && clutchBonusAllowed) out.score += cfg::kClutchBonusScore;
  return out;
}

}  // namespace er::scoring
