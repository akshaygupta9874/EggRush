#include "AppController.hpp"

#include <QDateTime>
#include <QDir>
#include <QGraphicsView>
#include <QImage>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QTimer>
#include <algorithm>

#include "Art.hpp"
#include "GameScreen.hpp"
#include "MainWindow.hpp"
#include "Screens.hpp"
#include "Text.hpp"
#include "UiKit.hpp"
#include "er/Bot.hpp"

namespace er {

namespace {

class BackgroundItem : public QGraphicsObject {
 public:
  explicit BackgroundItem(const AppController* app) : app_(app) { setZValue(-10); }
  QRectF boundingRect() const override { return QRectF(0, 0, ui::kW, ui::kH); }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    art::drawBackground(p, app_->clock(), ui::Theme::highContrast());
  }

 private:
  const AppController* app_;
};

class NoteItem : public QGraphicsObject {
 public:
  explicit NoteItem(const AppController* app) : app_(app) { setZValue(100); }
  QRectF boundingRect() const override { return QRectF(0, 0, ui::kW, ui::kH); }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    const double a = app_->noteAlpha();
    if (a <= 0.0 || app_->noteText().isEmpty()) return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setOpacity(a);
    const QFont f = ui::Theme::font(28, false);
    const QString text = QFontMetricsF(f).elidedText(app_->noteText(), Qt::ElideMiddle, 1040);
    ui::drawPanel(p, QRectF(ui::kW / 2 - 560, 74, 1120, 60), 18, QColor(0x26, 0x1F, 0x45, 240), ui::Theme::accent(), 3);
    ui::drawShadowText(p, QRectF(ui::kW / 2 - 540, 74, 1080, 60), Qt::AlignCenter, text, f, ui::Theme::ink(), 0);
    p->restore();
  }

 private:
  const AppController* app_;
};

}  // namespace

AppController::AppController(LaunchOptions opts, ContentDb content, Profile profile, SaveSystem saves, QObject* parent)
    : QObject(parent),
      opts_(std::move(opts)),
      content_(std::move(content)),
      profile_(std::move(profile)),
      saves_(std::move(saves)) {
  scene_.setSceneRect(0, 0, ui::kW, ui::kH);
  scene_.setItemIndexMethod(QGraphicsScene::NoIndex);
  background_ = new BackgroundItem(this);
  scene_.addItem(background_);
  scene_.addItem(new NoteItem(this));

  const bool audioOff = opts_.mute || qEnvironmentVariable("ER_AUDIO") == QLatin1String("off");
  audio_ = std::make_unique<AudioEngine>(saves_.dir() + QStringLiteral("/cache"), !audioOff);
  applySettings();

  timer_.setTimerType(Qt::PreciseTimer);
  timer_.setInterval(8);
  connect(&timer_, &QTimer::timeout, this, &AppController::frame);
}

AppController::~AppController() {
  timer_.stop();
  while (!stack_.empty()) {
    Screen* s = stack_.back();
    stack_.pop_back();
    scene_.removeItem(s);
    delete s;
  }
}

void AppController::attachView(GameView* view) { view_ = view; }

Preset AppController::activePreset() const {
  if (opts_.preset >= 0 && opts_.preset <= 2) return static_cast<Preset>(opts_.preset);
  return profile_.preset;
}

void AppController::applySettings() {
  const Settings& s = profile_.settings;
  ui::Theme::setHighContrast(s.highContrast);
  audio_->setVolumes(s.masterVolume, s.sfxVolume, s.musicVolume, s.muted);
  emit fullscreenRequested(s.fullscreen);
  scene_.update();
}

void AppController::saveProfile() {
  QString err;
  if (!saves_.save(profile_, &err)) {
    qWarning("could not save profile: %s", qPrintable(err));
    postNote(QStringLiteral("Could not save your progress: %1").arg(err), 12.0);
  }
}

void AppController::install(Screen* s) {
  scene_.addItem(s);
  s->setPos(0, 0);
}

void AppController::uninstall(Screen* s) {
  scene_.removeItem(s);
  s->deleteLater();
}

void AppController::updateVisibility() {
  // The top screen is always visible; a screen below stays visible only while every
  // screen above it is see-through (pause menu over the game: yes; settings: no).
  bool covered = false;
  for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
    (*it)->setVisible(!covered);
    if ((*it)->opaque()) covered = true;
  }
}

void AppController::push(Screen* s) {
  install(s);
  stack_.push_back(s);
  updateVisibility();
  s->onEnter();
}

void AppController::pop() {
  QMetaObject::invokeMethod(this, [this]() {
    if (stack_.empty()) return;
    Screen* s = stack_.back();
    stack_.pop_back();
    s->onLeave();
    uninstall(s);
    updateVisibility();
  }, Qt::QueuedConnection);
}

void AppController::replaceAll(Screen* s) {
  while (!stack_.empty()) {
    Screen* old = stack_.back();
    stack_.pop_back();
    old->onLeave();
    uninstall(old);
  }
  push(s);
}

void AppController::goTitle() {
  QMetaObject::invokeMethod(this, [this]() { replaceAll(new TitleScreen(*this)); }, Qt::QueuedConnection);
}

void AppController::goLevelSelect(int focusLevel) {
  QMetaObject::invokeMethod(this, [this, focusLevel]() { replaceAll(new LevelSelectScreen(*this, focusLevel)); }, Qt::QueuedConnection);
}

