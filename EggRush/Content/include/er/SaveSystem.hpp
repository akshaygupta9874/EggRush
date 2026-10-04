#pragma once
// Player profile + crash-safe persistence (Section 52 / 60). Qt Core only.
//  - QSaveFile: the file is replaced atomically, never half-written.
//  - SHA-256 checksum over the payload: bit rot or hand edits are detected.
//  - 3 rotating backups (profile.bak1..3), tried in order when the main file is bad.
//  - SessionGuard: QLockFile (one instance) + running.lock sentinel (crash detection).
#include <QLockFile>
#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>

#include "er/Content.hpp"
#include "er/World.hpp"

namespace er {

inline constexpr int kProfileVersion = 1;

// Three stars per level (Section 68): Clear, Objective, Flawless. Rank is separate.
struct LevelRecord {
  int64_t bestScore = 0;
  int bestCombo = 0;
  int bestRank = 0;  // 0 none, 1 C, 2 B, 3 A, 4 S
  bool cleared = false;
  bool objective = false;
  bool flawless = false;
  int plays = 0;

  int stars() const { return (cleared ? 1 : 0) + (objective ? 1 : 0) + (flawless ? 1 : 0); }
};

struct Settings {
  float masterVolume = 0.8f;
  float sfxVolume = 1.0f;
  float musicVolume = 0.6f;
  bool muted = false;
  bool screenShake = true;
  bool reducedMotion = false;
  bool highContrast = false;
  bool showFps = false;
  bool fullscreen = false;
};

struct Profile {
  int version = kProfileVersion;
  int coins = 0;
  int xp = 0;
  Preset preset = Preset::Normal;
  QString lastLevel = QStringLiteral("1-1");
  QMap<QString, LevelRecord> levels;
  Settings settings;
  int64_t totalRuns = 0;
  int64_t totalCatches = 0;
  int64_t totalMisses = 0;
  int bestComboEver = 0;
  double playSec = 0.0;

  int playerLevel() const;
  int xpIntoLevel() const;
  int xpForNextLevel() const;
  // A level is playable when it is the first one or the previous one was cleared.
  bool isUnlocked(const ContentDb& db, int levelIndex) const;
};

struct RunSummary {
  bool newBest = false;
  bool firstClear = false;
  int starsNow = 0;
  int starsBefore = 0;
  int rankNow = 0;     // rank earned in this run (0 when not cleared)
  bool newRank = false;
  int levelBefore = 1;
  int levelAfter = 1;
};

// C = cleared; B >= 0.55 x par; A >= 0.75 x par; S >= 0.90 x par (Section 69).
// With no par yet (parScore == 0) every clear is a C.
int rankFor(const LevelDef& level, const RunResult& r);
char rankLetter(int rank);  // 'C','B','A','S' or '-'
int totalStars(const Profile& p);
// Folds a finished run into the profile (best score, stars, coins, xp, lifetime stats).
RunSummary recordRun(Profile& p, const LevelDef& level, const RunResult& r);

struct LoadReport {
  enum class Source { Fresh, Primary, Backup };
  Source source = Source::Fresh;
  int backupIndex = 0;  // 1..3 when source == Backup
  QStringList notes;    // human readable, shown in the log / title screen
};

class SaveSystem {
 public:
  explicit SaveSystem(QString dir);

  QString dir() const { return dir_; }
  QString path() const { return dir_ + QStringLiteral("/profile.json"); }
  QString backupPath(int i) const { return dir_ + QStringLiteral("/profile.bak%1").arg(i); }

  LoadReport load(Profile& out) const;
  bool save(const Profile& p, QString* error = nullptr) const;

  static QByteArray serialize(const Profile& p);
  static bool deserialize(const QByteArray& bytes, Profile& out, QString* error);

  static QString defaultDir();  // QStandardPaths::AppDataLocation

 private:
  QString dir_;
};

class SessionGuard {
 public:
  explicit SessionGuard(QString dir);
  ~SessionGuard();
  SessionGuard(const SessionGuard&) = delete;
  SessionGuard& operator=(const SessionGuard&) = delete;

  // false if another live instance holds the lock.
  bool acquire();
  bool previousSessionCrashed() const { return crashed_; }

 private:
  QString dir_;
  std::unique_ptr<QLockFile> lock_;
  bool crashed_ = false;
  bool held_ = false;
};

}  // namespace er
