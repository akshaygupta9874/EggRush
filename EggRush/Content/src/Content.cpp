#include "er/Content.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <array>
#include <vector>

namespace er {

namespace {

int editDistance(const QString& a, const QString& b) {
  const int n = static_cast<int>(a.size());
  const int m = static_cast<int>(b.size());
  std::vector<int> prev(static_cast<size_t>(m) + 1);
  std::vector<int> cur(static_cast<size_t>(m) + 1);
  for (int j = 0; j <= m; ++j) prev[static_cast<size_t>(j)] = j;
  for (int i = 1; i <= n; ++i) {
    cur[0] = i;
    for (int j = 1; j <= m; ++j) {
      const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
      cur[static_cast<size_t>(j)] = std::min({prev[static_cast<size_t>(j)] + 1,
                                              cur[static_cast<size_t>(j - 1)] + 1,
                                              prev[static_cast<size_t>(j - 1)] + cost});
    }
    prev.swap(cur);
  }
  return prev[static_cast<size_t>(m)];
}

bool readJson(const QString& path, QJsonDocument& doc, QStringList& errors) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    errors << QStringLiteral("%1: cannot open file").arg(QFileInfo(path).fileName());
    return false;
  }
  QJsonParseError pe;
  doc = QJsonDocument::fromJson(f.readAll(), &pe);
  if (pe.error != QJsonParseError::NoError) {
    errors << QStringLiteral("%1: JSON error at byte %2: %3")
                  .arg(QFileInfo(path).fileName())
                  .arg(pe.offset)
                  .arg(pe.errorString());
    return false;
  }
  return true;
}

// Reads a number, reports (and clamps) values outside [lo, hi].
double num(const QJsonObject& o, const char* key, double def, double lo, double hi,
           const QString& ctx, QStringList& errors) {
  if (!o.contains(QLatin1String(key))) return def;
  const QJsonValue v = o.value(QLatin1String(key));
  if (!v.isDouble()) {
    errors << QStringLiteral("%1: '%2' must be a number").arg(ctx, QLatin1String(key));
    return def;
  }
  const double d = v.toDouble();
  if (d < lo || d > hi) {
    errors << QStringLiteral("%1: '%2' = %3 is outside %4 ... %5")
                  .arg(ctx, QLatin1String(key))
                  .arg(d)
                  .arg(lo)
                  .arg(hi);
    return std::clamp(d, lo, hi);
  }
  return d;
}

bool eggTypeFromString(const QString& s, EggType& out) {
  for (int i = 0; i < kEggTypeCount; ++i) {
    if (s.compare(QLatin1String(eggTypeName(static_cast<EggType>(i))), Qt::CaseInsensitive) == 0) {
      out = static_cast<EggType>(i);
      return true;
    }
  }
  return false;
}

bool objectiveTypeFromString(const QString& s, ObjectiveType& out) {
  struct Row {
    const char* name;
    ObjectiveType type;
  };
  static const Row rows[] = {{"catchEggs", ObjectiveType::CatchEggs},
                             {"perfectStreak", ObjectiveType::PerfectStreak},
                             {"goldenCatches", ObjectiveType::GoldenCatches},
                             {"comboReach", ObjectiveType::ComboReach},
                             {"birdStreak", ObjectiveType::BirdStreak}};
  for (const Row& r : rows) {
    if (s == QLatin1String(r.name)) {
      out = r.type;
      return true;
    }
  }
  return false;
}

QStringList objectiveTypeNames() {
  return {QStringLiteral("catchEggs"), QStringLiteral("perfectStreak"), QStringLiteral("goldenCatches"),
          QStringLiteral("comboReach"), QStringLiteral("birdStreak")};
}

}  // namespace

const LevelDef* ContentDb::findLevel(const QString& id) const {
  const int i = levelIndex(id);
  return i >= 0 ? &levels[static_cast<size_t>(i)] : nullptr;
}

int ContentDb::levelIndex(const QString& id) const {
  for (size_t i = 0; i < levels.size(); ++i) {
    if (QString::fromStdString(levels[i].id) == id) return static_cast<int>(i);
  }
  return -1;
}

QString ContentLoader::closestMatch(const QString& word, const QStringList& candidates) {
  QString best;
  int bestD = 1 << 30;
  for (const QString& c : candidates) {
    const int d = editDistance(word.toLower(), c.toLower());
    if (d < bestD) {
      bestD = d;
      best = c;
    }
  }
  return bestD <= std::max(2, static_cast<int>(word.size() / 2)) ? best : QString();
}

QString ContentLoader::defaultDir() {
  const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
  if (QFileInfo::exists(beside + QStringLiteral("/eggs.json"))) return beside;
#ifdef ER_DATA_DIR
  return QStringLiteral(ER_DATA_DIR);
#else
  return beside;
#endif
}

