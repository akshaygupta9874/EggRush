#include "Screens.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QLocale>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

#include "Art.hpp"
#include "GameScreen.hpp"
#include "Text.hpp"

namespace er {

using ui::kH;
using ui::kW;
using ui::drawPanel;
using ui::drawShadowText;
using ui::Theme;

namespace {

double easeOutBack(double t) {
  t = std::clamp(t, 0.0, 1.0);
  const double c1 = 1.70158, c3 = c1 + 1.0;
  return 1.0 + c3 * std::pow(t - 1.0, 3.0) + c1 * std::pow(t - 1.0, 2.0);
}
double easeOut(double t) {
  t = std::clamp(t, 0.0, 1.0);
  return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
}

QString thousands(qint64 v) { return QLocale(QLocale::English).toString(v); }

void dim(QPainter* p, int alpha) { p->fillRect(QRectF(0, 0, kW, kH), QColor(0x0E, 0x0A, 0x20, alpha)); }

void titleText(QPainter* p, const QRectF& r, const QString& text, qreal px, const QColor& top, const QColor& bottom) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  QFont f = Theme::font(px);
  QPainterPath path;
  QFontMetricsF fm(f);
  const qreal w = fm.horizontalAdvance(text);
  path.addText(r.center().x() - w / 2, r.center().y() + fm.ascent() / 2 - fm.descent() / 2, f, text);
  p->translate(0, px * 0.05);
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0x1B, 0x10, 0x30, 190));
  p->drawPath(path);
  p->translate(0, -px * 0.05);
  QLinearGradient g(0, r.top(), 0, r.bottom());
  g.setColorAt(0, top);
  g.setColorAt(1, bottom);
  p->setBrush(g);
  p->setPen(QPen(QColor(0x7A, 0x3E, 0x08), px * 0.05, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p->drawPath(path);
  p->restore();
}

}  // namespace

// ============================================================================
// Title
// ============================================================================
TitleScreen::TitleScreen(AppController& app) : Screen(app) {
  fx_ = new fx::FxLayer(this);
  auto* play = new ButtonItem(QStringLiteral("PLAY"), 460, 96, true, this);
  auto* settings = new ButtonItem(QStringLiteral("SETTINGS"), 460, 86, false, this);
  auto* quit = new ButtonItem(QStringLiteral("QUIT"), 460, 86, false, this);
  play->setPos(kW / 2 - 230, 560);
  settings->setPos(kW / 2 - 230, 676);
  quit->setPos(kW / 2 - 230, 782);
  addFocusable(play);
  addFocusable(settings);
  addFocusable(quit);
  connect(play, &FocusItem::activated, this, [this]() { app_.goLevelSelect(); });
  connect(settings, &FocusItem::activated, this, [this]() { app_.openSettings(); });
  connect(quit, &FocusItem::activated, this, [this]() { app_.requestQuit(); });
}

void TitleScreen::onEnter() {
  app_.audio().startMusic();
  fx_->setReducedMotion(app_.profile().settings.reducedMotion);
}

void TitleScreen::tick(double dt) {
  t_ += dt;
  fx_->step(dt);
  Demo& d = demo_;
  const float fdt = static_cast<float>(dt);
  if (!d.falling && !d.laying) {
    d.timer -= dt;
    if (d.timer <= 0.0) {
      d.henTarget = rng_.range(420.f, 1500.f);
      d.laying = true;
      d.lay = 0.0;
    }
  }
  const float dx = d.henTarget - d.henX;
  const float step = 1100.f * fdt;
  d.henX += std::clamp(dx, -step, step);
  if (d.laying && std::fabs(dx) < 4.f) {
    d.lay += dt / 0.55;
    if (d.lay >= 1.0) {
      d.laying = false;
      d.falling = true;
      d.eggX = d.henX;
      d.eggY = 120.f;
      d.eggVy = 0.f;
      d.timer = 0.9;
    }
  }
  if (d.falling) {
    d.eggVy += 900.f * fdt;
    d.eggY += d.eggVy * fdt;
    if (d.eggY >= 980.f) {
      d.falling = false;
      d.squash = 1.f;
      ++d.caught;
      fx_->catchBurst(QPointF(d.eggX, 980), 2, false, false);
      app_.audio().play(Sfx::Catch, std::min(7, d.caught % 8));
    }
  }
  const float target = d.falling ? d.eggX : d.henX;
  const float want = (target - d.basketX) * std::min(1.f, 9.f * fdt);
  d.basketVx = want / std::max(fdt, 1e-4f);
  d.basketX += want;
  d.squash = std::max(0.f, d.squash - fdt * 4.f);
  update();
}

