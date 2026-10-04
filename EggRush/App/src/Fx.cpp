#include "Fx.hpp"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

#include "Theme.hpp"

namespace er::fx {

QPointF Shake::offset(float scale) const {
  const float a = trauma_ * trauma_ * 26.f * scale;
  return QPointF(a * std::sin(t_ * 71.0), a * std::sin(t_ * 83.0 + 1.3));
}

float Shake::angle(float scale) const { return trauma_ * trauma_ * 1.4f * scale * static_cast<float>(std::sin(t_ * 59.0)); }

FxLayer::FxLayer(QGraphicsItem* parent) : QGraphicsObject(parent) { setZValue(60); }

void FxLayer::clear() {
  for (auto& p : parts_) p.alive = false;
  for (auto& p : popups_) p.alive = false;
  for (auto& r : rings_) r.alive = false;
}

FxLayer::Particle& FxLayer::slot() {
  cursor_ = (cursor_ + 1) % parts_.size();  // oldest particle gets recycled when the pool is full
  return parts_[cursor_];
}

void FxLayer::spawn(Shape s, QPointF at, float speed, float angle, float spread, float life, float size,
                    QRgb color, float gravity, float drag) {
  Particle& p = slot();
  const float a = angle + rng_.range(-spread, spread);
  const float v = speed * rng_.range(0.55f, 1.f);
  p.alive = true;
  p.shape = s;
  p.x = static_cast<float>(at.x());
  p.y = static_cast<float>(at.y());
  p.vx = std::cos(a) * v;
  p.vy = std::sin(a) * v;
  p.ay = gravity;
  p.drag = drag;
  p.life = p.maxLife = life * rng_.range(0.7f, 1.2f);
  p.size = size * rng_.range(0.7f, 1.3f);
  p.rot = rng_.range(0.f, 6.28f);
  p.vrot = rng_.range(-9.f, 9.f);
  p.color = color;
}

void FxLayer::catchBurst(QPointF at, int quality, bool golden, bool clutch) {
  const float k = reduced_ ? 0.4f : 1.f;
  const int n = static_cast<int>((8 + quality * 8 + (golden ? 16 : 0) + (clutch ? 10 : 0)) * k);
  const QColor base = golden ? QColor(0xFF, 0xD7, 0x3D) : quality == 2 ? QColor(0xFF, 0xF1, 0x7A) : quality == 1 ? QColor(0xFF, 0xE8, 0xB0) : QColor(0xF2, 0xDC, 0xB0);
  for (int i = 0; i < n; ++i) {
    const Shape s = (golden || quality == 2) && (i % 3 == 0) ? Star : (i % 2 ? Dot : Square);
    spawn(s, at, 420.f + 120.f * static_cast<float>(quality), -1.5708f, 1.3f, 0.65f, 7.f, base.rgba(), 1100.f, 1.2f);
  }
  if (quality == 2) ring(at, QColor(0xFF, 0xF1, 0x7A), 90.f, 0.35f);
  if (clutch) ring(at, QColor(0xFF, 0x8A, 0x3D), 120.f, 0.45f);
}

void FxLayer::splat(QPointF at, bool heavy) {
  const int n = static_cast<int>((heavy ? 26 : 16) * (reduced_ ? 0.5f : 1.f));
  for (int i = 0; i < n; ++i) {
    spawn(Drop, at, 380.f, -1.5708f, 1.1f, 0.7f, heavy ? 9.f : 7.f, QColor(0xFF, 0xC8, 0x2E).rgba(), 1500.f, 0.6f);
  }
  for (int i = 0; i < n / 2; ++i) {
    spawn(Square, at, 300.f, -1.5708f, 1.4f, 0.8f, 5.f, QColor(0xFF, 0xF4, 0xDC).rgba(), 1500.f, 0.4f);
  }
}

void FxLayer::rimSparks(QPointF at) {
  for (int i = 0; i < 8; ++i) spawn(Dot, at, 300.f, -1.5708f, 1.2f, 0.35f, 4.f, QColor(255, 255, 255).rgba(), 800.f, 2.f);
}

void FxLayer::confetti(int count) {
  static const QRgb cols[] = {0xffff6b6b, 0xffffd73d, 0xff7be08a, 0xff6bc5ff, 0xffd58bff};
  const int n = static_cast<int>(static_cast<float>(count) * (reduced_ ? 0.35f : 1.f));
  for (int i = 0; i < n; ++i) {
    spawn(Square, QPointF(rng_.range(0.f, 1920.f), rng_.range(-60.f, 40.f)), 120.f, 1.5708f, 0.6f, 2.8f, 9.f,
          cols[i % 5], 260.f, 0.8f);
  }
}

void FxLayer::ring(QPointF at, const QColor& c, float maxR, float life) {
  Ring& r = rings_[ringCursor_++ % rings_.size()];
  r = Ring{true, static_cast<float>(at.x()), static_cast<float>(at.y()), 8.f, maxR, life, life, c};
}

void FxLayer::popup(const QString& text, QPointF at, const QColor& c, int px, float life, float rise) {
  Popup& p = popups_[popCursor_++ % popups_.size()];
  p.alive = true;
  p.text = text;
  p.x = static_cast<float>(at.x());
  p.y = static_cast<float>(at.y());
  p.rise = rise;
  p.life = p.maxLife = life;
  p.px = px;
  p.color = c;
}

void FxLayer::step(double dtD) {
  const float dt = static_cast<float>(dtD);
  for (Particle& p : parts_) {
    if (!p.alive) continue;
    p.life -= dt;
    if (p.life <= 0.f) {
      p.alive = false;
      continue;
    }
    p.vy += p.ay * dt;
    const float damp = std::max(0.f, 1.f - p.drag * dt);
    p.vx *= damp;
    p.vy *= damp;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.rot += p.vrot * dt;
  }
  for (Popup& p : popups_) {
    if (!p.alive) continue;
    p.life -= dt;
    if (p.life <= 0.f) p.alive = false;
  }
  for (Ring& r : rings_) {
    if (!r.alive) continue;
    r.life -= dt;
    if (r.life <= 0.f) {
      r.alive = false;
      continue;
    }
    const float t = 1.f - r.life / r.maxLife;
    r.r = 8.f + (r.maxR - 8.f) * (1.f - (1.f - t) * (1.f - t));
  }
  update();
}

void FxLayer::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  for (const Ring& r : rings_) {
    if (!r.alive) continue;
    QColor c = r.color;
    c.setAlphaF(std::clamp(r.life / r.maxLife, 0.f, 1.f) * 0.9f);
    p->setPen(QPen(c, 6.0 * static_cast<double>(r.life / r.maxLife) + 1.5));
    p->setBrush(Qt::NoBrush);
    p->drawEllipse(QPointF(r.x, r.y), r.r, r.r * 0.45);
  }
  p->setPen(Qt::NoPen);
  for (const Particle& pt : parts_) {
    if (!pt.alive) continue;
    QColor c = QColor::fromRgba(pt.color);
    c.setAlphaF(std::clamp(pt.life / pt.maxLife * 1.6f, 0.f, 1.f));
    p->setBrush(c);
    const double s = static_cast<double>(pt.size);
    switch (pt.shape) {
      case Dot: p->drawEllipse(QPointF(pt.x, pt.y), s, s); break;
      case Drop: p->drawEllipse(QPointF(pt.x, pt.y), s * 0.8, s * 1.2); break;
      case Square:
        p->save();
        p->translate(pt.x, pt.y);
        p->rotate(static_cast<double>(pt.rot) * 57.3);
        p->drawRect(QRectF(-s, -s * 0.6, s * 2, s * 1.2));
        p->restore();
        break;
      case Star: p->drawPath(ui::starPath(QPointF(pt.x, pt.y), s * 1.5, s * 0.6, 4, static_cast<double>(pt.rot) * 57.3)); break;
    }
  }
  for (const Popup& pp : popups_) {
    if (!pp.alive) continue;
    const float t = 1.f - pp.life / pp.maxLife;
    const float pop = t < 0.15f ? 0.6f + 0.55f * (t / 0.15f) : 1.15f - 0.15f * std::min(1.f, (t - 0.15f) / 0.2f);
    QColor c = pp.color;
    c.setAlphaF(std::clamp(pp.life / (pp.maxLife * 0.35f), 0.f, 1.f));
    p->save();
    p->translate(static_cast<double>(pp.x), static_cast<double>(pp.y - pp.rise * (1.f - (1.f - t) * (1.f - t))));
    p->scale(static_cast<double>(pop), static_cast<double>(pop));
    QFont f = ui::Theme::font(pp.px);
    p->setFont(f);
    QColor sh = ui::Theme::shadow();
    sh.setAlphaF(c.alphaF() * 0.8);
    p->setPen(sh);
    p->drawText(QRectF(-300 + 3, -40 + 3, 600, 80), Qt::AlignCenter, pp.text);
    p->setPen(c);
    p->drawText(QRectF(-300, -40, 600, 80), Qt::AlignCenter, pp.text);
    p->restore();
  }
}

}  // namespace er::fx
