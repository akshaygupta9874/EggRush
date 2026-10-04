#include "GameScreen.hpp"

#include <QGraphicsScene>
#include <QLocale>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

#include "Screens.hpp"
#include "Text.hpp"
#include "er/Kinematics.hpp"
#include "er/Scoring.hpp"

namespace er {

using ui::kH;
using ui::kW;
using ui::drawHeart;
using ui::drawPanel;
using ui::drawShadowText;
using ui::Theme;

// ============================================================================
// WorldView: everything that lives inside the play field (shaken as one piece).
// ============================================================================
class WorldView : public QGraphicsObject {
 public:
  explicit WorldView(GameScreen* gs) : gs_(gs) { setZValue(10); }
  QRectF boundingRect() const override { return QRectF(-100, -100, kW + 200, kH + 200); }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    const World& w = gs_->world();
    const double alpha = gs_->alpha();
    const double now = w.timeSec();
    const bool hc = Theme::highContrast();
    const double clock = gs_->viewClock();

    art::drawPerch(p);

    // ---- landing markers (a calm "where will it land" cue) ----------------------
    auto marker = [&](float x, double landT, EggType type) {
      const double rem = landT - now;
      if (rem < 0.0 || rem > 1.6) return;
      const double k = 1.0 - rem / 1.6;  // 0 far .. 1 about to land
      QColor c = type == EggType::Golden ? QColor(0xFF, 0xD7, 0x3D) : type == EggType::Fast ? QColor(0x6B, 0xC5, 0xFF)
                 : type == EggType::Heavy ? QColor(0xFF, 0x8A, 0x65) : QColor(255, 255, 255);
      c.setAlphaF(0.18 + 0.55 * k);
      const double r = 70.0 - 40.0 * k;
      p->setPen(QPen(c, hc ? 6.0 : 4.0));
      p->setBrush(Qt::NoBrush);
      p->drawEllipse(QPointF(static_cast<double>(x), static_cast<double>(cfg::kMouthYPx) + 2), r, r * 0.28);
    };
    for (const Bird& b : w.birds()) {
      if (b.phase == BirdPhase::Telegraphing && b.plan.active) marker(b.plan.x, b.plan.landT, b.plan.type);
    }
    for (const Egg& e : w.eggs()) {
      if (e.alive && !e.crossed) marker(e.x, e.landT, e.type);
    }

    // ---- birds -------------------------------------------------------------------
    for (const Bird& b : w.birds()) {
      art::HenPose pose;
      const LevelBird& lb = w.level().birds[static_cast<size_t>(b.spawnIndex)];
      pose.variant = std::min(lb.birdDef, 2);
      pose.x = b.prevX + (b.x - b.prevX) * static_cast<float>(alpha);
      pose.y = cfg::kBirdYPx;
      pose.t = clock + b.id * 0.7;
      pose.moving = b.phase == BirdPhase::Moving;
      pose.laying = b.phase == BirdPhase::Telegraphing;
      if (pose.laying && b.plan.active) {
        const double tel = std::max(0.05, w.telegraphSecOf(b));
        pose.lay = static_cast<float>(std::clamp(1.0 - (b.plan.releaseT - now) / tel, 0.0, 1.0));
      }
      if (b.plan.active && std::fabs(b.plan.x - pose.x) > 2.f) pose.facing = b.plan.x > pose.x ? 1 : -1;
      else pose.facing = (b.id % 2 == 0) ? 1 : -1;
      art::drawHen(p, pose);
    }

    // ---- basket (back), eggs, basket (front) --------------------------------------
    art::BasketPose bp;
    const BasketState& bs = w.basket();
    bp.x = bs.prevX + (bs.x - bs.prevX) * static_cast<float>(alpha);
    bp.vx = bs.vx;
    bp.squash = gs_->basketSquash();
    bp.glow = gs_->basketGlow();
    art::drawBasketBack(p, bp);

    for (const Egg& e : w.eggs()) {
      if (!e.alive) continue;
      const double x = static_cast<double>(e.prevX + (e.x - e.prevX) * static_cast<float>(alpha));
      const double y = static_cast<double>(e.prevY + (e.y - e.prevY) * static_cast<float>(alpha));
      const double spin = std::sin(clock * 5.0 + e.id) * 9.0 + (e.rimCount > 0 ? static_cast<double>(e.vx) * 0.25 : 0.0);
      p->save();
      p->translate(x, y);
      art::drawEgg(p, e.type, spin, clock, hc);
      p->restore();
    }
    art::drawBasketFront(p, bp);