void TitleScreen::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  art::drawPerch(p);
  art::HenPose hen;
  hen.x = demo_.henX;
  hen.t = t_;
  hen.laying = demo_.laying;
  hen.lay = static_cast<float>(demo_.lay);
  hen.moving = std::fabs(demo_.henTarget - demo_.henX) > 6.f;
  hen.facing = demo_.henTarget >= demo_.henX ? 1 : -1;
  art::drawHen(p, hen);

  art::BasketPose basket;
  basket.x = demo_.basketX;
  basket.vx = demo_.basketVx;
  basket.squash = demo_.squash;
  art::drawBasketBack(p, basket);
  if (demo_.falling) {
    p->save();
    p->translate(demo_.eggX, demo_.eggY);
    art::drawEgg(p, EggType::Normal, demo_.eggVy * 0.01, t_, Theme::highContrast());
    p->restore();
  }
  art::drawBasketFront(p, basket);

  const double wob = 1.0 + 0.012 * std::sin(t_ * 2.2);
  p->save();
  p->translate(kW / 2, 300);
  p->scale(wob, wob);
  p->translate(-kW / 2, -300);
  titleText(p, QRectF(0, 200, kW, 200), QStringLiteral("EGG RUSH"), 200, QColor(0xFF, 0xF1, 0x7A), QColor(0xFF, 0xA7, 0x26));
  p->restore();
  drawShadowText(p, QRectF(0, 410, kW, 70), Qt::AlignCenter, QStringLiteral("Catch every egg. Chase every combo."),
                 Theme::font(44), Theme::ink(), 3);

  const Profile& pr = app_.profile();
  const QString line = QStringLiteral("Level %1   \u00B7   %2 coins   \u00B7   %3 stars")
                           .arg(pr.playerLevel())
                           .arg(pr.coins)
                           .arg(totalStars(pr));
  drawPanel(p, QRectF(kW / 2 - 400, 892, 800, 70), 30, Theme::panel(), Theme::panelEdge(), 3);
  drawShadowText(p, QRectF(kW / 2 - 400, 892, 800, 70), Qt::AlignCenter, line, Theme::font(34), Theme::ink(), 0);
  drawShadowText(p, QRectF(0, 488, kW, 44), Qt::AlignCenter,
                 QStringLiteral("\u2190 \u2192 / A D / mouse: move      Esc: pause      F11: fullscreen      M: mute"),
                 Theme::font(28, false), Theme::ink(), 3);
  drawShadowText(p, QRectF(kW - 420, kH - 52, 400, 40), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("v0.1.0"), Theme::font(24, false), Theme::inkDim(), 0);
}

