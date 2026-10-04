#pragma once
// Egg Rush: gameplay constants (Section 61 / Section 63 of the master prompt).
// All distances are logical px (1920x1080, +y down), time is seconds.
// Names carry units. If you change a value here, re-run the tests: several
// invariants (Tests/test_invariants.cpp) check that the numbers still agree.

namespace er::cfg {

// ---- logical space -------------------------------------------------------
inline constexpr float kLogicalWidthPx = 1920.f;
inline constexpr float kLogicalHeightPx = 1080.f;
inline constexpr float kPerchYPx = 120.f;    // bird perch line (egg release height)
inline constexpr float kMouthYPx = 980.f;    // basket mouth line (catch line)
inline constexpr float kGroundYPx = 1040.f;  // ground line (miss line)
inline constexpr float kFallHeightPx = kMouthYPx - kPerchYPx;  // 860 px

// ---- basket --------------------------------------------------------------
inline constexpr float kBasketWidthPx = 220.f;
inline constexpr float kMouthWidthPx = 190.f;
inline constexpr float kCatchMarginPx = 10.f;  // fairness margin inside the mouth
inline constexpr float kBasketMinXPx = kBasketWidthPx * 0.5f;                    // 110
inline constexpr float kBasketMaxXPx = kLogicalWidthPx - kBasketWidthPx * 0.5f;  // 1810
inline constexpr float kBasketStartXPx = kLogicalWidthPx * 0.5f;
inline constexpr float kBasketMaxSpeedPxPerSec = 1400.f;
inline constexpr float kBasketAccelPxPerSec2 = 9000.f;
inline constexpr float kBasketDecelPxPerSec2 = 14000.f;
inline constexpr float kTurnAroundAccelMult = 1.4f;

// ---- catch quality (u is measured against the MOUTH half width) -----------
inline constexpr float kPerfectU = 0.22f;
inline constexpr float kGoodU = 0.55f;
inline constexpr float kClutchWindowSec = 0.09f;
inline constexpr float kDefaultForgivenessPx = 8.f;  // invisible, worlds 1-2 only

// ---- eggs and timing -----------------------------------------------------
inline constexpr float kGravityPxPerSec2 = 900.f;
inline constexpr float kRimBounceSpeedPxPerSec = 260.f;
inline constexpr float kRimBounceRestitution = 0.3f;
inline constexpr float kSimStepSec = 1.0f / 120.0f;
inline constexpr double kMaxFrameDtSec = 0.25;  // spiral-of-death clamp

// ---- spawn grid ----------------------------------------------------------
inline constexpr int kSlotCount = 16;
inline constexpr float kSlotPitchPx = 120.f;
inline constexpr float kSlotOffsetPx = 60.f;
inline constexpr float kBirdYPx = 78.f;  // where the bird body is drawn

// ---- combo ---------------------------------------------------------------
inline constexpr float kComboGraceSec = 2.5f;
inline constexpr float kComboMultCap = 8.0f;
inline constexpr int kClutchBonusScore = 50;
inline constexpr int kClutchBonusesPerBlock = 8;
inline constexpr float kClutchBlockSec = 30.f;  // Phase 1 stand-in for "per wave"
inline constexpr float kQualityDepthCap = 3.0f;

// ---- comfort rule (stricter than the validator, never looser) ---------------
// Two must-catch eggs landing within kPairWindowSec of each other must be at most
// kPairMaxDxPx apart. The validator alone would allow up to 170 px, which leaves the
// basket exactly one pixel of freedom: possible for a machine, miserable for a human.
inline constexpr double kPairWindowSec = 0.30;
inline constexpr float kPairMaxDxPx = 100.f;

// ---- misc ----------------------------------------------------------------
inline constexpr float kFirstDropSec = 1.2f;  // PLAY -> first egg released
inline constexpr float kMinTelegraphSec = 0.25f;

}  // namespace er::cfg