    // ---- tutorial helpers ---------------------------------------------------------
    if (gs_->tutorialStep() == 1) {
      // Highlight the Perfect zone: 22% of the mouth half-width on each side of centre.
      const double half = static_cast<double>(cfg::kPerfectU * cfg::kMouthWidthPx * 0.5f);
      const double pulse = 0.5 + 0.5 * std::sin(clock * 6.0);
      p->setPen(QPen(QColor(0xFF, 0xF1, 0x7A, static_cast<int>(120 + 120 * pulse)), 5));
      p->setBrush(QColor(0xFF, 0xF1, 0x7A, static_cast<int>(40 + 40 * pulse)));
      p->drawRoundedRect(QRectF(static_cast<double>(bp.x) - half, static_cast<double>(cfg::kMouthYPx) - 22, half * 2, 44), 12, 12);
    }
  }

 private:
  GameScreen* gs_;
};

// ============================================================================
// HudTopView: a slim strip (score | level + timer | hearts). It sits BELOW the world
// layer on purpose: the hens perch right under it, and they must stay visible when they
// walk across the strip.
// ============================================================================
class HudTopView : public QGraphicsObject {
 public:
  explicit HudTopView(GameScreen* gs) : gs_(gs) { setZValue(5); }
  QRectF boundingRect() const override { return QRectF(0, 0, kW, 70); }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    const World& w = gs_->world();
    const LevelDef& lvl = gs_->level();

    drawPanel(p, QRectF(-30, -40, kW + 60, 98), 30, Theme::panel(), Theme::panelEdge(), 3);

    // score
    drawShadowText(p, QRectF(172, 4, 110, 50), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SCORE"), Theme::font(22), Theme::inkDim(), 0);
    drawShadowText(p, QRectF(284, 0, 250, 56), Qt::AlignLeft | Qt::AlignVCenter,
                   QLocale(QLocale::English).toString(static_cast<qint64>(std::llround(gs_->shownScore()))), Theme::font(46), Theme::gold(), 3);

    // level name + wave timer
    const double total = static_cast<double>(lvl.durationSec);
    const double frac = std::clamp(w.timeSec() / total, 0.0, 1.0);
    const qreal x0 = kW / 2 - 412;
    const QFontMetricsF fm(Theme::font(26));
    drawShadowText(p, QRectF(x0, 2, 340, 54), Qt::AlignRight | Qt::AlignVCenter,
                   fm.elidedText(QStringLiteral("%1  ·  %2").arg(QString::fromStdString(lvl.id), QString::fromStdString(lvl.name)), Qt::ElideRight, 336),
                   Theme::font(26), Theme::ink(), 0);
    const QRectF bar(x0 + 360, 21, 340, 16);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0x14, 0x10, 0x28, 220));
    p->drawRoundedRect(bar, 8, 8);
    p->setBrush(Theme::accent());
    p->drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * frac, bar.height()), 8, 8);
    p->setPen(QPen(QColor(255, 255, 255, 150), 2));
    for (double t = cfg::kClutchBlockSec; t < total; t += static_cast<double>(cfg::kClutchBlockSec)) {
      const double x = bar.left() + bar.width() * (t / total);
      p->drawLine(QPointF(x, bar.top() - 2), QPointF(x, bar.bottom() + 2));
    }
    drawShadowText(p, QRectF(bar.right() + 16, 2, 130, 54), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("WAVE %1").arg(w.waveIndex() + 1),
                   Theme::font(22), Theme::inkDim(), 0);

    // hearts
    const int heartSlots = w.maxHalfHearts() / 2;
    const double hs = 44, gap = 6;
    const double totalW = heartSlots * hs + (heartSlots - 1) * gap;
    const double hx = kW - 170 - totalW;  // keep clear of the outermost hen lanes
    for (int i = 0; i < heartSlots; ++i) {
      const int halves = std::clamp(w.halfHearts() - 2 * i, 0, 2);
      const double pulse = 1.0 + 0.28 * static_cast<double>(gs_->heartPulse());
      drawHeart(p, QPointF(hx + hs / 2 + i * (hs + gap), 29), hs, static_cast<float>(halves) * 0.5f, pulse);
    }
  }

 private:
  GameScreen* gs_;
};

