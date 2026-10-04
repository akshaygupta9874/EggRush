#pragma once
// Audio without audio assets: every effect and the music loop are synthesised at start-up,
// written once to the cache folder as small WAV files and played through QSoundEffect.
// If there is no audio device the engine quietly does nothing.
#include <QObject>
#include <QSoundEffect>
#include <QString>
#include <map>
#include <memory>
#include <vector>

namespace er {

enum class Sfx : int {
  UiMove,
  UiConfirm,
  UiBack,
  Telegraph,
  Catch,  // 8 pitch variants: pass the combo-derived step as `variant`
  Perfect,
  Clutch,
  Golden,
  Rim,
  Miss,
  MissHeavy,
  HeartLost,
  HeartGained,
  Wave,
  Objective,
  LevelClear,
  GameOver,
  Count
};

class AudioEngine : public QObject {
 public:
  AudioEngine(const QString& cacheDir, bool enabled, QObject* parent = nullptr);
  ~AudioEngine() override;

  bool enabled() const { return enabled_; }
  void setVolumes(float master, float sfx, float music, bool muted);
  void play(Sfx s, int variant = 0);
  void startMusic();
  void stopMusic();
  void duckMusic(bool ducked);

 private:
  struct Voice {
    std::vector<std::unique_ptr<QSoundEffect>> pool;
    size_t next = 0;
  };
  void build(const QString& cacheDir);
  Voice& voice(int key) { return voices_[key]; }
  void applyVolumes();

  bool enabled_;
  float master_ = 0.8f, sfx_ = 1.f, music_ = 0.6f;
  bool muted_ = false, ducked_ = false;
  std::map<int, Voice> voices_;
  std::unique_ptr<QSoundEffect> music_fx_;
};

}  // namespace er
