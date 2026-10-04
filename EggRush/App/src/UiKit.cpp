#include "UiKit.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QLinearGradient>
#include <QPainter>
#include <algorithm>
#include <cmath>

#include "AppController.hpp"

namespace er {

using ui::Theme;

// ---- FocusItem ------------------------------------------------------------------
FocusItem::FocusItem(QGraphicsItem* parent) : QGraphicsObject(parent) {
  setAcceptHoverEvents(true);
  setAcceptedMouseButtons(Qt::LeftButton);
}

void FocusItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) { emit focusRequested(this); }

void FocusItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  pressed_ = true;
  emit focusRequested(this);
  update();
  e->accept();
}

void FocusItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
  const bool was = pressed_;
  pressed_ = false;
  update();
  if (was && boundingRect().contains(e->pos())) activate();
}

// ---- ButtonItem -------------------------------------------------------------------
ButtonItem::ButtonItem(const QString& text, qreal w, qreal h, bool primary, QGraphicsItem* parent)
    : FocusItem(parent), text_(text), primary_(primary) {
  size_ = QSizeF(w, h);
}

void ButtonItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = boundingRect().adjusted(4, 4, -4, -4);
  const bool f = focused();
  const double grow = f ? 1.04 : 1.0;
  p->save();
  p->translate(r.center());
  p->scale(pressed_ ? 0.97 : grow, pressed_ ? 0.97 : grow);
  p->translate(-r.center());
  QColor fill, edge, ink;
  if (primary_) {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, locked() ? QColor(0x88, 0x80, 0x70) : Theme::accent().lighter(115));
    g.setColorAt(1, locked() ? QColor(0x66, 0x60, 0x55) : Theme::accentDark());
    p->setPen(QPen(Theme::highContrast() ? Qt::white : QColor(0x7A, 0x3E, 0x08), 4));
    p->setBrush(g);
    ink = QColor(0x2A, 0x16, 0x05);
  } else {
    p->setPen(QPen(Theme::panelEdge(), 3));
    p->setBrush(Theme::panel());
    ink = locked() ? Theme::inkDim().darker(140) : Theme::ink();
  }
  p->drawRoundedRect(r, r.height() * 0.28, r.height() * 0.28);
  if (f) {
    p->setPen(QPen(Theme::focusRing(), 5));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(r.adjusted(-5, -5, 5, 5), r.height() * 0.34, r.height() * 0.34);
  }
  p->setFont(Theme::font(r.height() * 0.42));
  p->setPen(ink);
  p->drawText(r, Qt::AlignCenter, text_);
  p->restore();
}

// ---- ChoiceItem -------------------------------------------------------------------
ChoiceItem::ChoiceItem(const QString& label, QStringList options, int index, qreal w, qreal h, QGraphicsItem* parent)
    : FocusItem(parent), label_(label), options_(std::move(options)), index_(index) {
  size_ = QSizeF(w, h);
}

void ChoiceItem::setIndex(int i) {
  index_ = std::clamp(i, 0, static_cast<int>(options_.size()) - 1);
  update();
}

void ChoiceItem::step(int dir) {
  const int n = static_cast<int>(options_.size());
  if (n == 0) return;
  index_ = (index_ + dir + n) % n;
  update();
  emit changed(index_);
}

void ChoiceItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  emit focusRequested(this);
  step(e->pos().x() < size_.width() * 0.55 && e->pos().x() > size_.width() * 0.45 ? +1 : (e->pos().x() < size_.width() * 0.72 ? -1 : +1));
  e->accept();
}

void ChoiceItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = boundingRect().adjusted(4, 4, -4, -4);
  p->setPen(QPen(focused() ? Theme::focusRing() : Theme::panelEdge(), focused() ? 5 : 3));
  p->setBrush(Theme::panel());
  p->drawRoundedRect(r, r.height() * 0.28, r.height() * 0.28);
  p->setFont(Theme::font(r.height() * 0.40));
  p->setPen(Theme::ink());
  p->drawText(r.adjusted(26, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, label_);
  const QString v = options_.isEmpty() ? QString() : options_[index_];
  p->setPen(focused() ? Theme::gold() : Theme::accent());
  const QString shown = focused() ? QStringLiteral("◀  %1  ▶").arg(v) : v;
  p->drawText(r.adjusted(10, 0, -26, 0), Qt::AlignVCenter | Qt::AlignRight, shown);
}

// ---- SliderItem -------------------------------------------------------------------
SliderItem::SliderItem(const QString& label, float value, qreal w, qreal h, QGraphicsItem* parent)
    : FocusItem(parent), label_(label), value_(std::clamp(value, 0.f, 1.f)) {
  size_ = QSizeF(w, h);
}

QRectF SliderItem::track() const {
  const QRectF r = boundingRect().adjusted(4, 4, -4, -4);
  return QRectF(r.left() + r.width() * 0.46, r.center().y() - 8, r.width() * 0.40, 16);
}

bool SliderItem::adjust(int dir) {
  value_ = std::clamp(value_ + 0.05f * static_cast<float>(dir), 0.f, 1.f);
  update();
  emit changed(value_);
  return true;
}