// ============================================================================
// HudView: score, hearts, combo, timer, objective, toasts, banners.
// ============================================================================
class HudView : public QGraphicsObject {
 public:
  explicit HudView(GameScreen* gs) : gs_(gs) { setZValue(30); }
  QRectF boundingRect() const override { return QRectF(0, 0, kW, kH); }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    const World& w = gs_->world();
    const LevelDef& lvl = gs_->level();
    AppController& app = gs_->controller();
    const double clock = gs_->viewClock();

    // low health vignette (pulsing pattern, not just a colour)
    const float lh = gs_->lowHealthPulse();
    if (lh > 0.f) {
      QRadialGradient g(kW / 2, kH / 2, kW * 0.62);
      g.setColorAt(0.62, QColor(0xE0, 0x1E, 0x3C, 0));
      g.setColorAt(1.0, QColor(0xE0, 0x1E, 0x3C, static_cast<int>(120.f * lh)));
      p->setPen(Qt::NoPen);
      p->setBrush(g);
      p->drawRect(QRectF(0, 0, kW, kH));
    }

    // ---- combo ----------------------------------------------------------------------------
    const int combo = w.combo();
    if (combo >= 2) {
      const double pop = 1.0 + 0.25 * static_cast<double>(gs_->comboPop());
      p->save();
      p->translate(kW / 2, 190);
      p->scale(pop, pop);
      drawShadowText(p, QRectF(-300, -50, 600, 80), Qt::AlignCenter, QStringLiteral("COMBO  %1").arg(combo), Theme::font(66), Theme::ink(), 4);
      const float mult = scoring::comboMultiplier(combo);
      drawShadowText(p, QRectF(-300, 24, 600, 40), Qt::AlignCenter, QStringLiteral("x%1").arg(static_cast<double>(mult), 0, 'g', 4), Theme::font(36), Theme::gold(), 2);
      const QRectF gb(-130, 70, 260, 12);
      p->setPen(Qt::NoPen);
      p->setBrush(QColor(0x14, 0x10, 0x28, 200));
      p->drawRoundedRect(gb, 6, 6);
      p->setBrush(gs_->graceFraction() > 0.3f ? Theme::good() : Theme::bad());
      p->drawRoundedRect(QRectF(gb.left(), gb.top(), gb.width() * static_cast<double>(gs_->graceFraction()), gb.height()), 6, 6);
      p->restore();
    }

    // ---- objective chip (bottom-left) -----------------------------------------------------
    if (lvl.objective.type != ObjectiveType::None) {
      const int prog = std::min(w.objectiveProgress(), lvl.objective.target);
      const bool done = w.objectiveMet();
      const QString text = QStringLiteral("%1 %2   %3/%4").arg(done ? QStringLiteral("✔") : QStringLiteral("★"),
                                                              objectiveText(app.content(), lvl)).arg(prog).arg(lvl.objective.target);
      const double flash = gs_->objectiveFlash() ? 0.5 + 0.5 * std::sin(clock * 14.0) : 0.0;
      QFontMetricsF fm(Theme::font(26));
      const double w2 = fm.horizontalAdvance(text) + 56;
      drawPanel(p, QRectF(24, kH - 76, w2, 56), 20, Theme::panel(), done ? Theme::good() : Theme::panelEdge(), 3 + 3 * flash);
      drawShadowText(p, QRectF(48, kH - 76, w2 - 30, 56), Qt::AlignLeft | Qt::AlignVCenter, text, Theme::font(26), done ? Theme::good() : Theme::ink(), 0);
    }

