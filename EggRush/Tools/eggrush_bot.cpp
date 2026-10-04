// Headless balance runner. Plays levels with bots that use the same movement model as a
// player and only know an egg after its telegraph plus a reaction delay.
//
//   eggrush_bot --level all --bot expert --preset normal --runs 200
//   eggrush_bot --calibrate --write        (stores median expert score as parScore)
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include "er/Bot.hpp"
#include "er/Content.hpp"

using namespace er;

namespace {

struct Agg {
  int runs = 0;
  int cleared = 0;
  int objective = 0;
  int flawless = 0;
  std::vector<double> scores;
  double misses = 0;
  double repairs = 0;
  double clutch = 0;
  double perfectShare = 0;
};

double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

bool pickBot(const QString& name, BotProfile& out) {
  if (name == QLatin1String("oracle")) out = kOracleBot;
  else if (name == QLatin1String("novice")) out = kNoviceBot;
  else if (name == QLatin1String("average")) out = kAverageBot;
  else if (name == QLatin1String("expert")) out = kExpertBot;
  else return false;
  return true;
}

bool pickPreset(const QString& name, Preset& out) {
  if (name == QLatin1String("chill")) out = Preset::Chill;
  else if (name == QLatin1String("normal")) out = Preset::Normal;
  else if (name == QLatin1String("hardcore")) out = Preset::Hardcore;
  else return false;
  return true;
}

Agg runMany(const ContentDb& db, const LevelDef& level, Preset preset, const BotProfile& bot, int runs,
            uint64_t seed0) {
  Agg a;
  for (int i = 0; i < runs; ++i) {
    const BotRunStats st = simulateRun(db.game, level, preset, seed0 + static_cast<uint64_t>(i) * 7919ULL, bot);
    ++a.runs;
    a.cleared += st.result.cleared ? 1 : 0;
    a.objective += st.result.objectiveMet ? 1 : 0;
    a.flawless += st.result.flawless ? 1 : 0;
    a.scores.push_back(static_cast<double>(st.result.score));
    a.misses += st.result.misses;
    a.repairs += st.repairs;
    a.clutch += st.result.clutch;
    const int c = std::max(1, st.result.catches);
    a.perfectShare += static_cast<double>(st.result.perfect) / c;
  }
  return a;
}

bool writePar(const QString& dir, const LevelDef& level, double par, QString& err) {
  const QDir ld(dir + QStringLiteral("/levels"));
  for (const QString& fn : ld.entryList({QStringLiteral("*.json")}, QDir::Files)) {
    QFile f(ld.filePath(fn));
    if (!f.open(QIODevice::ReadOnly)) continue;
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    f.close();
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) continue;
    QJsonObject o = doc.object();
    if (o.value(QStringLiteral("id")).toString().toStdString() != level.id) continue;
    o[QStringLiteral("parScore")] = par;
    QSaveFile out(ld.filePath(fn));
    if (!out.open(QIODevice::WriteOnly)) {
      err = out.errorString();
      return false;
    }
    out.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
      err = out.errorString();
      return false;
    }
    return true;
  }
  err = QStringLiteral("level file not found");
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCommandLineParser p;
  p.setApplicationDescription(QStringLiteral("Egg Rush headless balance bots"));
  p.addHelpOption();
  p.addOption({"data", "Data directory (default: built-in path)", "dir"});
  p.addOption({"level", "Level id or 'all'", "id", "all"});
  p.addOption({"bot", "oracle | novice | average | expert", "name", "average"});
  p.addOption({"preset", "chill | normal | hardcore", "name", "normal"});
  p.addOption({"runs", "Runs per level", "n", "100"});
  p.addOption({"seed", "First seed", "n", "1"});
  p.addOption({"calibrate", "Expert bot, Normal preset, 200 runs: report par scores"});
  p.addOption({"write", "With --calibrate: store par scores in the level JSON files"});
  p.process(app);

  QTextStream out(stdout);
  const QString dir = p.isSet("data") ? p.value("data") : ContentLoader::defaultDir();
  const ContentResult res = ContentLoader::load(dir);
  if (!res.ok) {
    for (const QString& e : res.errors) out << "content error: " << e << "\n";
    return 2;
  }
  const bool calibrate = p.isSet("calibrate");
  BotProfile bot = kAverageBot;
  Preset preset = Preset::Normal;
  if (calibrate) {
    bot = kExpertBot;
  } else if (!pickBot(p.value("bot"), bot)) {
    out << "unknown bot '" << p.value("bot") << "'\n";
    return 2;
  }
  if (!calibrate && !pickPreset(p.value("preset"), preset)) {
    out << "unknown preset '" << p.value("preset") << "'\n";
    return 2;
  }
  const int runs = calibrate ? 200 : std::max(1, p.value("runs").toInt());
  const uint64_t seed0 = p.value("seed").toULongLong();

  const char* presetNames[] = {"chill", "normal", "hardcore"};
  out << QString("%1 | %2 bot | %3 runs/level | preset %4 (%5 hearts)\n")
             .arg(calibrate ? "calibrate" : "balance", bot.name)
             .arg(runs)
             .arg(presetNames[static_cast<int>(preset)])
             .arg(presetDef(preset).startHalfHearts / 2.0);
  out << "level  clear%  clearLB  obj%  flawless%  medianScore  misses  repairs  clutch  perfect%\n";
  int rc = 0;
  for (const LevelDef& level : res.db.levels) {
    if (p.value("level") != QLatin1String("all") && QString::fromStdString(level.id) != p.value("level")) continue;
    const Agg a = runMany(res.db, level, preset, bot, runs, seed0);
    const double n = a.runs;
    const double lb = wilsonLowerBound(a.cleared, a.runs);
    const double med = median(a.scores);
    out << QString("%1  %2  %3  %4  %5  %6  %7  %8  %9  %10\n")
               .arg(QString::fromStdString(level.id), -5)
               .arg(100.0 * a.cleared / n, 6, 'f', 1)
               .arg(100.0 * lb, 7, 'f', 1)
               .arg(100.0 * a.objective / n, 5, 'f', 1)
               .arg(100.0 * a.flawless / n, 9, 'f', 1)
               .arg(med, 11, 'f', 0)
               .arg(a.misses / n, 6, 'f', 2)
               .arg(a.repairs / n, 7, 'f', 2)
               .arg(a.clutch / n, 6, 'f', 2)
               .arg(100.0 * a.perfectShare / n, 8, 'f', 1);
    if (calibrate && p.isSet("write")) {
      QString err;
      if (!writePar(dir, level, med, err)) {
        out << "  could not write par: " << err << "\n";
        rc = 1;
      }
    }
  }
  out.flush();
  return rc;
}
