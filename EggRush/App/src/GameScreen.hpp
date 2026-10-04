#pragma once
#include <QPixmap>
#include <array>
#include <memory>

#include "AppController.hpp"
#include "Art.hpp"
#include "Fx.hpp"
#include "UiKit.hpp"
#include "er/Bot.hpp"

namespace er {

class WorldView;
class HudView;
class HudTopView;

// One level run: owns the World (the Sim), steps it on a fixed timestep, turns Sim events
// into particles / sound / shake, and draws everything from the Sim's read-only state.
class GameScreen : public Screen, public EventSink {
  Q_OBJECT
 public:
  GameScreen(AppController& app, int levelIndex, Preset preset, uint64_t seed);
  ~GameScreen() override;

  void onEnter() override;
  void onLeave() override;
  void tick(double dt) override;
  bool handleKey(QKeyEvent* e) override;
  void keyReleased(QKeyEvent* e) override;
  void mouseMoved(double x, double y) override;
  void windowDeactivated() override { pause(); }
  void onBack() override { pause(); }
  void onEvent(const Event& e) override;

  void pause();
  void resume();
  void restart();
  void quitToLevels();
  bool paused() const { return paused_; }

  // ---- read by the views --------------------------------------------------------
  const World& world() const { return world_; }
  const LevelDef& level() const { return *level_; }
  double alpha() const { return alpha_; }
  int levelIndex() const { return levelIndex_; }
  double shownScore() const { return shownScore_; }
  float basketSquash() const { return squash_; }
  float basketGlow() const { return glow_; }
  float heartPulse() const { return heartPulse_; }
  float lowHealthPulse() const;
  double viewClock() const { return viewClock_; }
  int tutorialStep() const { return tutorialStep_; }
  float intro() const { return introT_; }
  fx::FxLayer* fx() { return fx_; }
  const QString& toast() const { return toast_; }
  float toastAlpha() const;
  QColor toastColor() const { return toastColor_; }
  bool objectiveFlash() const { return objectiveFlash_ > 0.f; }
  int comboShown() const { return comboShown_; }
  float comboPop() const { return comboPop_; }
  float graceFraction() const;
  AppController& controller() { return app_; }

 private:
  void stepSim(double dt);
  void finishRun();
  void showToast(const QString& t, const QColor& c, float sec = 1.6f);
  InputSnapshot currentInput();
  float mouseXLogical() const { return mouseX_; }

  int levelIndex_;
  const LevelDef* level_;
  Preset preset_;
  uint64_t seed_;
  World world_;
  std::unique_ptr<Bot> bot_;
  bool autoplay_;

  double acc_ = 0.0;
  double alpha_ = 0.0;
  double viewClock_ = 0.0;
  bool paused_ = false;
  bool ending_ = false;
  double endTimer_ = 0.0;
  bool finished_ = false;

  bool leftHeld_ = false, rightHeld_ = false;
  int lastDir_ = 0;
  bool hasMouse_ = false;
  float mouseX_ = 960.f;

  fx::Shake shake_;
  fx::FxLayer* fx_;
  WorldView* worldView_;
  HudView* hud_;
  HudTopView* hudTop_;
  double shownScore_ = 0.0;
  float squash_ = 0.f, glow_ = 0.f, heartPulse_ = 0.f, introT_ = 0.f;
  int tutorialStep_ = 0;  // 0 move hint, 1 center hint, 2 praise, 3 done
  int catchesForTutorial_ = 0;
  QString toast_;
  QColor toastColor_;
  float toastT_ = 0.f, toastMax_ = 1.f;
  float objectiveFlash_ = 0.f;
  int comboShown_ = 0;
  float comboPop_ = 0.f;
  float tutorialTimer_ = 0.f;
  double lowHealthClock_ = 0.0;
  std::array<float, 8> eggSpin_{};
};

}  // namespace er