    // ---- tutorial banner --------------------------------------------------------------------
    const int ts = gs_->tutorialStep();
    if (ts >= 0 && ts <= 2) {
      QString msg;
      if (ts == 0) msg = QStringLiteral("Move with ← → (or A / D) or the mouse to catch the eggs");
      else if (ts == 1) msg = QStringLiteral("Center the egg in the glow for a PERFECT catch!");
      else msg = QStringLiteral("Nice! Keep catching to build a combo.");
      const double bob = std::sin(clock * 4.0) * 5;
      QFontMetricsF fm(Theme::font(36));
      const double bw = fm.horizontalAdvance(msg) + 90;
      drawPanel(p, QRectF(kW / 2 - bw / 2, 790 + bob, bw, 84), 28, QColor(0x26, 0x1F, 0x45, 235), Theme::accent(), 4);
      drawShadowText(p, QRectF(kW / 2 - bw / 2, 790 + bob, bw, 84), Qt::AlignCenter, msg, Theme::font(36), Theme::ink(), 0);
      if (ts == 0) {
        const double a = std::sin(clock * 5.0) * 26;
        p->setPen(QPen(Theme::gold(), 8, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(kW / 2 - 150 - a, 900), QPointF(kW / 2 - 210 - a, 900));
        p->drawLine(QPointF(kW / 2 + 150 + a, 900), QPointF(kW / 2 + 210 + a, 900));
      }
    }

    // ---- toast ------------------------------------------------------------------------------------
    if (gs_->toastAlpha() > 0.f) {
      QColor c = gs_->toastColor();
      c.setAlphaF(static_cast<qreal>(gs_->toastAlpha()));
      QColor sh = Theme::shadow();
      sh.setAlphaF(static_cast<qreal>(gs_->toastAlpha()) * 0.8);
      const double rise = (1.f - gs_->toastAlpha()) * 30.0;
      p->setFont(Theme::font(78));
      p->setPen(sh);
      p->drawText(QRectF(0 + 4, 300 + 4 - rise, kW, 110), Qt::AlignCenter, gs_->toast());
      p->setPen(c);
      p->drawText(QRectF(0, 300 - rise, kW, 110), Qt::AlignCenter, gs_->toast());
    }

    // ---- intro banner -------------------------------------------------------------------------------
    const float it = gs_->intro();
    if (it < 2.6f) {
      const double in = std::min(1.0, static_cast<double>(it) / 0.35);
      const double out = it > 2.0f ? 1.0 - static_cast<double>(it - 2.0f) / 0.6 : 1.0;
      const double a = std::clamp(std::min(in, out), 0.0, 1.0);
      p->save();
      p->setOpacity(a);
      p->translate((1.0 - in) * -400.0, 0);
      drawPanel(p, QRectF(kW / 2 - 420, 430, 840, 150), 36, QColor(0x26, 0x1F, 0x45, 235), Theme::accent(), 4);
      drawShadowText(p, QRectF(kW / 2 - 420, 440, 840, 80), Qt::AlignCenter,
                     QStringLiteral("%1  %2").arg(QString::fromStdString(lvl.id), QString::fromStdString(lvl.name)), Theme::font(58), Theme::gold(), 3);
      drawShadowText(p, QRectF(kW / 2 - 420, 515, 840, 50), Qt::AlignCenter,
                     QStringLiteral("%1  ·  Objective: %2").arg(presetName(w.preset()), objectiveText(app.content(), lvl)), Theme::font(26, false), Theme::ink(), 0);
      p->restore();
    }

    // ---- fps / debug ------------------------------------------------------------------------------------
    if (app.profile().settings.showFps || app.debugOverlay()) {
      QString t = QStringLiteral("%1 fps").arg(app.fps(), 0, 'f', 0);
      if (app.debugOverlay()) {
        t += QStringLiteral("   tick %1   eggs %2   repairs %3   seed %4   hash %5")
                 .arg(w.tick()).arg(w.aliveEggCount()).arg(w.result().repairs).arg(w.result().seed).arg(w.stateHash(), 0, 16);
      }
      drawShadowText(p, QRectF(24, 150, 1400, 30), Qt::AlignLeft | Qt::AlignVCenter, t, Theme::font(22, false), Theme::inkDim(), 2);
    }
    if (gs_->controller().options().autoplay) {
      drawShadowText(p, QRectF(kW - 424, kH - 44, 400, 36), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("AUTOPLAY (bot)"), Theme::font(24), Theme::inkDim(), 2);
    }
  }

 private:
  GameScreen* gs_;
};

