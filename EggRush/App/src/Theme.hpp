#pragma once
// Look and feel: colors, fonts and the small drawing helpers every screen shares.
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace er::ui {

inline constexpr float kW = 1920.f;
inline constexpr float kH = 1080.f;

class Theme {
 public:
  static void setHighContrast(bool on) { highContrast_ = on; }
  static bool highContrast() { return highContrast_; }

  static QColor ink() { return QColor(0xFF, 0xF8, 0xE7); }
  static QColor inkDim() { return highContrast_ ? QColor(0xE8, 0xE8, 0xE8) : QColor(0xCF, 0xC6, 0xB0); }
  static QColor shadow() { return QColor(0x1B, 0x16, 0x2B, 200); }
  static QColor panel() { return highContrast_ ? QColor(0x08, 0x08, 0x14, 240) : QColor(0x26, 0x1F, 0x45, 220); }
  static QColor panelEdge() { return highContrast_ ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xFF, 0xD9, 0x8A, 230); }
  static QColor accent() { return QColor(0xFF, 0xB3, 0x47); }
  static QColor accentDark() { return QColor(0xD9, 0x7A, 0x1E); }
  static QColor good() { return QColor(0x7B, 0xE0, 0x8A); }
  static QColor bad() { return QColor(0xFF, 0x6B, 0x6B); }
  static QColor gold() { return QColor(0xFF, 0xD7, 0x3D); }
  static QColor focusRing() { return highContrast_ ? QColor(0xFF, 0xFF, 0x00) : QColor(0xFF, 0xF1, 0xA8); }

  // Font stack: first installed family wins, so the game looks the same on any OS.
  static QFont font(qreal px, bool bold = true);

 private:
  static inline bool highContrast_ = false;
};

void drawShadowText(QPainter* p, const QRectF& r, int flags, const QString& text, const QFont& font,
                    const QColor& color, qreal shadowPx = 3.0);
void drawPanel(QPainter* p, const QRectF& r, qreal radius, const QColor& fill, const QColor& edge,
               qreal edgeWidth = 3.0);
QPainterPath starPath(QPointF c, qreal outer, qreal inner = -1, int points = 5, qreal rotDeg = -90);
QPainterPath heartPath(QPointF c, qreal size);
// fill: 0 = empty outline, 0.5 = half, 1 = full.
void drawHeart(QPainter* p, QPointF c, qreal size, float fill, qreal pulse = 1.0);
void drawStar(QPainter* p, QPointF c, qreal r, bool filled);

}  // namespace er::ui
