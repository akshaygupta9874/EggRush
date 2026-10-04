#include "Art.hpp"

#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

#include "Theme.hpp"
#include "er/Config.hpp"

namespace er::art {

namespace {

using ui::kH;
using ui::kW;

QPainterPath hillPath(double baseY, double amp, double wavelength, double phase) {
  QPainterPath path;
  path.moveTo(-20, kH + 20);
  for (double x = -20; x <= kW + 20; x += 40) {
    const double y = baseY + amp * std::sin(x / wavelength + phase) + amp * 0.45 * std::sin(x / (wavelength * 0.43) + phase * 1.7);
    path.lineTo(x, y);
  }
  path.lineTo(kW + 20, kH + 20);
  path.closeSubpath();
  return path;
}

const QPainterPath& farHills() {
  static const QPainterPath p = hillPath(700, 38, 210, 0.4);
  return p;
}
const QPainterPath& midHills() {
  static const QPainterPath p = hillPath(790, 30, 170, 2.1);
  return p;
}
const QPainterPath& nearHills() {
  static const QPainterPath p = hillPath(880, 24, 140, 4.0);
  return p;
}

void cloud(QPainter* p, double x, double y, double s, const QColor& c) {
  p->setPen(Qt::NoPen);
  p->setBrush(c);
  p->drawEllipse(QPointF(x, y), 70 * s, 34 * s);
  p->drawEllipse(QPointF(x - 55 * s, y + 8 * s), 48 * s, 26 * s);
  p->drawEllipse(QPointF(x + 58 * s, y + 10 * s), 52 * s, 26 * s);
  p->drawEllipse(QPointF(x + 8 * s, y - 20 * s), 44 * s, 30 * s);
}

const QPainterPath& eggShape() {
  static const QPainterPath path = [] {
    QPainterPath e;
    e.moveTo(0, -28);
    e.cubicTo(14, -28, 22, -4, 22, 8);
    e.cubicTo(22, 20, 12, 28, 0, 28);
    e.cubicTo(-12, 28, -22, 20, -22, 8);
    e.cubicTo(-22, -4, -14, -28, 0, -28);
    e.closeSubpath();
    return e;
  }();
  return path;
}

}  // namespace

void drawBackground(QPainter* p, double t, bool hc) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  QLinearGradient sky(0, 0, 0, 760);
  if (hc) {
    sky.setColorAt(0, QColor(0x14, 0x2A, 0x6B));
    sky.setColorAt(1, QColor(0x8F, 0xB8, 0xEA));
  } else {
    sky.setColorAt(0, QColor(0x5F, 0xB9, 0xFF));
    sky.setColorAt(0.65, QColor(0xBF, 0xEA, 0xFF));
    sky.setColorAt(1, QColor(0xFF, 0xF1, 0xC9));
  }
  p->fillRect(QRectF(0, 0, kW, kH), sky);

  if (!hc) {
    QRadialGradient glow(1560, 250, 220);
    glow.setColorAt(0, QColor(255, 244, 190, 230));
    glow.setColorAt(1, QColor(255, 244, 190, 0));
    p->setPen(Qt::NoPen);
    p->setBrush(glow);
    p->drawEllipse(QPointF(1560, 250), 220, 220);
    p->setBrush(QColor(0xFF, 0xE9, 0x7A));
    p->drawEllipse(QPointF(1560, 250), 78, 78);

    const QColor cc(255, 255, 255, 215);
    const double span = kW + 500;
    cloud(p, std::fmod(200 + t * 9, span) - 250, 240, 1.2, cc);
    cloud(p, std::fmod(820 + t * 6, span) - 250, 180, 0.9, QColor(255, 255, 255, 190));
    cloud(p, std::fmod(1300 + t * 12, span) - 250, 330, 1.5, QColor(255, 255, 255, 170));
    cloud(p, std::fmod(1700 + t * 7, span) - 250, 140, 0.8, QColor(255, 255, 255, 200));
    cloud(p, std::fmod(450 + t * 5, span) - 250, 450, 1.0, QColor(255, 255, 255, 140));
  }

  p->setPen(Qt::NoPen);
  p->setBrush(hc ? QColor(0x2E, 0x5E, 0x3A) : QColor(0x93, 0xD9, 0x9B));
  p->drawPath(farHills());