ContentResult ContentLoader::load(const QString& dir) {
  ContentResult res;
  QStringList& errors = res.errors;
  ContentDb& db = res.db;

  // ---- eggs.json ----------------------------------------------------------
  QStringList eggIds;
  {
    QJsonDocument doc;
    if (readJson(dir + QStringLiteral("/eggs.json"), doc, errors) && doc.isArray()) {
      std::array<bool, kEggTypeCount> seen{};
      for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const QString ctx = QStringLiteral("eggs.json: egg '%1'").arg(id);
        EggType type = EggType::Normal;
        const QString typeName = o.value(QStringLiteral("type")).toString();
        if (!eggTypeFromString(typeName, type)) {
          errors << QStringLiteral("%1: unknown type '%2'").arg(ctx, typeName);
          continue;
        }
        if (seen[static_cast<size_t>(type)]) {
          errors << QStringLiteral("%1: duplicate egg type '%2'").arg(ctx, typeName);
          continue;
        }
        seen[static_cast<size_t>(type)] = true;
        EggDef d;
        d.id = id.toStdString();
        d.type = type;
        d.gravityScale = static_cast<float>(num(o, "gravityScale", 1.0, 0.2, 4.0, ctx, errors));
        d.launchVelPxPerSec = static_cast<float>(num(o, "launchVelocity", 0.0, 0.0, 2000.0, ctx, errors));
        d.baseScore = static_cast<int>(num(o, "score", 10, 0, 100000, ctx, errors));
        d.coins = static_cast<int>(num(o, "coins", 1, 0, 1000, ctx, errors));
        d.threatUnits = static_cast<float>(num(o, "threat", 1.0, 0.0, 20.0, ctx, errors));
        d.missHalfHearts = static_cast<int>(num(o, "missHalfHearts", 1, 0, 10, ctx, errors));
        d.bonus = o.value(QStringLiteral("bonus")).toBool(false);
        if (d.bonus) d.missHalfHearts = 0;  // bonus eggs never cost hearts (Section 1)
        db.game.eggs[static_cast<size_t>(type)] = d;
        eggIds << id;
      }
      for (int i = 0; i < kEggTypeCount; ++i) {
        if (!seen[static_cast<size_t>(i)]) {
          errors << QStringLiteral("eggs.json: missing egg type '%1'")
                        .arg(QLatin1String(eggTypeName(static_cast<EggType>(i))));
        }
      }
    } else if (errors.isEmpty()) {
      errors << QStringLiteral("eggs.json: expected an array");
    }
  }

  // ---- birds.json ---------------------------------------------------------
  QStringList birdIds;
  {
    QJsonDocument doc;
    if (readJson(dir + QStringLiteral("/birds.json"), doc, errors) && doc.isArray()) {
      for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const QString ctx = QStringLiteral("birds.json: bird '%1'").arg(id);
        if (id.isEmpty() || birdIds.contains(id)) {
          errors << QStringLiteral("%1: missing or duplicate id").arg(ctx);
          continue;
        }
        BirdDef b;
        b.id = id.toStdString();
        b.displayName = o.value(QStringLiteral("name")).toString(id).toStdString();
        b.moveSpeedPxPerSec = static_cast<float>(num(o, "moveSpeed", 1800, 20, 6000, ctx, errors));
        b.telegraphSec = static_cast<float>(
            num(o, "telegraph", 0.45, cfg::kMinTelegraphSec, 2.0, ctx, errors));
        db.game.birds.push_back(b);
        birdIds << id;
      }
    }
  }

  // ---- levels/*.json ------------------------------------------------------
  struct Ordered {
    int order;
    LevelDef def;
  };
  std::vector<Ordered> levels;
  {
    const QDir ld(dir + QStringLiteral("/levels"));
    const QStringList files = ld.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) errors << QStringLiteral("levels/: no level files found in %1").arg(ld.path());
    for (const QString& fn : files) {
      QJsonDocument doc;
      if (!readJson(ld.filePath(fn), doc, errors) || !doc.isObject()) continue;
      const QJsonObject o = doc.object();
      const QString id = o.value(QStringLiteral("id")).toString();
      const QString ctx = QStringLiteral("levels/%1: level '%2'").arg(fn, id);
      LevelDef l;
      l.id = id.toStdString();
      l.name = o.value(QStringLiteral("name")).toString(id).toStdString();
      l.world = static_cast<int>(num(o, "world", 1, 1, 99, ctx, errors));
      l.durationSec = static_cast<float>(num(o, "duration", 60, 5, 3600, ctx, errors));
      l.firstDropSec = static_cast<float>(num(o, "firstDrop", cfg::kFirstDropSec, 0.5, 5.0, ctx, errors));
      l.forgivenessPx = static_cast<float>(num(o, "forgivenessPx", cfg::kDefaultForgivenessPx, 0, 20, ctx, errors));
      l.coinMultiplier = static_cast<float>(num(o, "coinMultiplier", 1.0, 0.1, 10.0, ctx, errors));
      l.parScore = num(o, "parScore", 0, 0, 1e9, ctx, errors);
      const int order = static_cast<int>(num(o, "order", 1000, 0, 100000, ctx, errors));

      const QJsonObject mix = o.value(QStringLiteral("eggMix")).toObject();
      l.eggMix = {0.f, 0.f, 0.f, 0.f};
      float total = 0.f;
      for (auto it = mix.begin(); it != mix.end(); ++it) {
        EggType t = EggType::Normal;
        if (!eggTypeFromString(it.key(), t)) {
          const QString hint = closestMatch(it.key(), eggIds);
          errors << QStringLiteral("%1: eggMix has unknown egg '%2'%3")
                        .arg(ctx, it.key(),
                             hint.isEmpty() ? QString() : QStringLiteral("; did you mean '%1'?").arg(hint));
          continue;
        }
        const float w = static_cast<float>(it.value().toDouble());
        l.eggMix[static_cast<size_t>(t)] = w;
        total += w;
      }
      if (total <= 0.f) errors << QStringLiteral("%1: eggMix is empty").arg(ctx);

      for (const QJsonValue& v : o.value(QStringLiteral("scripted")).toArray()) {
        const QJsonObject s = v.toObject();
        EggType t = EggType::Golden;
        const QString name = s.value(QStringLiteral("egg")).toString();
        if (!eggTypeFromString(name, t)) {
          errors << QStringLiteral("%1: scripted egg '%2' is unknown").arg(ctx, name);
          continue;
        }
        l.scripted.push_back({s.value(QStringLiteral("index")).toInt(), t});
      }

      for (const QJsonValue& v : o.value(QStringLiteral("birds")).toArray()) {
        const QJsonObject b = v.toObject();
        const QString birdName = b.value(QStringLiteral("bird")).toString();
        const int idx = static_cast<int>(birdIds.indexOf(birdName));
        if (idx < 0) {
          const QString hint = closestMatch(birdName, birdIds);
          errors << QStringLiteral("%1: bird '%2' is unknown%3")
                        .arg(ctx, birdName,
                             hint.isEmpty() ? QString() : QStringLiteral("; did you mean '%1'?").arg(hint));
          continue;
        }
        LevelBird lb;
        lb.birdDef = idx;
        lb.intervalSec = static_cast<float>(num(b, "interval", 2.6, 0.4, 30, ctx, errors));
        lb.intervalJitter = static_cast<float>(num(b, "jitter", 0.2, 0.0, 0.5, ctx, errors));
        lb.startOffsetSec = static_cast<float>(num(b, "startOffset", 0.0, 0.0, 60, ctx, errors));
        lb.laneMin = static_cast<int>(num(b, "laneMin", 0, 0, cfg::kSlotCount - 1, ctx, errors));
        lb.laneMax = static_cast<int>(num(b, "laneMax", cfg::kSlotCount - 1, 0, cfg::kSlotCount - 1, ctx, errors));
        lb.maxHopPx = static_cast<float>(num(b, "maxHopPx", 100000, 0, 100000, ctx, errors));
        if (lb.laneMax < lb.laneMin) {
          errors << QStringLiteral("%1: laneMax %2 is below laneMin %3").arg(ctx).arg(lb.laneMax).arg(lb.laneMin);
          std::swap(lb.laneMin, lb.laneMax);
        }
        l.birds.push_back(lb);
      }
      l.forgiveFirstMiss = o.value(QStringLiteral("forgiveFirstMiss")).toBool(false);
      if (o.contains(QStringLiteral("objective"))) {
        const QJsonObject ob = o.value(QStringLiteral("objective")).toObject();
        const QString tn = ob.value(QStringLiteral("type")).toString();
        if (!objectiveTypeFromString(tn, l.objective.type)) {
          const QString hint = closestMatch(tn, objectiveTypeNames());
          errors << QStringLiteral("%1: unknown objective type '%2'%3")
                        .arg(ctx, tn,
                             hint.isEmpty() ? QString() : QStringLiteral("; did you mean '%1'?").arg(hint));
        } else {
          l.objective.target = static_cast<int>(num(ob, "target", 1, 1, 100000, ctx, errors));
          l.objective.birdIndex = static_cast<int>(num(ob, "birdIndex", 0, 0, 7, ctx, errors));
          if (l.objective.type == ObjectiveType::BirdStreak &&
              l.objective.birdIndex >= static_cast<int>(l.birds.size())) {
            errors << QStringLiteral("%1: objective birdIndex %2 has no matching bird")
                          .arg(ctx)
                          .arg(l.objective.birdIndex);
          }
        }
      }
      if (l.birds.empty()) errors << QStringLiteral("%1: has no birds").arg(ctx);
      for (const Ordered& other : levels) {
        if (other.def.id == l.id) errors << QStringLiteral("%1: duplicate level id").arg(ctx);
      }
      levels.push_back({order, l});
    }
  }
  std::stable_sort(levels.begin(), levels.end(),
                   [](const Ordered& a, const Ordered& b) { return a.order < b.order; });
  for (Ordered& o : levels) db.levels.push_back(std::move(o.def));

  res.ok = errors.isEmpty() && !db.levels.empty();
  return res;
}

}  // namespace er
