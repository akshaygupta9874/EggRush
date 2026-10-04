#pragma once
#include <QPixmap>

#include "AppController.hpp"
#include "Fx.hpp"
#include "UiKit.hpp"

namespace er {

class GameScreen;

class TitleScreen : public Screen {
  Q_OBJECT
 public:
  explicit TitleScreen(AppController& app);
  void onEnter() override;
  void tick(double dt) override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void onBack() override { app_.requestQuit(); }

 private:
  struct Demo {
    float henX = 960, henTarget = 960, eggX = 0, eggY = -100, eggVy = 0, basketX = 960, basketVx = 0, squash = 0;
    double timer = 1.0, lay = 0.0;
    bool falling = false, laying = false;
    int caught = 0;
  } demo_;
  fx::FxLayer* fx_;
  fx::Rand rng_{99};
  double t_ = 0;
};

class LevelSelectScreen : public Screen {
  Q_OBJECT
 public:
  LevelSelectScreen(AppController& app, int focusLevel);
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void onBack() override { app_.goTitle(); }
  void tick(double dt) override { t_ += dt; update(); }

 private:
  std::vector<FocusItem*> cards_;
  ChoiceItem* preset_ = nullptr;
  double t_ = 0;
};

class SettingsScreen : public Screen {
  Q_OBJECT
 public:
  explicit SettingsScreen(AppController& app);
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void onBack() override;
  bool opaque() const override { return true; }
};

class PauseOverlay : public Screen {
  Q_OBJECT
 public:
  PauseOverlay(AppController& app, GameScreen* game);
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void onBack() override;

 private:
  GameScreen* game_;
};

class ResultScreen : public Screen {
  Q_OBJECT
 public:
  ResultScreen(AppController& app, ResultData data);
  void tick(double dt) override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  void onBack() override { app_.goLevelSelect(data_.levelIndex); }
  bool handleKey(QKeyEvent* e) override;
  bool opaque() const override { return true; }

 private:
  ResultData data_;
  double t_ = 0;
  int stage_ = 0;
  fx::FxLayer* fx_;
  ButtonItem* next_ = nullptr;
};

}  // namespace er
