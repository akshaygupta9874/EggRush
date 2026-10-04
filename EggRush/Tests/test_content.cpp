// Content loading and validation (Section 49): good data loads, bad data produces
// readable, specific errors with "did you mean" hints.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "TestUtil.hpp"

using namespace er;

namespace {

bool copyTree(const QString& from, const QString& to) {
  QDir().mkpath(to);
  const QDir src(from);
  for (const QFileInfo& fi : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
    const QString dst = to + "/" + fi.fileName();
    if (fi.isDir()) {
      if (!copyTree(fi.absoluteFilePath(), dst)) return false;
    } else if (!QFile::copy(fi.absoluteFilePath(), dst)) {
      return false;
    }
  }
  return true;
}

QByteArray readAll(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeAll(const QString& path, const QByteArray& data) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(data);
}

}  // namespace

class TestContent : public QObject {
  Q_OBJECT
 private slots:
  void realDataLoadsCleanly() {
    const ContentResult r = ContentLoader::load(QStringLiteral(ER_DATA_DIR));
    QVERIFY2(r.ok, qPrintable(r.errors.join("\n")));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.db.game.birds.size(), size_t(3));
    QVERIFY(r.db.levels.size() >= 6);
  }

  void levelsAreInCampaignOrder() {
    const ContentDb& db = ertest::realContent();
    const char* expect[] = {"1-1", "1-2", "1-3", "1-5", "1-6", "1-9"};
    QVERIFY(db.levels.size() >= 6);
    for (size_t i = 0; i < 6; ++i) QCOMPARE(db.levels[i].id, std::string(expect[i]));
    QCOMPARE(db.levelIndex("1-5"), 3);
    QCOMPARE(db.levelIndex("9-9"), -1);
    QVERIFY(db.findLevel("1-9") != nullptr);
    QVERIFY(db.findLevel("nope") == nullptr);
  }

  void eggStatsMatchSection69() {
    const GameData& g = ertest::realContent().game;
    const EggDef& golden = g.eggs[static_cast<size_t>(EggType::Golden)];
    QVERIFY(golden.bonus);
    QCOMPARE(golden.missHalfHearts, 0);
    QCOMPARE(golden.baseScore, 100);
    QCOMPARE(g.eggs[static_cast<size_t>(EggType::Heavy)].missHalfHearts, 2);
    QCOMPARE(g.eggs[static_cast<size_t>(EggType::Fast)].gravityScale, 1.6f);
    QCOMPARE(g.eggs[static_cast<size_t>(EggType::Fast)].launchVelPxPerSec, 500.f);
    QCOMPARE(g.birds[1].moveSpeedPxPerSec, 180.f);
    QCOMPARE(g.birds[2].moveSpeedPxPerSec, 320.f);
  }

  void firstLevelFollowsSection53And68() {
    const LevelDef& l = *ertest::realContent().findLevel("1-1");
    QVERIFY(l.forgiveFirstMiss);
    QCOMPARE(l.birds.size(), size_t(1));
    // Spawn x is confined to 560 ... 1360 (Section 68 note).
    const float xmin = cfg::kSlotOffsetPx + cfg::kSlotPitchPx * static_cast<float>(l.birds[0].laneMin);
    const float xmax = cfg::kSlotOffsetPx + cfg::kSlotPitchPx * static_cast<float>(l.birds[0].laneMax);
    QVERIFY(xmin >= 560.f && xmax <= 1360.f);
    QCOMPARE(l.objective.type, ObjectiveType::CatchEggs);
    QCOMPARE(l.objective.target, 20);
    QCOMPARE(l.scripted.size(), size_t(1));
  }

  void everyLevelHasAnObjective() {
    for (const LevelDef& l : ertest::realContent().levels) {
      QVERIFY2(l.objective.type != ObjectiveType::None, l.id.c_str());
      QVERIFY(l.objective.target > 0);
    }
  }

  void closestMatchHints() {
    const QStringList c = {"hen", "moving", "fast"};
    QCOMPARE(ContentLoader::closestMatch("hne", c), QString("hen"));
    QCOMPARE(ContentLoader::closestMatch("movin", c), QString("moving"));
    QCOMPARE(ContentLoader::closestMatch("completely-unrelated-word", c), QString());
  }

  void typoInBirdNameGetsAHint() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(copyTree(QStringLiteral(ER_DATA_DIR), tmp.path()));
    QByteArray lvl = readAll(tmp.path() + "/levels/1-2.json");
    QVERIFY(lvl.contains("\"hen\""));
    lvl.replace("\"hen\"", "\"hne\"");
    writeAll(tmp.path() + "/levels/1-2.json", lvl);
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    const QString all = r.errors.join("\n");
    QVERIFY2(all.contains("'hne'") && all.contains("did you mean 'hen'"), qPrintable(all));
  }

  void malformedJsonReportsFileAndOffset() {
    QTemporaryDir tmp;
    QVERIFY(copyTree(QStringLiteral(ER_DATA_DIR), tmp.path()));
    writeAll(tmp.path() + "/birds.json", "[ { \"id\": \"hen\", ");
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    QVERIFY2(r.errors.join("\n").contains("birds.json: JSON error at byte"), qPrintable(r.errors.join("\n")));
  }

  void outOfRangeNumbersAreFlaggedAndClamped() {
    QTemporaryDir tmp;
    QVERIFY(copyTree(QStringLiteral(ER_DATA_DIR), tmp.path()));
    QByteArray b = readAll(tmp.path() + "/birds.json");
    b.replace("\"telegraph\": 0.45", "\"telegraph\": 0.05");
    writeAll(tmp.path() + "/birds.json", b);
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    QVERIFY2(r.errors.join("\n").contains("telegraph"), qPrintable(r.errors.join("\n")));
    // The telegraph minimum protects fairness (Section 63): it must have been clamped up.
    for (const BirdDef& bd : r.db.game.birds) QVERIFY(bd.telegraphSec >= cfg::kMinTelegraphSec);
  }

  void missingFilesAreReported() {
    QTemporaryDir tmp;
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    const QString all = r.errors.join("\n");
    QVERIFY(all.contains("eggs.json") && all.contains("birds.json") && all.contains("levels/"));
  }

  void unknownObjectiveGetsAHint() {
    QTemporaryDir tmp;
    QVERIFY(copyTree(QStringLiteral(ER_DATA_DIR), tmp.path()));
    QByteArray lvl = readAll(tmp.path() + "/levels/1-5.json");
    lvl.replace("comboReach", "comboReech");
    writeAll(tmp.path() + "/levels/1-5.json", lvl);
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    QVERIFY2(r.errors.join("\n").contains("did you mean 'comboReach'"), qPrintable(r.errors.join("\n")));
  }

  void duplicateLevelIdsAreRejected() {
    QTemporaryDir tmp;
    QVERIFY(copyTree(QStringLiteral(ER_DATA_DIR), tmp.path()));
    QVERIFY(QFile::copy(tmp.path() + "/levels/1-2.json", tmp.path() + "/levels/1-2-copy.json"));
    const ContentResult r = ContentLoader::load(tmp.path());
    QVERIFY(!r.ok);
    QVERIFY2(r.errors.join("\n").contains("duplicate level id"), qPrintable(r.errors.join("\n")));
  }
};

QTEST_GUILESS_MAIN(TestContent)
#include "test_content.moc"