void SliderItem::setFromX(qreal x) {
  const QRectF t = track();
  value_ = static_cast<float>(std::clamp((x - t.left()) / t.width(), 0.0, 1.0));
  update();
  emit changed(value_);
}

void SliderItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  emit focusRequested(this);
  setFromX(e->pos().x());
  e->accept();
}

void SliderItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e) { setFromX(e->pos().x()); }

void SliderItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = boundingRect().adjusted(4, 4, -4, -4);
  p->setPen(QPen(focused() ? Theme::focusRing() : Theme::panelEdge(), focused() ? 5 : 3));
  p->setBrush(Theme::panel());
  p->drawRoundedRect(r, r.height() * 0.28, r.height() * 0.28);
  p->setFont(Theme::font(r.height() * 0.40));
  p->setPen(Theme::ink());
  p->drawText(r.adjusted(26, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, label_);
  const QRectF t = track();
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0x14, 0x10, 0x28, 200));
  p->drawRoundedRect(t, 8, 8);
  p->setBrush(Theme::accent());
  p->drawRoundedRect(QRectF(t.left(), t.top(), t.width() * static_cast<double>(value_), t.height()), 8, 8);
  p->setBrush(Theme::ink());
  p->setPen(QPen(QColor(0x7A, 0x3E, 0x08), 3));
  p->drawEllipse(QPointF(t.left() + t.width() * static_cast<double>(value_), t.center().y()), 15, 15);
  p->setPen(Theme::inkDim());
  p->setFont(Theme::font(r.height() * 0.34));
  p->drawText(QRectF(t.right() + 28, r.top(), 100, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
              QStringLiteral("%1%").arg(static_cast<int>(std::lround(static_cast<double>(value_) * 100))));
}

// ---- Screen -------------------------------------------------------------------------
Screen::Screen(AppController& app) : app_(app) { setZValue(20); }

void Screen::addFocusable(FocusItem* item) {
  items_.push_back(item);
  connect(item, &FocusItem::focusRequested, this, [this](FocusItem* it) {
    if (focus_ != it) {
      setFocusItem(it);
      app_.audio().play(Sfx::UiMove);
    }
  });
  connect(item, &FocusItem::activated, this, [this]() { app_.audio().play(Sfx::UiConfirm); });
  if (focus_ == nullptr) setFocusItem(item);
}

void Screen::setFocusItem(FocusItem* item) {
  if (focus_ == item) return;
  if (focus_ != nullptr) focus_->setFocused(false);
  focus_ = item;
  if (focus_ != nullptr) focus_->setFocused(true);
}

void Screen::moveFocus(int dx, int dy) {
  if (items_.empty()) return;
  if (focus_ == nullptr) {
    setFocusItem(items_.front());
    return;
  }
  const QPointF c = focus_->centerInScene();
  FocusItem* best = nullptr;
  double bestScore = 1e18;
  for (FocusItem* it : items_) {
    if (it == focus_ || !it->isVisible()) continue;
    const QPointF d = it->centerInScene() - c;
    const double primary = dx != 0 ? d.x() * dx : d.y() * dy;
    const double perp = dx != 0 ? std::fabs(d.y()) : std::fabs(d.x());
    if (primary < 4.0) continue;
    const double score = primary + 2.5 * perp;
    if (score < bestScore) {
      bestScore = score;
      best = it;
    }
  }
  if (best == nullptr) {  // wrap to the far side
    double far = -1e18;
    for (FocusItem* it : items_) {
      if (it == focus_ || !it->isVisible()) continue;
      const QPointF d = it->centerInScene() - c;
      const double primary = dx != 0 ? -d.x() * dx : -d.y() * dy;
      const double perp = dx != 0 ? std::fabs(d.y()) : std::fabs(d.x());
      const double score = primary - 2.5 * perp;
      if (score > far) {
        far = score;
        best = it;
      }
    }
  }
  if (best != nullptr && best != focus_) {
    setFocusItem(best);
    app_.audio().play(Sfx::UiMove);
  }
}

bool Screen::handleKey(QKeyEvent* e) {
  switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
      if (focus_ != nullptr && focus_->adjust(-1)) {
        app_.audio().play(Sfx::UiMove);
        return true;
      }
      moveFocus(-1, 0);
      return true;
    case Qt::Key_Right:
    case Qt::Key_D:
      if (focus_ != nullptr && focus_->adjust(+1)) {
        app_.audio().play(Sfx::UiMove);
        return true;
      }
      moveFocus(+1, 0);
      return true;
    case Qt::Key_Up:
    case Qt::Key_W: moveFocus(0, -1); return true;
    case Qt::Key_Down:
    case Qt::Key_S: moveFocus(0, +1); return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
      if (focus_ != nullptr) {
        focus_->activate();
        return true;
      }
      return false;
    case Qt::Key_Escape:
    case Qt::Key_Backspace:
      app_.audio().play(Sfx::UiBack);
      onBack();
      return true;
    default: break;
  }
  return false;
}

}  // namespace er
