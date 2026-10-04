#include "Theme.hpp"

#include <QFontDatabase>
#include <QLinearGradient>
#include <QStringList>
#include <cmath>

namespace er::ui {

QFont Theme::font(qreal px, bool bold) {
  static QStringList families;
  if (families.isEmpty()) {
    const QStringList wanted = {"Poppins", "Inter", "Segoe UI", "Helvetica Neue", "Noto Sans", "DejaVu Sans",
                                "Arial"};
    const QStringList have = QFontDatabase::families();
    for (const QString& f : wanted) {
      if (have.contains(f)) families << f;
    }
    if (families.isEmpty()) families << QStringLiteral("sans-serif");
  }
  QFont f;
  f.setFamilies(families);
  f.setPixelSize(static_cast<int>(std::lround(px)));
  f.setWeight(bold ? QFont::Bold : QFont::Medium);
  f.setHintingPreference(QFont::PreferNoHinting);
  return f;
}

void drawShadowText(QPainter* p, const QRectF& r, int flags, const QString& text, const QFont& font,
                    const QColor& color, qreal shadowPx) {
  p->save();
  p->setFont(font);
  if (shadowPx > 0) {
    p->setPen(Theme::shadow());
    p->drawText(r.translated(shadowPx, shadowPx), flags, text);
  }
  p->setPen(color);
  p->drawText(r, flags, text);
  p->restore();
}

void drawPanel(QPainter* p, const QRectF& r, qreal radius, const QColor& fill, const QColor& edge,
               qreal edgeWidth) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(QPen(edge, edgeWidth));
  p->setBrush(fill);
  p->drawRoundedRect(r, radius, radius);
  p->restore();
}

QPainterPath starPath(QPointF c, qreal outer, qreal inner, int points, qreal rotDeg) {
  if (inner < 0) inner = outer * 0.45;
  QPainterPath path;
  const qreal rot = rotDeg * M_PI / 180.0;
  for (int i = 0; i < points * 2; ++i) {
    const qreal r = (i % 2 == 0) ? outer : inner;
    const qreal a = rot + i * M_PI / points;
    const QPointF pt(c.x() + std::cos(a) * r, c.y() + std::sin(a) * r);
    if (i == 0) path.moveTo(pt);
    else path.lineTo(pt);
  }
  path.closeSubpath();
  return path;
}

QPainterPath heartPath(QPointF c, qreal s) {
  // Two arcs and a point; s is the full width.
  QPainterPath path;
  const qreal w = s, h = s * 0.92;
  const qreal x = c.x() - w / 2, y = c.y() - h / 2;
  path.moveTo(x + w / 2, y + h);
  path.cubicTo(x - w * 0.15, y + h * 0.55, x + w * 0.02, y - h * 0.05, x + w / 2, y + h * 0.28);
  path.cubicTo(x + w * 0.98, y - h * 0.05, x + w * 1.15, y + h * 0.55, x + w / 2, y + h);
  path.closeSubpath();
  return path;
}

void drawHeart(QPainter* p, QPointF c, qreal size, float fill, qreal pulse) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->translate(c);
  p->scale(pulse, pulse);
  const QPainterPath path = heartPath(QPointF(0, 0), size);
  // empty slot
  p->setPen(QPen(QColor(0xFF, 0xF8, 0xE7, 230), size * 0.07));
  p->setBrush(QColor(0x2A, 0x14, 0x2E, 170));
  p->drawPath(path);
  if (fill > 0.f) {
    p->save();
    const qreal left = -size / 2;
    p->setClipRect(QRectF(left, -size, size * static_cast<qreal>(fill), size * 2));
    QLinearGradient g(0, -size / 2, 0, size / 2);
    g.setColorAt(0, QColor(0xFF, 0x8A, 0x8A));
    g.setColorAt(1, QColor(0xE0, 0x2B, 0x4B));
    p->setPen(Qt::NoPen);
    p->setBrush(g);
    p->drawPath(path);
    p->restore();
    // shine
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 120));
    p->drawEllipse(QPointF(-size * 0.22, -size * 0.14), size * 0.09, size * 0.06);
  }
  p->restore();
}

void drawStar(QPainter* p, QPointF c, qreal r, bool filled) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const QPainterPath path = starPath(c, r);
  if (filled) {
    QLinearGradient g(c.x(), c.y() - r, c.x(), c.y() + r);
    g.setColorAt(0, QColor(0xFF, 0xF0, 0x7A));
    g.setColorAt(1, QColor(0xFF, 0xB0, 0x20));
    p->setBrush(g);
    p->setPen(QPen(QColor(0x9A, 0x5B, 0x00), r * 0.1));
  } else {
    p->setBrush(QColor(0x1B, 0x16, 0x2B, 140));
    p->setPen(QPen(QColor(0xFF, 0xF8, 0xE7, 160), r * 0.1));
  }
  p->drawPath(path);
  p->restore();
}

}  // namespace er::ui