  // A tiny barn on the far hill.
  p->save();
  p->translate(250, 668);
  p->setBrush(hc ? QColor(0x70, 0x20, 0x20) : QColor(0xC8, 0x4B, 0x3C));
  p->setPen(QPen(QColor(0x7A, 0x24, 0x1B), 3));
  p->drawRect(QRectF(-70, -50, 140, 100));
  QPainterPath roof;
  roof.moveTo(-84, -50);
  roof.lineTo(0, -108);
  roof.lineTo(84, -50);
  roof.closeSubpath();
  p->setBrush(QColor(0x7A, 0x3B, 0x2E));
  p->drawPath(roof);
  p->setBrush(QColor(0xF8, 0xEE, 0xD8));
  p->drawRect(QRectF(-26, -4, 52, 54));
  p->drawLine(QPointF(-26, -4), QPointF(26, 50));
  p->drawLine(QPointF(26, -4), QPointF(-26, 50));
  p->restore();

  p->setPen(Qt::NoPen);
  p->setBrush(hc ? QColor(0x24, 0x6B, 0x35) : QColor(0x66, 0xC2, 0x6E));
  p->drawPath(midHills());
  p->setBrush(hc ? QColor(0x1B, 0x55, 0x2A) : QColor(0x45, 0xA8, 0x55));
  p->drawPath(nearHills());

  // Fence behind the basket lane.
  p->setPen(QPen(QColor(0xF5, 0xE6, 0xC8), 2));
  p->setBrush(QColor(0xFB, 0xF1, 0xDA));
  for (double x = 20; x < kW; x += 74) p->drawRoundedRect(QRectF(x, 930, 22, 96), 6, 6);
  p->drawRect(QRectF(0, 958, kW, 14));
  p->drawRect(QRectF(0, 992, kW, 14));

  // Ground.
  QLinearGradient soil(0, 1030, 0, kH);
  soil.setColorAt(0, QColor(0x8A, 0x5A, 0x2E));
  soil.setColorAt(1, QColor(0x5B, 0x37, 0x1B));
  p->setPen(Qt::NoPen);
  p->setBrush(soil);
  p->drawRect(QRectF(0, cfg::kGroundYPx, kW, kH - cfg::kGroundYPx));
  p->setBrush(QColor(0x3F, 0xA3, 0x4D));
  p->drawRect(QRectF(0, cfg::kGroundYPx - 8, kW, 14));
  p->setBrush(QColor(0x5B, 0xC4, 0x66));
  for (double x = 8; x < kW; x += 26) {
    QPainterPath tuft;
    tuft.moveTo(x, cfg::kGroundYPx - 6);
    tuft.lineTo(x + 5, cfg::kGroundYPx - 20 - std::fmod(x * 7.0, 9.0));
    tuft.lineTo(x + 11, cfg::kGroundYPx - 6);
    tuft.closeSubpath();
    p->drawPath(tuft);
  }
  p->restore();
}

void drawPerch(QPainter* p) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  QLinearGradient wood(0, 104, 0, 140);
  wood.setColorAt(0, QColor(0xC8, 0x8E, 0x55));
  wood.setColorAt(1, QColor(0x8F, 0x5E, 0x34));
  p->setPen(QPen(QColor(0x5E, 0x3A, 0x1F), 3));
  p->setBrush(wood);
  p->drawRoundedRect(QRectF(-10, 106, kW + 20, 30), 8, 8);
  p->setPen(QPen(QColor(0x6E, 0x45, 0x25, 160), 2));
  for (double x = 80; x < kW; x += 190) {
    p->drawEllipse(QPointF(x, 121), 9, 5);
  }
  p->setPen(QPen(QColor(0x5E, 0x3A, 0x1F), 3));
  p->setBrush(QColor(0x7A, 0x4E, 0x2B));
  p->drawRect(QRectF(0, 136, 26, 34));
  p->drawRect(QRectF(kW - 26, 136, 26, 34));
  p->restore();
}

