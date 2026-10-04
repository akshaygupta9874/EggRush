#pragma once
// A tiny retained-mode UI kit on top of QGraphicsObject: focusable buttons, choice rows,
// sliders, and a Screen base class with keyboard / mouse focus navigation.
#include <QGraphicsObject>
#include <QKeyEvent>
#include <QSizeF>
#include <functional>
#include <vector>

#include "Theme.hpp"

namespace er {

class AppController;

class FocusItem : public QGraphicsObject {
  Q_OBJECT
 public:
  explicit FocusItem(QGraphicsItem* parent = nullptr);
  QRectF boundingRect() const override { return QRectF(QPointF(0, 0), size_); }
  void setSize(QSizeF s) {
    prepareGeometryChange();
    size_ = s;
  }
  QSizeF size() const { return size_; }
  QPointF centerInScene() const { return mapToScene(boundingRect().center()); }

  bool focused() const { return focused_; }
  void setFocused(bool f) {
    focused_ = f;
    update();
  }
  bool locked() const { return locked_; }
  void setLocked(bool l) {
    locked_ = l;
    update();
  }

  virtual void activate() { emit activated(); }
  virtual bool adjust(int /*dir*/) { return false; }  // Left/Right; true if consumed

 signals:
  void activated();
  void focusRequested(er::FocusItem* self);

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* e) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
  bool pressed_ = false;
  QSizeF size_{300, 70};

 private:
  bool focused_ = false;
  bool locked_ = false;
};

class ButtonItem : public FocusItem {
  Q_OBJECT
 public:
  ButtonItem(const QString& text, qreal w, qreal h, bool primary = false, QGraphicsItem* parent = nullptr);
  void setText(const QString& t) {
    text_ = t;
    update();
  }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

 private:
  QString text_;
  bool primary_;
};

class ChoiceItem : public FocusItem {
  Q_OBJECT
 public:
  ChoiceItem(const QString& label, QStringList options, int index, qreal w, qreal h, QGraphicsItem* parent = nullptr);
  int index() const { return index_; }
  void setIndex(int i);
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void activate() override { step(+1); }
  bool adjust(int dir) override {
    step(dir);
    return true;
  }
 signals:
  void changed(int index);

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;

 private:
  void step(int dir);
  QString label_;
  QStringList options_;
  int index_;
};

class SliderItem : public FocusItem {
  Q_OBJECT
 public:
  SliderItem(const QString& label, float value, qreal w, qreal h, QGraphicsItem* parent = nullptr);
  float value() const { return value_; }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void activate() override {}
  bool adjust(int dir) override;
 signals:
  void changed(float value);

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;

 private:
  void setFromX(qreal x);
  QRectF track() const;
  QString label_;
  float value_;
};

// A full-screen layer. The AppController keeps a stack of them; only the top one gets
// input and ticks.
class Screen : public QGraphicsObject {
  Q_OBJECT
 public:
  explicit Screen(AppController& app);
  QRectF boundingRect() const override { return QRectF(0, 0, ui::kW, ui::kH); }
  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}

  virtual void onEnter() {}
  virtual void onLeave() {}
  virtual void tick(double /*dt*/) {}
  virtual bool handleKey(QKeyEvent* e);
  virtual void keyReleased(QKeyEvent*) {}
  virtual void onBack() {}
  // An opaque screen hides every screen below it (no ghost text bleeding through).
  virtual bool opaque() const { return false; }
  virtual void mouseMoved(double /*x*/, double /*y*/) {}
  virtual void windowDeactivated() {}

  void addFocusable(FocusItem* item);
  void setFocusItem(FocusItem* item);
  FocusItem* focusItem() const { return focus_; }

 protected:
  void moveFocus(int dx, int dy);
  AppController& app_;
  std::vector<FocusItem*> items_;
  FocusItem* focus_ = nullptr;
};

}  // namespace er