// ============================================================================
// Level select
// ============================================================================
namespace {

class LevelCardItem : public FocusItem {
 public:
  LevelCardItem(AppController& app, int idx, QGraphicsItem* parent) : FocusItem(parent), app_(app), idx_(idx) {
    size_ = QSizeF(520, 236);
    setLocked(!app.profile().isUnlocked(app.content(), idx));
  }
  void activate() override {
    if (locked()) {
      app_.audio().play(Sfx::UiBack);
      shake_ = 0.35;
      if (timerId_ == 0) timerId_ = startTimer(16);
      return;
    }
    FocusItem::activate();
    app_.startLevel(idx_);
  }
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    const LevelDef& l = app_.content().levels[static_cast<size_t>(idx_)];
    const LevelRecord rec = app_.profile().levels.value(QString::fromStdString(l.id));
    QRectF r = boundingRect().adjusted(8, 8, -8, -8);
    const bool f = focused();
    p->save();
    if (shake_ > 0.0) p->translate(std::sin(shake_ * 70.0) * 10.0 * shake_ / 0.35, 0);
    p->translate(r.center());
    p->scale(f ? 1.04 : 1.0, f ? 1.04 : 1.0);
    p->translate(-r.center());

    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, locked() ? QColor(0x3A, 0x34, 0x4E, 235) : QColor(0x3C, 0x30, 0x6E, 235));
    g.setColorAt(1, locked() ? QColor(0x24, 0x20, 0x34, 235) : QColor(0x24, 0x1C, 0x4A, 235));
    p->setPen(QPen(f ? Theme::focusRing() : Theme::panelEdge(), f ? 6 : 3));
    p->setBrush(g);
    p->drawRoundedRect(r, 30, 30);

    p->setFont(Theme::font(80));
    p->setPen(locked() ? Theme::inkDim().darker(140) : Theme::gold());
    p->drawText(QRectF(r.left() + 26, r.top() + 6, 240, 92), Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(l.id));
    p->setFont(Theme::font(34));
    p->setPen(locked() ? Theme::inkDim().darker(120) : Theme::ink());
    p->drawText(QRectF(r.left() + 26, r.top() + 92, r.width() - 52, 44), Qt::AlignLeft | Qt::AlignVCenter,
                QString::fromStdString(l.name));

    if (locked()) {
      // A padlock instead of the (hidden) objective / stars, so nothing overlaps.
      const QPointF c(r.right() - 66, r.top() + 62);
      p->setPen(QPen(Theme::inkDim(), 8, Qt::SolidLine, Qt::RoundCap));
      p->setBrush(Qt::NoBrush);
      p->drawArc(QRectF(c.x() - 20, c.y() - 44, 40, 48), 0, 180 * 16);
      p->setPen(QPen(QColor(0x1B, 0x10, 0x30), 3));
      p->setBrush(Theme::inkDim());
      p->drawRoundedRect(QRectF(c.x() - 32, c.y() - 8, 64, 48), 10, 10);
      p->setBrush(QColor(0x1B, 0x10, 0x30));
      p->drawEllipse(QPointF(c.x(), c.y() + 14), 7, 7);
      const QString prev = idx_ > 0 ? QString::fromStdString(app_.content().levels[static_cast<size_t>(idx_ - 1)].id) : QString();
      p->setFont(Theme::font(28, false));
      p->setPen(Theme::inkDim());
      p->drawText(QRectF(r.left() + 26, r.top() + 146, r.width() - 52, 40), Qt::AlignLeft | Qt::AlignVCenter,
                  QStringLiteral("Clear %1 to unlock").arg(prev));
    } else {
      // objective line
      p->setFont(Theme::font(25, false));
      p->setPen(Theme::inkDim());
      const QFontMetricsF fm(p->font());
      p->drawText(QRectF(r.left() + 26, r.top() + 134, r.width() - 52, 32), Qt::AlignLeft | Qt::AlignVCenter,
                  fm.elidedText(QStringLiteral("★ ") + objectiveText(app_.content(), l), Qt::ElideRight, r.width() - 52));

      // stars (Clear / Objective / Flawless)
      const bool earned[3] = {rec.cleared, rec.objective, rec.flawless};
      for (int i = 0; i < 3; ++i) ui::drawStar(p, QPointF(r.left() + 50 + i * 54, r.bottom() - 32), 20, earned[i]);

      // rank badge + best score
      if (rec.bestRank > 0) {
        const QColor rc = rec.bestRank == 4 ? QColor(0xFF, 0xD7, 0x3D) : rec.bestRank == 3 ? QColor(0x7B, 0xE0, 0x8A) : rec.bestRank == 2 ? QColor(0x6B, 0xC5, 0xFF) : QColor(0xC9, 0xC2, 0xD8);
        p->setPen(QPen(rc.darker(160), 4));
        p->setBrush(rc);
        p->drawEllipse(QPointF(r.right() - 62, r.top() + 62), 40, 40);
        p->setFont(Theme::font(48));
        p->setPen(QColor(0x2A, 0x16, 0x05));
        p->drawText(QRectF(r.right() - 102, r.top() + 22, 80, 80), Qt::AlignCenter, QString(rankLetter(rec.bestRank)));
      }
      p->setFont(Theme::font(28));
      p->setPen(Theme::ink());
      p->drawText(QRectF(r.right() - 250, r.bottom() - 54, 230, 40), Qt::AlignRight | Qt::AlignVCenter,
                  rec.bestScore > 0 ? QStringLiteral("Best %1").arg(thousands(rec.bestScore)) : QStringLiteral("New"));
    }
    p->restore();
  }

 protected:
  void timerEvent(QTimerEvent*) override {
    shake_ = std::max(0.0, shake_ - 0.016);
    if (shake_ <= 0.0 && timerId_ != 0) {
      killTimer(timerId_);
      timerId_ = 0;
    }
    update();
  }

 private:
  AppController& app_;
  int idx_;
  double shake_ = 0.0;
  int timerId_ = 0;
};

}  // namespace