void drawHen(QPainter* p, const HenPose& h) {
  struct Style {
    QColor body, belly, wing, comb, beak, outline;
    double scale;
  };
  static const Style styles[3] = {
      {QColor(0xFF, 0xF4, 0xE0), QColor(0xFF, 0xFF, 0xFF), QColor(0xF0, 0xD6, 0xA8), QColor(0xE0, 0x3E, 0x3E),
       QColor(0xFF, 0xA7, 0x26), QColor(0x8A, 0x6A, 0x45), 1.0},
      {QColor(0x7F, 0xD6, 0xE8), QColor(0xD9, 0xF6, 0xFB), QColor(0x3B, 0x9F, 0xBD), QColor(0x2E, 0x6F, 0xC6),
       QColor(0xFF, 0xB7, 0x4D), QColor(0x24, 0x62, 0x78), 0.92},
      {QColor(0xFF, 0xE0, 0x6B), QColor(0xFF, 0xF4, 0xB8), QColor(0xF2, 0xA9, 0x1C), QColor(0xFF, 0x6F, 0x3C),
       QColor(0xFF, 0x8A, 0x3D), QColor(0x9A, 0x6A, 0x0A), 0.85}};
  const Style& s = styles[std::clamp(h.variant, 0, 2)];

  const double bob = h.moving ? std::sin(h.t * 22.0) * 3.0 : std::sin(h.t * 3.2) * 2.0;
  const double squash = h.laying ? 1.0 - 0.10 * std::sin(std::min(1.0, static_cast<double>(h.lay)) * M_PI) : 1.0;
  const double shake = h.laying ? std::sin(h.t * 60.0) * 2.0 * static_cast<double>(h.lay) : 0.0;

  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->translate(static_cast<double>(h.x) + shake, static_cast<double>(h.y) + bob);
  p->scale(h.facing * s.scale, s.scale * squash);
  const QPen outline(s.outline, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  if (h.variant == 2) {  // speed lines
    p->setPen(QPen(QColor(255, 255, 255, 170), 4, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 3; ++i) p->drawLine(QPointF(-70 - i * 14, -8 + i * 14), QPointF(-112 - i * 14, -8 + i * 14));
  }
  // legs
  p->setPen(QPen(QColor(0xE8, 0x93, 0x1B), 5, Qt::SolidLine, Qt::RoundCap));
  const double step = h.moving ? std::sin(h.t * 22.0) * 7.0 : 0.0;
  p->drawLine(QPointF(-14, 28), QPointF(-14 + step, 46));
  p->drawLine(QPointF(14, 28), QPointF(14 - step, 46));
  // tail
  p->setPen(outline);
  p->setBrush(s.wing);
  QPainterPath tail;
  tail.moveTo(-40, 6);
  tail.cubicTo(-78, -30, -92, -6, -64, 16);
  tail.cubicTo(-80, 22, -70, 40, -40, 28);
  p->drawPath(tail);
  // body
  QLinearGradient bg(0, -34, 0, 38);
  bg.setColorAt(0, s.body);
  bg.setColorAt(1, s.belly);
  p->setBrush(bg);
  p->drawEllipse(QPointF(0, 4), 56, 40);
  // wing
  const double flap = h.moving ? std::sin(h.t * 22.0) * 10.0 : std::sin(h.t * 2.4) * 2.0;
  p->setBrush(s.wing);
  QPainterPath wing;
  wing.moveTo(-26, 0);
  wing.cubicTo(-8, -22 - flap, 28, -8 - flap, 30, 16);
  wing.cubicTo(10, 28, -20, 22, -26, 0);
  p->drawPath(wing);
  // head
  p->setBrush(s.body);
  p->drawEllipse(QPointF(42, -26), 24, 22);
  // comb
  p->setBrush(s.comb);
  p->setPen(QPen(s.comb.darker(150), 2.5));
  p->drawEllipse(QPointF(34, -50), 8, 9);
  p->drawEllipse(QPointF(45, -53), 9, 10);
  p->drawEllipse(QPointF(56, -48), 7, 8);
  // beak
  p->setBrush(s.beak);
  p->setPen(QPen(s.beak.darker(160), 2.5));
  QPainterPath beak;
  beak.moveTo(62, -30);
  beak.lineTo(82, -23);
  beak.lineTo(62, -17);
  beak.closeSubpath();
  p->drawPath(beak);
  // wattle
  p->setBrush(s.comb);
  p->drawEllipse(QPointF(62, -9), 5, 8);
  // eye
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0x2B, 0x20, 0x20));
  const double blink = (std::fmod(h.t, 3.7) > 3.58) ? 0.2 : 1.0;
  p->drawEllipse(QPointF(50, -30), 4.5, 5.5 * blink);
  p->setBrush(Qt::white);
  p->drawEllipse(QPointF(51.5, -32), 1.6, 1.6);
  if (h.laying) {  // straining face: cheeks puff, a glowing egg grows underneath
    p->setBrush(QColor(0xFF, 0x8A, 0x8A, 150));
    p->drawEllipse(QPointF(42, -16), 7, 5);
    const double r = 0.35 + 0.65 * static_cast<double>(std::min(1.f, h.lay));
    QRadialGradient glow(0, 52, 46);
    glow.setColorAt(0, QColor(255, 244, 170, static_cast<int>(210 * r)));
    glow.setColorAt(1, QColor(255, 244, 170, 0));
    p->setBrush(glow);
    p->drawEllipse(QPointF(0, 52), 46 * r, 30 * r);
  }
  p->restore();
}