// ============================================================================
// GameScreen
// ============================================================================
GameScreen::GameScreen(AppController& app, int levelIndex, Preset preset, uint64_t seed)
    : Screen(app), levelIndex_(levelIndex), preset_(preset), seed_(seed), autoplay_(app.options().autoplay) {
  setZValue(10);
  level_ = &app.content().levels[static_cast<size_t>(levelIndex)];
  worldView_ = new WorldView(this);
  worldView_->setParentItem(this);
  fx_ = new fx::FxLayer(worldView_);
  hud_ = new HudView(this);
  hud_->setParentItem(this);
  hudTop_ = new HudTopView(this);
  hudTop_->setParentItem(this);
  fx_->setReducedMotion(app.profile().settings.reducedMotion);
  world_.setSink(this);
  world_.reset(app.content().game, *level_, preset, seed);
  if (autoplay_) bot_ = std::make_unique<Bot>(kAverageBot, seed ^ 0xB07ULL);
  tutorialStep_ = level_->forgiveFirstMiss ? 0 : 3;
  comboShown_ = 0;
}

GameScreen::~GameScreen() = default;

void GameScreen::onEnter() {
  app_.audio().startMusic();
  app_.audio().duckMusic(false);
}

void GameScreen::onLeave() {}

float GameScreen::lowHealthPulse() const {
  const int h = world_.halfHearts();
  if (h <= 0 || h > 2 || world_.status() != RunStatus::Running) return 0.f;
  if (app_.profile().settings.reducedMotion) return 0.5f;
  return 0.5f + 0.5f * static_cast<float>(std::sin(lowHealthClock_ * 6.0));
}

float GameScreen::toastAlpha() const {
  if (toastT_ <= 0.f) return 0.f;
  return std::clamp(toastT_ / (toastMax_ * 0.3f), 0.f, 1.f);
}

float GameScreen::graceFraction() const {
  if (world_.combo() <= 0) return 0.f;
  return std::clamp(static_cast<float>(world_.comboGraceSec()) / cfg::kComboGraceSec, 0.f, 1.f);
}

void GameScreen::showToast(const QString& t, const QColor& c, float sec) {
  toast_ = t;
  toastColor_ = c;
  toastT_ = toastMax_ = sec;
}

InputSnapshot GameScreen::currentInput() {
  if (autoplay_ && bot_) return bot_->decide(world_);
  InputSnapshot in;
  if (hasMouse_) {
    in.hasMouse = true;
    in.mouseX = mouseX_;
  } else if (leftHeld_ && rightHeld_) {
    in.axis = static_cast<int8_t>(lastDir_);
  } else if (rightHeld_) {
    in.axis = 1;
  } else if (leftHeld_) {
    in.axis = -1;
  }
  return in;
}

bool GameScreen::handleKey(QKeyEvent* e) {
  switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
      leftHeld_ = true;
      lastDir_ = -1;
      hasMouse_ = false;
      return true;
    case Qt::Key_Right:
    case Qt::Key_D:
      rightHeld_ = true;
      lastDir_ = 1;
      hasMouse_ = false;
      return true;
    case Qt::Key_Escape:
    case Qt::Key_P:
      pause();
      return true;
    default: break;
  }
  return false;
}

void GameScreen::keyReleased(QKeyEvent* e) {
  if (e->isAutoRepeat()) return;
  if (e->key() == Qt::Key_Left || e->key() == Qt::Key_A) leftHeld_ = false;
  if (e->key() == Qt::Key_Right || e->key() == Qt::Key_D) rightHeld_ = false;
  if (leftHeld_ && !rightHeld_) lastDir_ = -1;
  if (rightHeld_ && !leftHeld_) lastDir_ = 1;
}

void GameScreen::mouseMoved(double x, double) {
  mouseX_ = static_cast<float>(x);
  if (!autoplay_) hasMouse_ = true;
}

void GameScreen::pause() {
  if (paused_ || ending_ || finished_) return;
  paused_ = true;
  leftHeld_ = rightHeld_ = false;
  app_.audio().duckMusic(true);
  app_.push(new PauseOverlay(app_, this));
}

void GameScreen::resume() {
  if (!paused_) return;
  paused_ = false;
  acc_ = 0.0;
  app_.audio().duckMusic(false);
  app_.pop();
}

void GameScreen::restart() {
  app_.startLevel(levelIndex_);
}

void GameScreen::quitToLevels() { app_.goLevelSelect(levelIndex_); }