LevelSelectScreen::LevelSelectScreen(AppController& app, int focusLevel) : Screen(app) {
  const ContentDb& db = app.content();
  preset_ = new ChoiceItem(QStringLiteral("Difficulty"), {QStringLiteral("Chill"), QStringLiteral("Normal"), QStringLiteral("Hardcore")},
                           static_cast<int>(app.profile().preset), 760, 84, this);
  preset_->setPos(kW / 2 - 380, 150);
  addFocusable(preset_);
  connect(preset_, &ChoiceItem::changed, this, [this](int i) {
    app_.profile().preset = static_cast<Preset>(i);
    app_.saveProfile();
    update();
  });

  const int cols = 3;
  int firstUnlocked = 0;
  for (int i = 0; i < static_cast<int>(db.levels.size()); ++i) {
    auto* c = new LevelCardItem(app, i, this);
    const int row = i / cols, col = i % cols;
    c->setPos(140 + col * 560, 300 + row * 270);
    addFocusable(c);
    cards_.push_back(c);
    if (!c->locked()) firstUnlocked = i;
  }
  auto* back = new ButtonItem(QStringLiteral("BACK"), 280, 80, false, this);
  back->setPos(140, 920);
  addFocusable(back);
  connect(back, &FocusItem::activated, this, [this]() { app_.goTitle(); });

  int want = focusLevel >= 0 ? focusLevel : firstUnlocked;
  if (focusLevel < 0) {
    const int last = db.levelIndex(app.profile().lastLevel);
    if (last >= 0) want = last;
    if (want >= static_cast<int>(cards_.size()) || static_cast<LevelCardItem*>(cards_[static_cast<size_t>(want)])->locked()) want = firstUnlocked;
  }
  if (want >= 0 && want < static_cast<int>(cards_.size())) setFocusItem(cards_[static_cast<size_t>(want)]);
}

void LevelSelectScreen::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  dim(p, 120);
  titleText(p, QRectF(0, 30, kW, 110), QStringLiteral("Choose a Level"), 92, QColor(0xFF, 0xF1, 0x7A), QColor(0xFF, 0xA7, 0x26));
  drawShadowText(p, QRectF(0, 238, kW, 44), Qt::AlignCenter, presetInfo(static_cast<Preset>(preset_->index())),
                 Theme::font(28, false), Theme::ink(), 2);
  const Profile& pr = app_.profile();
  drawShadowText(p, QRectF(kW - 760, 930, 620, 60), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("★ %1 / %2     %3 coins").arg(totalStars(pr)).arg(app_.content().levels.size() * 3).arg(pr.coins),
                 Theme::font(36), Theme::ink(), 3);
}