void drawEgg(QPainter* p, EggType type, double spin, double t, bool hc) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->rotate(spin);
  const QPainterPath& e = eggShape();
  switch (type) {
    case EggType::Normal: {
      QLinearGradient g(-18, -24, 18, 26);
      g.setColorAt(0, QColor(0xFF, 0xFB, 0xEF));
      g.setColorAt(1, QColor(0xF2, 0xDC, 0xB0));
      p->setPen(QPen(hc ? QColor(0x10, 0x10, 0x30) : QColor(0xA9, 0x7C, 0x45), hc ? 5.0 : 3.0));
      p->setBrush(g);
      p->drawPath(e);
      p->setPen(Qt::NoPen);
      p->setBrush(QColor(0xC9, 0x9A, 0x62, 150));
      p->drawEllipse(QPointF(-7, 6), 3.2, 2.6);
      p->drawEllipse(QPointF(8, -4), 2.6, 2.2);
      p->drawEllipse(QPointF(5, 14), 2.8, 2.2);
      p->setBrush(QColor(255, 255, 255, 190));
      p->drawEllipse(QPointF(-9, -12), 4, 7);
      break;
    }
    case EggType::Golden: {
      QLinearGradient g(-18, -24, 18, 26);
      g.setColorAt(0, QColor(0xFF, 0xF1, 0x8A));
      g.setColorAt(0.5, QColor(0xFF, 0xCB, 0x2B));
      g.setColorAt(1, QColor(0xE0, 0x9A, 0x00));
      p->setPen(QPen(hc ? QColor(0x10, 0x10, 0x30) : QColor(0x9A, 0x62, 0x00), hc ? 5.0 : 3.0));
      p->setBrush(g);
      p->drawPath(e);
      p->setPen(Qt::NoPen);
      p->setBrush(QColor(255, 255, 255, 220));
      p->drawEllipse(QPointF(-9, -12), 4, 8);
      // twinkling star (shape cue, not just color)
      const double tw = 0.75 + 0.25 * std::sin(t * 9.0);
      p->setBrush(QColor(255, 255, 255, 235));
      p->drawPath(ui::starPath(QPointF(6, 2), 9 * tw, 3 * tw, 4, 0));
      break;
    }
    case EggType::Fast: {
      p->scale(0.86, 1.0);
      QLinearGradient g(-18, -24, 18, 26);
      g.setColorAt(0, QColor(0xE6, 0xF6, 0xFF));
      g.setColorAt(1, QColor(0x8F, 0xCB, 0xF0));
      p->setPen(QPen(hc ? QColor(0x10, 0x10, 0x30) : QColor(0x2F, 0x78, 0xB5), hc ? 5.0 : 3.0));
      p->setBrush(g);
      p->drawPath(e);
      p->setPen(QPen(QColor(0x2F, 0x78, 0xB5), 3, Qt::SolidLine, Qt::RoundCap));
      p->drawLine(QPointF(-10, 2), QPointF(10, -6));  // lightning-ish stripe
      p->drawLine(QPointF(10, -6), QPointF(-6, 12));
      p->setPen(QPen(QColor(255, 255, 255, 190), 3.5, Qt::SolidLine, Qt::RoundCap));
      for (int i = -1; i <= 1; ++i) p->drawLine(QPointF(i * 10.0, -34), QPointF(i * 10.0, -58 - (i == 0 ? 8 : 0)));
      break;
    }
    case EggType::Heavy: {
      p->scale(1.22, 1.22);
      QLinearGradient g(-18, -24, 18, 26);
      g.setColorAt(0, QColor(0xB8, 0x9F, 0x95));
      g.setColorAt(1, QColor(0x6A, 0x4F, 0x46));
      p->setPen(QPen(hc ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x3E, 0x2A, 0x23), hc ? 4.0 : 3.0));
      p->setBrush(g);
      p->drawPath(e);
      p->setPen(QPen(QColor(0x3E, 0x2A, 0x23), 3));
      p->drawLine(QPointF(-21, 4), QPointF(21, 4));
      p->setPen(Qt::NoPen);
      p->setBrush(QColor(0xD9, 0xC9, 0xBF));
      p->drawEllipse(QPointF(-12, 4), 2.6, 2.6);
      p->drawEllipse(QPointF(0, 4), 2.6, 2.6);
      p->drawEllipse(QPointF(12, 4), 2.6, 2.6);
      p->setBrush(QColor(255, 255, 255, 120));
      p->drawEllipse(QPointF(-9, -12), 3.5, 6);
      break;
    }
    case EggType::Count: break;
  }
  p->restore();
}

