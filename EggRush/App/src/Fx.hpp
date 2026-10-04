#pragma once
// View-only "juice": particles, floating text and camera shake. None of this feeds back
// into the Sim, so it can never change gameplay or determinism.
#include <QColor>
#include <QGraphicsObject>
#include <QPointF>
#include <QString>
#include <array>
#include <cstdint>

namespace er::fx {

class Shake {
 public:
  void add(float amount) { trauma_ = std::min(1.f, trauma_ + amount); }
  void update(double dt) { trauma_ = std::max(0.f, trauma_ - static_cast<float>(dt) * 1.6f); t_ += dt; }
  void clear() { trauma_ = 0.f; }
  float trauma() const { return trauma_; }
  // Offsets in logical px / degrees for the current frame. `scale` honours the settings.
  QPointF offset(float scale) const;
  float angle(float scale) const;

 private:
  float trauma_ = 0.f;
  double t_ = 0.0;
};

class Rand {
 public:
  explicit Rand(uint32_t seed = 0x1234567u) : s_(seed) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  float uni() { return static_cast<float>(next() >> 8) * (1.f / 16777216.f); }
  float range(float lo, float hi) { return lo + (hi - lo) * uni(); }

 private:
  uint32_t s_;
};

class FxLayer : public QGraphicsObject {
 public:
  explicit FxLayer(QGraphicsItem* parent = nullptr);
  QRectF boundingRect() const override { return QRectF(-200, -200, 2320, 1480); }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

  void setReducedMotion(bool on) { reduced_ = on; }
  void clear();
  void step(double dt);

  // ---- effects --------------------------------------------------------------
  void catchBurst(QPointF at, int quality /*0 close,1 good,2 perfect*/, bool golden, bool clutch);
  void splat(QPointF at, bool heavy);
  void rimSparks(QPointF at);
  void confetti(int count);
  void ring(QPointF at, const QColor& c, float maxR, float life);
  void popup(const QString& text, QPointF at, const QColor& c, int px, float life = 0.9f, float rise = 90.f);

 private:
  enum Shape : uint8_t { Dot, Star, Square, Drop };
  struct Particle {
    bool alive = false;
    uint8_t shape = Dot;
    float x = 0, y = 0, vx = 0, vy = 0, ay = 0, drag = 0;
    float life = 0, maxLife = 1, size = 4, rot = 0, vrot = 0;
    QRgb color = 0xffffffff;
  };
  struct Popup {
    bool alive = false;
    QString text;
    float x = 0, y = 0, rise = 90, life = 0, maxLife = 1;
    int px = 40;
    QColor color;
  };
  struct Ring {
    bool alive = false;
    float x = 0, y = 0, r = 0, maxR = 100, life = 0, maxLife = 0.4f;
    QColor color;
  };
  Particle& slot();
  void spawn(Shape s, QPointF at, float speed, float angleRad, float spread, float life, float size,
             QRgb color, float gravity = 900.f, float drag = 0.f);

  std::array<Particle, 640> parts_{};
  std::array<Popup, 24> popups_{};
  std::array<Ring, 16> rings_{};
  size_t cursor_ = 0, popCursor_ = 0, ringCursor_ = 0;
  Rand rng_{0xC0FFEEu};
  bool reduced_ = false;
};

}  // namespace er::fx
