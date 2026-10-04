#include "er/SaveSystem.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace er {

namespace {

constexpr int kBackups = 3;

QString hexSha256(const QByteArray& bytes) {
  return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QJsonObject toJson(const Settings& s) {
  QJsonObject o;
  o[QStringLiteral("masterVolume")] = static_cast<double>(s.masterVolume);
  o[QStringLiteral("sfxVolume")] = static_cast<double>(s.sfxVolume);
  o[QStringLiteral("musicVolume")] = static_cast<double>(s.musicVolume);
  o[QStringLiteral("muted")] = s.muted;
  o[QStringLiteral("screenShake")] = s.screenShake;
  o[QStringLiteral("reducedMotion")] = s.reducedMotion;
  o[QStringLiteral("highContrast")] = s.highContrast;
  o[QStringLiteral("showFps")] = s.showFps;
  o[QStringLiteral("fullscreen")] = s.fullscreen;
  return o;
}

float clamp01(double v, float def) {
  if (!std::isfinite(v)) return def;
  return static_cast<float>(std::clamp(v, 0.0, 1.0));
}

Settings settingsFromJson(const QJsonObject& o) {
  Settings s;
  s.masterVolume = clamp01(o.value(QStringLiteral("masterVolume")).toDouble(s.masterVolume), s.masterVolume);
  s.sfxVolume = clamp01(o.value(QStringLiteral("sfxVolume")).toDouble(s.sfxVolume), s.sfxVolume);
  s.musicVolume = clamp01(o.value(QStringLiteral("musicVolume")).toDouble(s.musicVolume), s.musicVolume);
  s.muted = o.value(QStringLiteral("muted")).toBool(s.muted);
  s.screenShake = o.value(QStringLiteral("screenShake")).toBool(s.screenShake);
  s.reducedMotion = o.value(QStringLiteral("reducedMotion")).toBool(s.reducedMotion);
  s.highContrast = o.value(QStringLiteral("highContrast")).toBool(s.highContrast);
  s.showFps = o.value(QStringLiteral("showFps")).toBool(s.showFps);
  s.fullscreen = o.value(QStringLiteral("fullscreen")).toBool(s.fullscreen);
  return s;
}

}  // namespace

// ---- Profile ----------------------------------------------------------------

int Profile::playerLevel() const {
  return 1 + static_cast<int>(std::floor(std::sqrt(std::max(0, xp) / 50.0)));
}

static int xpAtLevel(int level) {
  const int n = level - 1;
  return 50 * n * n;
}

int Profile::xpIntoLevel() const { return std::max(0, xp) - xpAtLevel(playerLevel()); }

int Profile::xpForNextLevel() const { return xpAtLevel(playerLevel() + 1) - xpAtLevel(playerLevel()); }

bool Profile::isUnlocked(const ContentDb& db, int levelIndex) const {
  if (levelIndex <= 0) return true;
  if (levelIndex >= static_cast<int>(db.levels.size())) return false;
  const QString prev = QString::fromStdString(db.levels[static_cast<size_t>(levelIndex - 1)].id);
  const auto it = levels.constFind(prev);
  return it != levels.constEnd() && it->cleared;
}

int rankFor(const LevelDef& level, const RunResult& r) {
  if (!r.cleared) return 0;
  if (level.parScore <= 0.0) return 1;
  const double s = static_cast<double>(r.score);
  if (s >= level.parScore * 0.90) return 4;
  if (s >= level.parScore * 0.75) return 3;
  if (s >= level.parScore * 0.55) return 2;
  return 1;
}

char rankLetter(int rank) {
  switch (rank) {
    case 1: return 'C';
    case 2: return 'B';
    case 3: return 'A';
    case 4: return 'S';
    default: break;
  }
  return '-';
}

int totalStars(const Profile& p) {
  int n = 0;
  for (auto it = p.levels.constBegin(); it != p.levels.constEnd(); ++it) n += it->stars();
  return n;
}

RunSummary recordRun(Profile& p, const LevelDef& level, const RunResult& r) {
  RunSummary out;
  out.levelBefore = p.playerLevel();
  LevelRecord& rec = p.levels[QString::fromStdString(level.id)];
  out.starsBefore = rec.stars();
  rec.plays += 1;
  if (r.score > rec.bestScore) {
    rec.bestScore = r.score;
    out.newBest = r.score > 0;
  }
  rec.bestCombo = std::max(rec.bestCombo, r.bestCombo);
  if (r.cleared) {
    out.firstClear = !rec.cleared;
    rec.cleared = true;
    rec.flawless = rec.flawless || r.flawless;
    rec.objective = rec.objective || r.objectiveMet;
  }
  out.rankNow = rankFor(level, r);
  out.newRank = out.rankNow > rec.bestRank;
  rec.bestRank = std::max(rec.bestRank, out.rankNow);
  out.starsNow = rec.stars();

  p.coins += std::max(0, r.coins);
  p.xp += std::max(0, r.xp);
  p.totalRuns += 1;
  p.totalCatches += r.catches;
  p.totalMisses += r.misses;
  p.bestComboEver = std::max(p.bestComboEver, r.bestCombo);
  p.playSec += r.timeSec;
  p.lastLevel = QString::fromStdString(level.id);
  out.levelAfter = p.playerLevel();
  return out;
}

// ---- serialization --------------------------------------------------------------

QByteArray SaveSystem::serialize(const Profile& p) {
  QJsonObject data;
  data[QStringLiteral("coins")] = p.coins;
  data[QStringLiteral("xp")] = p.xp;
  data[QStringLiteral("preset")] = static_cast<int>(p.preset);
  data[QStringLiteral("lastLevel")] = p.lastLevel;
  data[QStringLiteral("totalRuns")] = static_cast<double>(p.totalRuns);
  data[QStringLiteral("totalCatches")] = static_cast<double>(p.totalCatches);
  data[QStringLiteral("totalMisses")] = static_cast<double>(p.totalMisses);
  data[QStringLiteral("bestComboEver")] = p.bestComboEver;
  data[QStringLiteral("playSec")] = p.playSec;
  data[QStringLiteral("settings")] = toJson(p.settings);
  QJsonObject levels;
  for (auto it = p.levels.constBegin(); it != p.levels.constEnd(); ++it) {
    QJsonObject l;
    l[QStringLiteral("bestScore")] = static_cast<double>(it->bestScore);
    l[QStringLiteral("bestCombo")] = it->bestCombo;
    l[QStringLiteral("bestRank")] = it->bestRank;
    l[QStringLiteral("cleared")] = it->cleared;
    l[QStringLiteral("objective")] = it->objective;
    l[QStringLiteral("flawless")] = it->flawless;
    l[QStringLiteral("plays")] = it->plays;
    levels[it.key()] = l;
  }
  data[QStringLiteral("levels")] = levels;

  const QByteArray compact = QJsonDocument(data).toJson(QJsonDocument::Compact);
  QJsonObject root;
  root[QStringLiteral("version")] = kProfileVersion;
  root[QStringLiteral("checksum")] = hexSha256(compact);
  root[QStringLiteral("data")] = data;
  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool SaveSystem::deserialize(const QByteArray& bytes, Profile& out, QString* error) {
  auto fail = [&](const QString& why) {
    if (error != nullptr) *error = why;
    return false;
  };
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
  if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
    return fail(QStringLiteral("not valid JSON (%1)").arg(pe.errorString()));
  }
  const QJsonObject root = doc.object();
  const int version = root.value(QStringLiteral("version")).toInt(-1);
  if (version < 1) return fail(QStringLiteral("missing version"));
  if (version > kProfileVersion) {
    return fail(QStringLiteral("saved by a newer version of the game (v%1)").arg(version));
  }
  const QJsonObject data = root.value(QStringLiteral("data")).toObject();
  const QByteArray compact = QJsonDocument(data).toJson(QJsonDocument::Compact);
  if (hexSha256(compact) != root.value(QStringLiteral("checksum")).toString()) {
    return fail(QStringLiteral("checksum mismatch (file damaged or edited)"));
  }

  Profile p;
  p.version = version;
  p.coins = std::max(0, data.value(QStringLiteral("coins")).toInt(0));
  p.xp = std::max(0, data.value(QStringLiteral("xp")).toInt(0));
  const int preset = data.value(QStringLiteral("preset")).toInt(1);
  p.preset = static_cast<Preset>(std::clamp(preset, 0, 2));
  p.lastLevel = data.value(QStringLiteral("lastLevel")).toString(QStringLiteral("1-1"));
  p.totalRuns = static_cast<int64_t>(data.value(QStringLiteral("totalRuns")).toDouble(0));
  p.totalCatches = static_cast<int64_t>(data.value(QStringLiteral("totalCatches")).toDouble(0));
  p.totalMisses = static_cast<int64_t>(data.value(QStringLiteral("totalMisses")).toDouble(0));
  p.bestComboEver = std::max(0, data.value(QStringLiteral("bestComboEver")).toInt(0));
  p.playSec = std::max(0.0, data.value(QStringLiteral("playSec")).toDouble(0));
  p.settings = settingsFromJson(data.value(QStringLiteral("settings")).toObject());
  const QJsonObject levels = data.value(QStringLiteral("levels")).toObject();
  for (auto it = levels.begin(); it != levels.end(); ++it) {
    const QJsonObject l = it.value().toObject();
    LevelRecord r;
    r.bestScore = static_cast<int64_t>(l.value(QStringLiteral("bestScore")).toDouble(0));
    r.bestCombo = std::max(0, l.value(QStringLiteral("bestCombo")).toInt(0));
    r.bestRank = std::clamp(l.value(QStringLiteral("bestRank")).toInt(0), 0, 4);
    r.cleared = l.value(QStringLiteral("cleared")).toBool(false);
    r.objective = l.value(QStringLiteral("objective")).toBool(false);
    r.flawless = l.value(QStringLiteral("flawless")).toBool(false);
    r.plays = std::max(0, l.value(QStringLiteral("plays")).toInt(0));
    p.levels.insert(it.key(), r);
  }
  out = p;
  return true;
}

// ---- files ------------------------------------------------------------------------

SaveSystem::SaveSystem(QString dir) : dir_(std::move(dir)) {}

QString SaveSystem::defaultDir() {
  const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return d.isEmpty() ? QDir::homePath() + QStringLiteral("/.eggrush") : d;
}

LoadReport SaveSystem::load(Profile& out) const {
  LoadReport rep;
  out = Profile{};

  auto tryFile = [&](const QString& file, QString& why) {
    QFile f(file);
    if (!f.exists()) {
      why = QStringLiteral("missing");
      return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
      why = QStringLiteral("cannot be opened");
      return false;
    }
    return deserialize(f.readAll(), out, &why);
  };

  QString why;
  const bool mainExists = QFileInfo::exists(path());
  if (tryFile(path(), why)) {
    rep.source = LoadReport::Source::Primary;
    return rep;
  }
  if (mainExists) rep.notes << QStringLiteral("profile.json unusable: %1").arg(why);

  for (int i = 1; i <= kBackups; ++i) {
    if (!QFileInfo::exists(backupPath(i))) continue;
    QString bwhy;
    if (tryFile(backupPath(i), bwhy)) {
      rep.source = LoadReport::Source::Backup;
      rep.backupIndex = i;
      rep.notes << QStringLiteral("restored progress from backup %1").arg(i);
      return rep;
    }
    rep.notes << QStringLiteral("profile.bak%1 unusable: %2").arg(i).arg(bwhy);
  }
  out = Profile{};
  rep.source = LoadReport::Source::Fresh;
  if (mainExists) rep.notes << QStringLiteral("starting a fresh profile");
  return rep;
}

bool SaveSystem::save(const Profile& p, QString* error) const {
  auto fail = [&](const QString& why) {
    if (error != nullptr) *error = why;
    return false;
  };
  if (!QDir().mkpath(dir_)) return fail(QStringLiteral("cannot create %1").arg(dir_));

  // Rotate: bak2 -> bak3, bak1 -> bak2, main -> bak1 (only a readable main is kept).
  QString dummy;
  Profile probe;
  QFile cur(path());
  if (cur.exists() && cur.open(QIODevice::ReadOnly)) {
    const QByteArray old = cur.readAll();
    cur.close();
    if (deserialize(old, probe, &dummy)) {
      QFile::remove(backupPath(kBackups));
      for (int i = kBackups - 1; i >= 1; --i) QFile::rename(backupPath(i), backupPath(i + 1));
      QFile bak(backupPath(1));
      if (bak.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        bak.write(old);
        bak.close();
      }
    }
  }

  QSaveFile f(path());
  if (!f.open(QIODevice::WriteOnly)) return fail(f.errorString());
  f.write(serialize(p));
  if (!f.commit()) return fail(f.errorString());
  return true;
}

// ---- SessionGuard -------------------------------------------------------------------

SessionGuard::SessionGuard(QString dir) : dir_(std::move(dir)) {}

bool SessionGuard::acquire() {
  QDir().mkpath(dir_);
  lock_ = std::make_unique<QLockFile>(dir_ + QStringLiteral("/eggrush.lock"));
  lock_->setStaleLockTime(30000);
  if (!lock_->tryLock(100)) return false;
  held_ = true;
  const QString sentinel = dir_ + QStringLiteral("/running.lock");
  crashed_ = QFileInfo::exists(sentinel);
  QFile f(sentinel);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    f.write("running");
    f.close();
  }
  return true;
}

SessionGuard::~SessionGuard() {
  if (held_) {
    QFile::remove(dir_ + QStringLiteral("/running.lock"));
    lock_->unlock();
  }
}

}  // namespace er