void drawBasketBack(QPainter* p, const BasketPose& b) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const double x = static_cast<double>(b.x);
  const double top = cfg::kMouthYPx;
  // golden glow
  if (b.glow > 0.01f) {
    QRadialGradient g(x, top, 190);
    g.setColorAt(0, QColor(255, 226, 90, static_cast<int>(150 * b.glow)));
    g.setColorAt(1, QColor(255, 226, 90, 0));
    p->setPen(Qt::NoPen);
    p->setBrush(g);
    p->drawEllipse(QPointF(x, top), 190, 190);
  }
  // inside of the basket (dark), visible behind the front lip
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0x4B, 0x2B, 0x14));
  p->drawEllipse(QPointF(x, top), cfg::kBasketWidthPx * 0.5 - 4, 15);
  p->restore();
}

void drawBasketFront(QPainter* p, const BasketPose& b) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const double x = static_cast<double>(b.x);
  const double top = cfg::kMouthYPx;
  const double bottom = cfg::kGroundYPx;
  p->translate(x, bottom);
  const double sq = static_cast<double>(b.squash);
  p->rotate(static_cast<double>(b.vx) / cfg::kBasketMaxSpeedPxPerSec * 4.0);
  p->scale(1.0 + 0.07 * sq, 1.0 - 0.10 * sq);
  p->translate(-x, -bottom);

  QPainterPath body;
  body.moveTo(x - 110, top);
  body.lineTo(x + 110, top);
  body.cubicTo(x + 108, top + 40, x + 90, bottom - 4, x + 70, bottom);
  body.lineTo(x - 70, bottom);
  body.cubicTo(x - 90, bottom - 4, x - 108, top + 40, x - 110, top);
  body.closeSubpath();

  QLinearGradient wick(x - 110, 0, x + 110, 0);
  wick.setColorAt(0, QColor(0xC9, 0x84, 0x3A));
  wick.setColorAt(0.5, QColor(0xEC, 0xB1, 0x62));
  wick.setColorAt(1, QColor(0xBC, 0x76, 0x30));
  p->setPen(QPen(QColor(0x6A, 0x3C, 0x14), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p->setBrush(wick);
  p->drawPath(body);

  p->save();
  p->setClipPath(body);
  p->setPen(QPen(QColor(0x8A, 0x52, 0x1F, 200), 3));
  for (double y = top + 12; y < bottom; y += 13) p->drawLine(QPointF(x - 115, y), QPointF(x + 115, y));
  int row = 0;
  for (double y = top + 12; y < bottom; y += 13, ++row) {
    for (double xx = x - 120 + (row % 2) * 12; xx < x + 120; xx += 24) {
      p->drawLine(QPointF(xx, y), QPointF(xx, y + 13));
    }
  }
  p->restore();

  // front lip of the rim
  p->setBrush(Qt::NoBrush);
  p->setPen(QPen(QColor(0xF6, 0xCF, 0x8C), 11, Qt::SolidLine, Qt::RoundCap));
  p->drawArc(QRectF(x - 110, top - 15, 220, 30), 180 * 16, 180 * 16);
  p->setPen(QPen(QColor(0x6A, 0x3C, 0x14), 3, Qt::SolidLine, Qt::RoundCap));
  p->drawArc(QRectF(x - 116, top - 20, 232, 40), 180 * 16, 180 * 16);
  p->restore();
}

QPixmap windowIcon() {
  QPixmap pm(128, 128);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  QLinearGradient bg(0, 0, 0, 128);
  bg.setColorAt(0, QColor(0x5F, 0xB9, 0xFF));
  bg.setColorAt(1, QColor(0xFF, 0xF1, 0xC9));
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawRoundedRect(QRectF(4, 4, 120, 120), 26, 26);
  p.translate(64, 52);
  p.scale(1.7, 1.7);
  drawEgg(&p, EggType::Golden, -12, 0.0, false);
  p.resetTransform();
  p.setBrush(QColor(0xE0, 0x9A, 0x4A));
  p.setPen(QPen(QColor(0x6A, 0x3C, 0x14), 4));
  p.drawRoundedRect(QRectF(26, 88, 76, 28), 10, 10);
  return pm;
}

}  // namespace er::art
