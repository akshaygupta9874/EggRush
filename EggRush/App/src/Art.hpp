#pragma once
// Procedural placeholder art (Section 2 / 43: "no external art required to run"). Every
// sprite is vector-drawn with QPainter, so it scales to any window and any DPI. Swap these
// functions for real sprites later without touching the gameplay code.
#include <QPainter>

#include "er/Types.hpp"

namespace er::art {

struct HenPose {
  float x = 0.f;
  float y = 78.f;
  int variant = 0;      // 0 hen, 1 moving bird, 2 fast bird
  double t = 0.0;       // animation clock, seconds
  float lay = 0.f;      // 0..1 progress of the lay telegraph
  int facing = 1;       // +1 right, -1 left
  bool moving = false;
  bool laying = false;
};

struct BasketPose {
  float x = 960.f;
  float vx = 0.f;
  float squash = 0.f;   // 0..1, spikes when an egg lands
  float glow = 0.f;     // 0..1, golden flash
};

void drawBackground(QPainter* p, double timeSec, bool highContrast);
void drawPerch(QPainter* p);
void drawHen(QPainter* p, const HenPose& pose);
// Draws an egg centred on the origin. `spin` in degrees.
void drawEgg(QPainter* p, EggType type, double spin, double timeSec, bool highContrast);
void drawBasketBack(QPainter* p, const BasketPose& pose);
void drawBasketFront(QPainter* p, const BasketPose& pose);
QPixmap windowIcon();

}  // namespace er::art