void GameScreen::tick(double dtIn) {
  if (paused_ || finished_) return;
  const double dt = std::min(dtIn, cfg::kMaxFrameDtSec);
  viewClock_ += dt;
  lowHealthClock_ += dt;
  introT_ += static_cast<float>(dt);
  const float fdt = static_cast<float>(dt);

  if (world_.status() == RunStatus::Running) {
    acc_ += dt;
    int guard = 0;
    while (acc_ >= static_cast<double>(cfg::kSimStepSec) && world_.status() == RunStatus::Running && guard++ < 90) {
      world_.step(currentInput());
      acc_ -= static_cast<double>(cfg::kSimStepSec);
    }
    alpha_ = std::clamp(acc_ / static_cast<double>(cfg::kSimStepSec), 0.0, 1.0);
    if (world_.status() != RunStatus::Running) {
      ending_ = true;
      endTimer_ = 0.0;
      alpha_ = 1.0;
      const bool cleared = world_.status() == RunStatus::Cleared;
      app_.audio().play(cleared ? Sfx::LevelClear : Sfx::GameOver);
      if (cleared) {
        fx_->confetti(140);
        showToast(QStringLiteral("LEVEL CLEAR!"), Theme::gold(), 2.2f);
      } else {
        shake_.add(0.8f);
        showToast(QStringLiteral("GAME OVER"), Theme::bad(), 2.2f);
      }
    }
  } else {
    alpha_ = 1.0;
  }

  // ---- view-side animation state -------------------------------------------------
  const double target = static_cast<double>(world_.score());
  shownScore_ += (target - shownScore_) * std::min(1.0, dt * 9.0);
  if (std::fabs(target - shownScore_) < 0.6) shownScore_ = target;
  squash_ = std::max(0.f, squash_ - fdt * 5.f);
  glow_ = std::max(0.f, glow_ - fdt * 2.5f);
  heartPulse_ = std::max(0.f, heartPulse_ - fdt * 3.f);
  comboPop_ = std::max(0.f, comboPop_ - fdt * 5.f);
  objectiveFlash_ = std::max(0.f, objectiveFlash_ - fdt);
  if (toastT_ > 0.f) toastT_ = std::max(0.f, toastT_ - fdt);
  if (tutorialStep_ == 2 && introT_ > 0.f) {
    tutorialTimer_ -= fdt;
    if (tutorialTimer_ <= 0.f) tutorialStep_ = 3;
  }

  const Settings& s = app_.profile().settings;
  shake_.update(dt);
  const float shakeScale = (s.screenShake && !s.reducedMotion) ? 1.f : 0.f;
  worldView_->setPos(shake_.offset(shakeScale));
  worldView_->setTransformOriginPoint(kW / 2, kH / 2);
  worldView_->setRotation(static_cast<qreal>(shake_.angle(shakeScale)));
  fx_->step(dt);
  worldView_->update();
  hud_->update();
  hudTop_->update();

  if (ending_) {
    endTimer_ += dt;
    if (endTimer_ >= (world_.status() == RunStatus::Cleared ? 2.0 : 1.7)) finishRun();
  }
}

void GameScreen::finishRun() {
  if (finished_) return;
  finished_ = true;
  ResultData rd;
  rd.levelIndex = levelIndex_;
  rd.run = world_.result();
  rd.hasNext = rd.run.cleared && levelIndex_ + 1 < static_cast<int>(app_.content().levels.size());
  if (!autoplay_) {
    rd.summary = recordRun(app_.profile(), *level_, rd.run);
    app_.saveProfile();
  } else {
    rd.summary.rankNow = rankFor(*level_, rd.run);
  }
  app_.showResult(rd);
}

