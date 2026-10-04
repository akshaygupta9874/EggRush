#pragma once
// Shared kinematics (Section 63). The fairness validator, the bots and level
// design all use these exact functions, so "fair" means the same thing everywhere.
#include <algorithm>
#include <cmath>

#include "er/Config.hpp"

namespace er::kin {

inline constexpr double kV = cfg::kBasketMaxSpeedPxPerSec;
inline constexpr double kA = cfg::kBasketAccelPxPerSec2;
inline constexpr double kSlackPx = cfg::kMouthWidthPx - 2.0 * cfg::kCatchMarginPx;  // 170

// Farthest the basket centre can get from REST in T seconds (accel limited).
inline double reach(double T) {
  if (T <= 0.0) return 0.0;
  const double ta = kV / kA;
  if (T <= ta) return 0.5 * kA * T * T;
  return kV * kV / (2.0 * kA) + kV * (T - ta);
}

// Inverse of reach(): time to travel distance d from rest.
inline double inverseReach(double d) {
  if (d <= 0.0) return 0.0;
  const double da = kV * kV / (2.0 * kA);
  if (d <= da) return std::sqrt(2.0 * d / kA);
  return kV / kA + (d - da) / kV;
}

// Farthest apart two consecutive must-catch eggs may land.
inline double maxDx(double gapSec, double bufferSec) { return kSlackPx + reach(gapSec - bufferSec); }

// Shortest time between two must-catch eggs that land dx px apart.
inline double minGap(double dxPx, double bufferSec) {
  return bufferSec + inverseReach(std::max(0.0, dxPx - kSlackPx));
}

// Time for an egg released at the perch line to reach the mouth line:
// 0.5 * g * t^2 + v0 * t = heightPx, with g = gravity * gravityScale.
inline double fallTime(double gravityScale, double launchVelPxPerSec,
                       double heightPx = cfg::kFallHeightPx) {
  const double g = cfg::kGravityPxPerSec2 * gravityScale;
  const double disc = launchVelPxPerSec * launchVelPxPerSec + 2.0 * g * heightPx;
  return (-launchVelPxPerSec + std::sqrt(disc)) / g;
}

inline double impactSpeed(double gravityScale, double launchVelPxPerSec,
                          double heightPx = cfg::kFallHeightPx) {
  const double g = cfg::kGravityPxPerSec2 * gravityScale;
  return launchVelPxPerSec + g * fallTime(gravityScale, launchVelPxPerSec, heightPx);
}

}  // namespace er::kin
