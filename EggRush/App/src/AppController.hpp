#pragma once
// Owns the profile, content, audio and the screen stack, and runs the frame loop.
// Screens talk to the rest of the game only through this class.
#include <QElapsedTimer>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "Audio.hpp"
#include "er/Content.hpp"
#include "er/SaveSystem.hpp"
#include "er/World.hpp"

namespace er {

class Screen;
class GameView;

struct LaunchOptions {
  QString dataDir;
  QString saveDir;
  QString level;             // start straight in this level
  QString screen;            // title | levels | settings | result | pause  (screenshots / QA)
  int preset = -1;           // 0..2 overrides the profile preset for this launch
  uint64_t seed = 0;
  bool seedSet = false;
  bool autoplay = false;     // a bot plays the level (smoke tests, demos)
  bool mute = false;
  QString render;            // "gl" | "software" | "" (auto)
  double quitAfterSec = 0;   // exit after this much wall-clock time (smoke tests)
  QString screenshotPath;    // grab the view to this file ...
  double screenshotAfterSec = 0;  // ... after this much wall-clock time
};

// Everything the result screen needs, captured when a run ends.
struct ResultData {
  int levelIndex = 0;
  RunResult run;
  RunSummary summary;
  bool hasNext = false;
};

class AppController : public QObject {
  Q_OBJECT
 public:
  AppController(LaunchOptions opts, ContentDb content, Profile profile, SaveSystem saves, QObject* parent = nullptr);
  ~AppController() override;

  void attachView(GameView* view);
  void start();  // builds the first screen according to the launch options

  QGraphicsScene* scene() { return &scene_; }
  GameView* view() { return view_; }
  Profile& profile() { return profile_; }
  const ContentDb& content() const { return content_; }
  AudioEngine& audio() { return *audio_; }
  const LaunchOptions& options() const { return opts_; }
  double clock() const { return clock_; }
  double fps() const { return fps_; }
  bool debugOverlay() const { return debug_; }
  // A short-lived message shown on every screen (screenshot saved, save problems, restored backup).
  void postNote(const QString& n, double sec = 8.0) {
    note_ = n;
    noteTimer_ = n.isEmpty() ? 0.0 : sec;
  }
  void setNote(const QString& n) { postNote(n, 10.0); }
  const QString& noteText() const { return note_; }
  double noteAlpha() const { return noteTimer_ <= 0.0 ? 0.0 : (noteTimer_ < 0.6 ? noteTimer_ / 0.6 : 1.0); }

  // ---- navigation -------------------------------------------------------------
  Screen* top() const { return stack_.empty() ? nullptr : stack_.back(); }
  void push(Screen* s);
  void pop();
  void replaceAll(Screen* s);
  void goTitle();
  void goLevelSelect(int focusLevel = -1);
  void startLevel(int levelIndex, bool useSeed = false);
  void showResult(const ResultData& r);
  void openSettings();
  void requestQuit();

  // ---- settings ----------------------------------------------------------------
  void applySettings();
  void saveProfile();
  Preset activePreset() const;

  // ---- input forwarded by the view -----------------------------------------------
  void keyPressed(QKeyEvent* e);
  void keyReleased(QKeyEvent* e);
  void mouseMoved(double sceneX, double sceneY);
  void windowDeactivated();
  bool grabScreenshot(const QString& path);

 signals:
  void fullscreenRequested(bool on);
  void quitRequested();

 private slots:
  void frame();

 private:
  void install(Screen* s);
  void uninstall(Screen* s);
  void updateVisibility();

  LaunchOptions opts_;
  ContentDb content_;
  Profile profile_;
  SaveSystem saves_;
  QGraphicsScene scene_;
  GameView* view_ = nullptr;
  std::unique_ptr<AudioEngine> audio_;
  std::vector<Screen*> stack_;
  QGraphicsObject* background_ = nullptr;
  QTimer timer_;
  QElapsedTimer wall_;
  QElapsedTimer sinceStart_;
  double clock_ = 0.0;
  double fps_ = 60.0;
  double fpsAccum_ = 0.0;
  int fpsFrames_ = 0;
  bool debug_ = false;
  bool shotTaken_ = false;
  QString note_;
  double noteTimer_ = 0.0;
};

}  // namespace er