// ============================================================================
// Settings
// ============================================================================
SettingsScreen::SettingsScreen(AppController& app) : Screen(app) {
  Settings& s = app.profile().settings;
  const qreal w = 980, h = 74, x = kW / 2 - w / 2;
  qreal y = 150;
  auto place = [&](FocusItem* it) {
    it->setPos(x, y);
    y += h + 12;
    addFocusable(it);
  };
  auto slider = [&](const QString& label, float v, auto setter) {
    auto* it = new SliderItem(label, v, w, h, this);
    connect(it, &SliderItem::changed, this, [this, setter](float nv) {
      setter(nv);
      app_.applySettings();
    });
    place(it);
  };
  auto toggle = [&](const QString& label, bool v, auto setter) {
    auto* it = new ChoiceItem(label, {QStringLiteral("Off"), QStringLiteral("On")}, v ? 1 : 0, w, h, this);
    connect(it, &ChoiceItem::changed, this, [this, setter](int i) {
      setter(i == 1);
      app_.applySettings();
    });
    place(it);
  };
  slider(QStringLiteral("Master volume"), s.masterVolume, [&s](float v) { s.masterVolume = v; });
  slider(QStringLiteral("Effects volume"), s.sfxVolume, [&s](float v) { s.sfxVolume = v; });
  slider(QStringLiteral("Music volume"), s.musicVolume, [&s](float v) { s.musicVolume = v; });
  toggle(QStringLiteral("Mute all sound"), s.muted, [&s](bool v) { s.muted = v; });
  toggle(QStringLiteral("Screen shake"), s.screenShake, [&s](bool v) { s.screenShake = v; });
  toggle(QStringLiteral("Reduced motion"), s.reducedMotion, [&s](bool v) { s.reducedMotion = v; });
  toggle(QStringLiteral("High contrast"), s.highContrast, [&s](bool v) { s.highContrast = v; });
  toggle(QStringLiteral("Show FPS"), s.showFps, [&s](bool v) { s.showFps = v; });
  toggle(QStringLiteral("Fullscreen"), s.fullscreen, [&s](bool v) { s.fullscreen = v; });
  auto* back = new ButtonItem(QStringLiteral("DONE"), 360, 82, true, this);
  back->setPos(kW / 2 - 180, y + 14);
  addFocusable(back);
  connect(back, &FocusItem::activated, this, [this]() { onBack(); });
}

void SettingsScreen::onBack() {
  app_.saveProfile();
  app_.pop();
}

void SettingsScreen::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  dim(p, 170);
  titleText(p, QRectF(0, 30, kW, 110), QStringLiteral("Settings"), 92, QColor(0xFF, 0xF1, 0x7A), QColor(0xFF, 0xA7, 0x26));
}

// ============================================================================
// Pause
// ============================================================================
PauseOverlay::PauseOverlay(AppController& app, GameScreen* game) : Screen(app), game_(game) {
  setZValue(40);
  const qreal w = 520, h = 90, x = kW / 2 - w / 2;
  auto* resume = new ButtonItem(QStringLiteral("RESUME"), w, h, true, this);
  auto* restart = new ButtonItem(QStringLiteral("RESTART"), w, h, false, this);
  auto* settings = new ButtonItem(QStringLiteral("SETTINGS"), w, h, false, this);
  auto* quit = new ButtonItem(QStringLiteral("LEVEL SELECT"), w, h, false, this);
  resume->setPos(x, 400);
  restart->setPos(x, 510);
  settings->setPos(x, 620);
  quit->setPos(x, 730);
  addFocusable(resume);
  addFocusable(restart);
  addFocusable(settings);
  addFocusable(quit);
  connect(resume, &FocusItem::activated, this, [this]() { game_->resume(); });
  connect(restart, &FocusItem::activated, this, [this]() { game_->restart(); });
  connect(settings, &FocusItem::activated, this, [this]() { app_.openSettings(); });
  connect(quit, &FocusItem::activated, this, [this]() { game_->quitToLevels(); });
}