void AppController::startLevel(int levelIndex, bool useSeed) {
  QMetaObject::invokeMethod(this, [this, levelIndex, useSeed]() {
    if (levelIndex < 0 || levelIndex >= static_cast<int>(content_.levels.size())) return;
    uint64_t seed = QRandomGenerator::global()->generate64();
    if (opts_.seedSet || useSeed) seed = opts_.seed;
    profile_.lastLevel = QString::fromStdString(content_.levels[static_cast<size_t>(levelIndex)].id);
    replaceAll(new GameScreen(*this, levelIndex, activePreset(), seed));
  }, Qt::QueuedConnection);
}

void AppController::showResult(const ResultData& r) {
  push(new ResultScreen(*this, r));
}

void AppController::openSettings() {
  QMetaObject::invokeMethod(this, [this]() { push(new SettingsScreen(*this)); }, Qt::QueuedConnection);
}

void AppController::requestQuit() {
  saveProfile();
  emit quitRequested();
}

void AppController::start() {
  wall_.start();
  sinceStart_.start();
  timer_.start();
  const QString& scr = opts_.screen;
  if (!opts_.level.isEmpty()) {
    const int idx = content_.levelIndex(opts_.level);
    if (idx >= 0) {
      replaceAll(new GameScreen(*this, idx, activePreset(), opts_.seedSet ? opts_.seed : QRandomGenerator::global()->generate64()));
      if (scr == QLatin1String("pause")) {
        QTimer::singleShot(2500, this, [this]() {
          if (auto* g = dynamic_cast<GameScreen*>(top())) g->pause();
        });
      }
      return;
    }
    qWarning("unknown level '%s'", qPrintable(opts_.level));
  }
  if (scr == QLatin1String("levels")) {
    replaceAll(new LevelSelectScreen(*this, -1));
  } else if (scr == QLatin1String("settings")) {
    replaceAll(new TitleScreen(*this));
    push(new SettingsScreen(*this));
  } else if (scr == QLatin1String("result")) {
    // A synthetic result for visual QA: one headless expert run of the first level.
    const LevelDef& l = content_.levels.front();
    const BotRunStats st = simulateRun(content_.game, l, activePreset(), opts_.seedSet ? opts_.seed : 7, kExpertBot);
    ResultData rd;
    rd.levelIndex = 0;
    rd.run = st.result;
    rd.hasNext = content_.levels.size() > 1;
    Profile scratch = profile_;
    rd.summary = recordRun(scratch, l, rd.run);
    replaceAll(new GameScreen(*this, 0, activePreset(), 7));
    showResult(rd);
  } else {
    replaceAll(new TitleScreen(*this));
  }
}

void AppController::frame() {
  const double dt = std::clamp(static_cast<double>(wall_.nsecsElapsed()) * 1e-9, 0.0, cfg::kMaxFrameDtSec);
  wall_.restart();
  clock_ += dt;
  if (noteTimer_ > 0.0) {
    noteTimer_ -= dt;
    if (noteTimer_ <= 0.0) note_.clear();
  }
  fpsAccum_ += dt;
  ++fpsFrames_;
  if (fpsAccum_ >= 0.5) {
    fps_ = fpsFrames_ / fpsAccum_;
    fpsAccum_ = 0.0;
    fpsFrames_ = 0;
  }
  if (Screen* t = top()) t->tick(dt);
  background_->update();
  scene_.update();

  const double elapsed = static_cast<double>(sinceStart_.elapsed()) / 1000.0;
  if (!opts_.screenshotPath.isEmpty() && !shotTaken_ && elapsed >= opts_.screenshotAfterSec) {
    shotTaken_ = true;
    const bool ok = grabScreenshot(opts_.screenshotPath);
    qInfo("screenshot %s: %s", qPrintable(opts_.screenshotPath), ok ? "saved" : "FAILED");
    if (opts_.quitAfterSec <= 0.0) emit quitRequested();
  }
  if (opts_.quitAfterSec > 0.0 && elapsed >= opts_.quitAfterSec) emit quitRequested();
}

bool AppController::grabScreenshot(const QString& path) {
  QImage img(static_cast<int>(ui::kW), static_cast<int>(ui::kH), QImage::Format_ARGB32);
  img.fill(Qt::black);
  QPainter p(&img);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  scene_.render(&p, QRectF(0, 0, ui::kW, ui::kH), QRectF(0, 0, ui::kW, ui::kH));
  p.end();
  QDir().mkpath(QFileInfo(path).absolutePath());
  return img.save(path);
}

void AppController::keyPressed(QKeyEvent* e) {
  switch (e->key()) {
    case Qt::Key_F11:
      profile_.settings.fullscreen = !profile_.settings.fullscreen;
      applySettings();
      return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      if (e->modifiers() & Qt::AltModifier) {
        profile_.settings.fullscreen = !profile_.settings.fullscreen;
        applySettings();
        return;
      }
      break;
    case Qt::Key_F3: debug_ = !debug_; return;
    case Qt::Key_F12: {
      const QString path = saves_.dir() + QStringLiteral("/screenshots/eggrush-%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
      if (grabScreenshot(path)) postNote(QStringLiteral("Screenshot saved: %1").arg(path));
      return;
    }
    case Qt::Key_M:
      if (!(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        profile_.settings.muted = !profile_.settings.muted;
        applySettings();
        return;
      }
      break;
    default: break;
  }
  if (Screen* t = top()) t->handleKey(e);
}

void AppController::keyReleased(QKeyEvent* e) {
  if (Screen* t = top()) t->keyReleased(e);
}

void AppController::mouseMoved(double x, double y) {
  if (Screen* t = top()) t->mouseMoved(x, y);
}

void AppController::windowDeactivated() {
  if (Screen* t = top()) t->windowDeactivated();
}

}  // namespace er