void GameScreen::onEvent(const Event& e) {
  const QPointF at(static_cast<double>(e.x), static_cast<double>(cfg::kMouthYPx));
  switch (e.type) {
    case EventType::CatchMade: {
      const bool golden = e.eggType == EggType::Golden;
      const bool clutch = (e.flags & kFlagClutch) != 0;
      const bool rescued = (e.flags & kFlagRescued) != 0;
      const int q = static_cast<int>(e.quality);
      fx_->catchBurst(at, q, golden, clutch);
      const QColor sc = golden ? Theme::gold() : q == 2 ? QColor(0xFF, 0xF1, 0x7A) : q == 1 ? Theme::ink() : Theme::inkDim();
      fx_->popup(QStringLiteral("+%1").arg(e.scoreDelta), QPointF(e.x, 925), sc, q == 2 || golden ? 58 : 46);
      float extraY = 880.f;
      if (golden) {
        fx_->popup(QStringLiteral("GOLDEN!"), QPointF(e.x, extraY), Theme::gold(), 54, 1.1f, 70.f);
        extraY -= 50.f;
      }
      if (q == 2 && !rescued) {
        fx_->popup(QStringLiteral("PERFECT"), QPointF(e.x, extraY), QColor(0xFF, 0xF1, 0x7A), 46, 0.9f, 70.f);
        extraY -= 48.f;
      }
      if (clutch) {
        fx_->popup(QStringLiteral("CLUTCH!"), QPointF(e.x, extraY), QColor(0xFF, 0x8A, 0x3D), 52, 1.0f, 80.f);
        shake_.add(0.25f);
      }
      if (rescued) fx_->popup(QStringLiteral("SAVED!"), QPointF(e.x, extraY), QColor(0x6B, 0xC5, 0xFF), 44, 0.9f, 70.f);
      squash_ = 1.f;
      glow_ = golden ? 1.f : std::max(glow_, 0.f);
      if (golden) app_.audio().play(Sfx::Golden);
      else if (clutch) app_.audio().play(Sfx::Clutch);
      else if (q == 2 && !rescued) app_.audio().play(Sfx::Perfect);
      app_.audio().play(Sfx::Catch, std::min(7, e.combo / 2));
      if (e.combo >= 10 && e.combo % 10 == 0) {
        fx_->popup(QStringLiteral("COMBO x%1").arg(e.combo), QPointF(kW / 2, 330), Theme::gold(), 80, 1.2f, 60.f);
        fx_->ring(QPointF(kW / 2, 330), Theme::gold(), 220.f, 0.5f);
      }
      ++catchesForTutorial_;
      if (tutorialStep_ == 0) tutorialStep_ = 1;
      if (tutorialStep_ == 1 && catchesForTutorial_ >= 3) {
        tutorialStep_ = 2;
        tutorialTimer_ = 4.f;
      }
      break;
    }
    case EventType::ComboChanged:
      comboShown_ = e.combo;
      comboPop_ = 1.f;
      break;
    case EventType::ComboBroken:
      if (e.combo >= 5) fx_->popup(QStringLiteral("Combo lost"), QPointF(kW / 2, 250), Theme::inkDim(), 44, 0.9f, 40.f);
      comboShown_ = 0;
      break;
    case EventType::EggRimBounced:
      fx_->rimSparks(at);
      app_.audio().play(Sfx::Rim);
      break;
    case EventType::EggMissed: {
      const bool counts = e.value > 0;
      const bool heavy = e.value >= 2;
      fx_->splat(QPointF(e.x, cfg::kGroundYPx - 4), heavy);
      if (counts) {
        shake_.add(heavy ? 0.7f : 0.45f);
        app_.audio().play(heavy ? Sfx::MissHeavy : Sfx::Miss);
        fx_->popup(QStringLiteral("MISS"), QPointF(std::clamp(e.x, 120.f, 1800.f), 960), Theme::bad(), 48, 0.9f, 60.f);
      }
      break;
    }
    case EventType::HeartLost:
      heartPulse_ = 1.f;
      app_.audio().play(Sfx::HeartLost);
      if ((e.flags & 1u) != 0) showToast(QStringLiteral("First miss is free here!"), Theme::good(), 2.0f);
      break;
    case EventType::HeartGained:
      heartPulse_ = 1.f;
      if ((e.flags & 1u) == 0) {
        app_.audio().play(Sfx::HeartGained);
        showToast(QStringLiteral("+½ HEART"), QColor(0xFF, 0x8A, 0x9A), 1.4f);
      }
      break;
    case EventType::WaveCleared: {
      const bool flawless = (e.flags & 1u) != 0;
      app_.audio().play(Sfx::Wave);
      showToast(flawless ? QStringLiteral("FLAWLESS WAVE!") : QStringLiteral("WAVE CLEAR"), flawless ? Theme::gold() : Theme::ink(), 1.6f);
      fx_->ring(QPointF(kW / 2, 400), flawless ? Theme::gold() : Theme::ink(), 300.f, 0.6f);
      break;
    }
    case EventType::ObjectiveMet:
      objectiveFlash_ = 1.6f;
      app_.audio().play(Sfx::Objective);
      showToast(QStringLiteral("OBJECTIVE COMPLETE!"), Theme::good(), 1.8f);
      break;
    case EventType::EggTelegraphed:
      app_.audio().play(Sfx::Telegraph);
      break;
    default: break;
  }
}

}  // namespace er