void PauseOverlay::onBack() { game_->resume(); }

void PauseOverlay::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  dim(p, 150);
  drawPanel(p, QRectF(kW / 2 - 330, 230, 660, 660), 40, Theme::panel(), Theme::panelEdge(), 4);
  titleText(p, QRectF(0, 260, kW, 120), QStringLiteral("Paused"), 96, QColor(0xFF, 0xF1, 0x7A), QColor(0xFF, 0xA7, 0x26));
}

// ============================================================================
// Result
// ============================================================================
ResultScreen::ResultScreen(AppController& app, ResultData data) : Screen(app), data_(std::move(data)) {
  setZValue(30);
  fx_ = new fx::FxLayer(this);
  fx_->setReducedMotion(app.profile().settings.reducedMotion);
  std::vector<ButtonItem*> btns;
  const qreal w = 340, h = 88, gap = 26;
  if (data_.run.cleared && data_.hasNext) {
    next_ = new ButtonItem(QStringLiteral("NEXT LEVEL"), w, h, true, this);
    btns.push_back(next_);
  }
  auto* retry = new ButtonItem(QStringLiteral("RETRY"), w, h, !next_, this);
  auto* levels = new ButtonItem(QStringLiteral("LEVELS"), w, h, false, this);
  auto* title = new ButtonItem(QStringLiteral("TITLE"), w, h, false, this);
  btns.push_back(retry);
  btns.push_back(levels);
  btns.push_back(title);
  const qreal total = static_cast<qreal>(btns.size()) * w + static_cast<qreal>(btns.size() - 1) * gap;
  qreal x = kW / 2 - total / 2;
  for (ButtonItem* b : btns) {
    b->setPos(x, 905);
    x += w + gap;
    addFocusable(b);
  }
  const int idx = data_.levelIndex;
  if (next_ != nullptr) connect(next_, &FocusItem::activated, this, [this, idx]() { app_.startLevel(idx + 1); });
  connect(retry, &FocusItem::activated, this, [this, idx]() { app_.startLevel(idx); });
  connect(levels, &FocusItem::activated, this, [this, idx]() { app_.goLevelSelect(idx); });
  connect(title, &FocusItem::activated, this, [this]() { app_.goTitle(); });
  setFocusItem(next_ != nullptr ? static_cast<FocusItem*>(next_) : static_cast<FocusItem*>(retry));
}

bool ResultScreen::handleKey(QKeyEvent* e) {
  if (t_ < 0.5 && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space)) return true;  // no accidental skip
  return Screen::handleKey(e);
}

void ResultScreen::tick(double dt) {
  t_ += dt;
  fx_->step(dt);
  if (stage_ == 0 && t_ > 0.1) {
    stage_ = 1;
    if (data_.run.cleared) fx_->confetti(160);
  }
  const bool cleared = data_.run.cleared;
  const bool earned[3] = {cleared, data_.run.objectiveMet, data_.run.flawless};
  for (int i = 0; i < 3; ++i) {
    if (stage_ == 1 + i && t_ > 1.5 + 0.45 * i) {
      stage_ = 2 + i;
      if (earned[i]) {
        app_.audio().play(i == 2 ? Sfx::Perfect : Sfx::Objective);
        fx_->ring(QPointF(kW / 2 - 300 + 300 * i, 560), Theme::gold(), 110, 0.5f);
      }
    }
  }
  if (stage_ == 4 && t_ > 3.0) {
    stage_ = 5;
    if (data_.summary.rankNow >= 3) fx_->confetti(120);
    if (data_.summary.rankNow > 0) app_.audio().play(Sfx::Golden);
  }
  update();
}

void ResultScreen::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  p->setRenderHint(QPainter::Antialiasing, true);
  dim(p, 175);
  const RunResult& r = data_.run;
  const LevelDef& lvl = app_.content().levels[static_cast<size_t>(data_.levelIndex)];
  drawPanel(p, QRectF(kW / 2 - 820, 40, 1640, 840), 44, Theme::panel(), Theme::panelEdge(), 4);

  const bool cleared = r.cleared;
  titleText(p, QRectF(0, 52, kW, 120), cleared ? QStringLiteral("LEVEL CLEAR!") : QStringLiteral("GAME OVER"), 100,
            cleared ? QColor(0xFF, 0xF1, 0x7A) : QColor(0xFF, 0xB0, 0xB0), cleared ? QColor(0xFF, 0xA7, 0x26) : QColor(0xE0, 0x4B, 0x4B));
  drawShadowText(p, QRectF(0, 168, kW, 50), Qt::AlignCenter,
                 QStringLiteral("%1  ·  %2  ·  %3").arg(QString::fromStdString(lvl.id), QString::fromStdString(lvl.name), presetName(r.preset)),
                 Theme::font(34, false), Theme::inkDim(), 2);

  // score count-up
  const double k = easeOut(t_ / 1.4);
  const qint64 shown = static_cast<qint64>(std::llround(static_cast<double>(r.score) * k));
  drawShadowText(p, QRectF(kW / 2 - 700, 230, 760, 110), Qt::AlignLeft | Qt::AlignVCenter, thousands(shown), Theme::font(104), Theme::gold(), 5);
  drawShadowText(p, QRectF(kW / 2 - 700, 322, 760, 40), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SCORE"), Theme::font(28), Theme::inkDim(), 0);
  if (data_.summary.newBest && t_ > 1.4) {
    const double s = 1.0 + 0.06 * std::sin(t_ * 8.0);
    p->save();
    p->translate(kW / 2 + 90, 280);
    p->rotate(-6);
    p->scale(s, s);
    drawPanel(p, QRectF(-130, -34, 260, 68), 20, QColor(0xFF, 0x6B, 0x6B), QColor(0xFF, 0xF8, 0xE7), 4);
    drawShadowText(p, QRectF(-130, -34, 260, 68), Qt::AlignCenter, QStringLiteral("NEW BEST!"), Theme::font(36), Theme::ink(), 0);
    p->restore();
  }

  // stars
  const char* names[3] = {"Clear", "Objective", "Flawless"};
  const bool earned[3] = {cleared, r.objectiveMet, r.flawless};
  for (int i = 0; i < 3; ++i) {
    const double at = 1.5 + 0.45 * i;
    const double sc = t_ < at ? 0.0 : easeOutBack((t_ - at) / 0.4);
    const QPointF c(kW / 2 - 300 + 300 * i, 480);
    p->save();
    p->translate(c);
    p->scale(sc, sc);
    ui::drawStar(p, QPointF(0, 0), 70, earned[i]);
    p->restore();
    drawShadowText(p, QRectF(c.x() - 140, 556, 280, 40), Qt::AlignCenter, QString::fromLatin1(names[i]), Theme::font(30),
                   earned[i] ? Theme::gold() : Theme::inkDim().darker(130), 2);
  }
  drawShadowText(p, QRectF(kW / 2 - 560, 604, 1120, 40), Qt::AlignCenter,
                 QStringLiteral("Objective: %1").arg(objectiveText(app_.content(), lvl)), Theme::font(28, false),
                 r.objectiveMet ? Theme::good() : Theme::inkDim(), 0);

  // rank badge
  if (data_.summary.rankNow > 0 && stage_ >= 5) {
    const double s = easeOutBack((t_ - 3.0) / 0.5);
    const int rk = data_.summary.rankNow;
    const QColor rc = rk == 4 ? QColor(0xFF, 0xD7, 0x3D) : rk == 3 ? QColor(0x7B, 0xE0, 0x8A) : rk == 2 ? QColor(0x6B, 0xC5, 0xFF) : QColor(0xC9, 0xC2, 0xD8);
    p->save();
    p->translate(kW / 2 + 620, 290);
    p->scale(s, s);
    p->setPen(QPen(rc.darker(160), 8));
    p->setBrush(rc);
    p->drawEllipse(QPointF(0, 0), 110, 110);
    p->setFont(Theme::font(130));
    p->setPen(QColor(0x2A, 0x16, 0x05));
    p->drawText(QRectF(-110, -110, 220, 220), Qt::AlignCenter, QString(rankLetter(rk)));
    p->restore();
    drawShadowText(p, QRectF(kW / 2 + 500, 405, 240, 36), Qt::AlignCenter, QStringLiteral("RANK"), Theme::font(26), Theme::inkDim(), 0);
  }

  // stats
  const int total = r.catches + r.misses;
  const QString acc = total > 0 ? QStringLiteral("%1%").arg(static_cast<int>(std::lround(100.0 * r.catches / total))) : QStringLiteral("-");
  struct Row {
    QString k, v;
  };
  const Row left[] = {{QStringLiteral("Catches"), QString::number(r.catches)},
                      {QStringLiteral("Accuracy"), acc},
                      {QStringLiteral("Best combo"), QStringLiteral("%1  (x%2)").arg(r.bestCombo).arg(static_cast<double>(r.maxMultiplier), 0, 'g', 3)}};
  const Row right[] = {{QStringLiteral("Perfect"), QString::number(r.perfect)},
                       {QStringLiteral("Clutch"), QString::number(r.clutch)},
                       {QStringLiteral("Golden"), QString::number(r.golden)}};
  for (int i = 0; i < 3; ++i) {
    const double y = 668 + i * 50;
    // two blocks: [label ........ value]   gap   [label ........ value]
    drawShadowText(p, QRectF(260, y, 260, 44), Qt::AlignLeft | Qt::AlignVCenter, left[i].k, Theme::font(30, false), Theme::inkDim(), 0);
    drawShadowText(p, QRectF(520, y, 300, 44), Qt::AlignRight | Qt::AlignVCenter, left[i].v, Theme::font(32), Theme::ink(), 2);
    drawShadowText(p, QRectF(1000, y, 260, 44), Qt::AlignLeft | Qt::AlignVCenter, right[i].k, Theme::font(30, false), Theme::inkDim(), 0);
    drawShadowText(p, QRectF(1260, y, 300, 44), Qt::AlignRight | Qt::AlignVCenter, right[i].v, Theme::font(32), Theme::ink(), 2);
  }
  // rewards
  const Profile& pr = app_.profile();
  drawShadowText(p, QRectF(kW / 2 - 700, 822, 760, 46), Qt::AlignLeft | Qt::AlignVCenter,
                 QStringLiteral("+%1 coins     +%2 XP     Level %3").arg(r.coins).arg(r.xp).arg(pr.playerLevel()), Theme::font(32), Theme::gold(), 2);
  const double frac = pr.xpForNextLevel() > 0 ? static_cast<double>(pr.xpIntoLevel()) / pr.xpForNextLevel() : 0.0;
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0x14, 0x10, 0x28, 200));
  p->drawRoundedRect(QRectF(kW / 2 + 90, 836, 590, 20), 10, 10);
  p->setBrush(Theme::accent());
  p->drawRoundedRect(QRectF(kW / 2 + 90, 836, 590 * std::clamp(frac, 0.0, 1.0), 20), 10, 10);
  if (data_.summary.levelAfter > data_.summary.levelBefore) {
    drawShadowText(p, QRectF(kW / 2 + 90, 790, 590, 40), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("LEVEL UP!"), Theme::font(32), Theme::good(), 2);
  }
}

}  // namespace er
