// Save system (Section 52 / 60): atomic writes, checksum, rotating backups, recovery,
// crash sentinel, single-instance lock, and the progression rules that live with it.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "TestUtil.hpp"
#include "er/SaveSystem.hpp"

using namespace er;

namespace {

Profile sampleProfile(int coins) {
  Profile p;
  p.coins = coins;
  p.xp = 1234;
  p.preset = Preset::Hardcore;
  p.lastLevel = "1-3";
  p.totalRuns = 17;
  p.totalCatches = 400;
  p.totalMisses = 25;
  p.bestComboEver = 61;
  p.playSec = 3600.5;
  p.settings.masterVolume = 0.3f;
  p.settings.muted = true;
  p.settings.screenShake = false;
  p.settings.highContrast = true;
  LevelRecord r;
  r.bestScore = 9876;
  r.bestCombo = 33;
  r.bestRank = 3;
  r.cleared = true;
  r.objective = true;
  r.plays = 5;
  p.levels.insert("1-1", r);
  return p;
}

void corrupt(const QString& path) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::ReadWrite));
  QByteArray b = f.readAll();
  b.replace("\"coins\"", "\"coinz\"");
  if (b.size() > 40) b[b.size() / 2] = '#';
  f.seek(0);
  f.resize(0);
  f.write(b);
}

}  // namespace

class TestSave : public QObject {
  Q_OBJECT
 private slots:
  void roundTripKeepsEveryField() {
    const Profile in = sampleProfile(250);
    Profile out;
    QString err;
    QVERIFY2(SaveSystem::deserialize(SaveSystem::serialize(in), out, &err), qPrintable(err));
    QCOMPARE(out.coins, 250);
    QCOMPARE(out.xp, 1234);
    QCOMPARE(out.preset, Preset::Hardcore);
    QCOMPARE(out.lastLevel, QString("1-3"));
    QCOMPARE(out.totalRuns, int64_t(17));
    QCOMPARE(out.bestComboEver, 61);
    QVERIFY(out.settings.muted && !out.settings.screenShake && out.settings.highContrast);
    QVERIFY(std::fabs(out.settings.masterVolume - 0.3f) < 1e-5f);
    const LevelRecord r = out.levels.value("1-1");
    QCOMPARE(r.bestScore, int64_t(9876));
    QCOMPARE(r.bestRank, 3);
    QVERIFY(r.cleared && r.objective && !r.flawless);
    QCOMPARE(r.stars(), 2);
  }

  void anyEditIsDetectedByTheChecksum() {
    QByteArray bytes = SaveSystem::serialize(sampleProfile(250));
    QVERIFY(bytes.contains("\"coins\": 250"));
    bytes.replace("\"coins\": 250", "\"coins\": 999999");
    Profile out;
    QString err;
    QVERIFY(!SaveSystem::deserialize(bytes, out, &err));
    QVERIFY2(err.contains("checksum"), qPrintable(err));
  }

  void garbageAndNewerVersionsAreRejected() {
    Profile out;
    QString err;
    QVERIFY(!SaveSystem::deserialize("not json at all", out, &err));
    QVERIFY(!SaveSystem::deserialize("{}", out, &err));
    QByteArray bytes = SaveSystem::serialize(sampleProfile(1));
    bytes.replace("\"version\": 1", "\"version\": 99");
    QVERIFY(!SaveSystem::deserialize(bytes, out, &err));
    QVERIFY2(err.contains("newer"), qPrintable(err));
  }

  void outOfRangeValuesAreClamped() {
    Profile p = sampleProfile(5);
    p.settings.masterVolume = 7.f;
    Profile out;
    QVERIFY(SaveSystem::deserialize(SaveSystem::serialize(p), out, nullptr));
    QVERIFY(out.settings.masterVolume <= 1.f);
  }

  void freshStartWhenNothingExists() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    Profile p = sampleProfile(5);
    const LoadReport rep = s.load(p);
    QCOMPARE(rep.source, LoadReport::Source::Fresh);
    QCOMPARE(p.coins, 0);
    QVERIFY(rep.notes.isEmpty());
  }

  void saveThenLoad() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    QString err;
    QVERIFY2(s.save(sampleProfile(100), &err), qPrintable(err));
    Profile p;
    const LoadReport rep = s.load(p);
    QCOMPARE(rep.source, LoadReport::Source::Primary);
    QCOMPARE(p.coins, 100);
  }

  void threeRotatingBackups() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    for (int i = 1; i <= 5; ++i) QVERIFY(s.save(sampleProfile(i * 100)));
    // main = 500, bak1 = 400, bak2 = 300, bak3 = 200. The very first save has rotated out.
    auto coinsOf = [&](const QString& path) {
      QFile f(path);
      Profile p;
      return (f.open(QIODevice::ReadOnly) && SaveSystem::deserialize(f.readAll(), p, nullptr)) ? p.coins : -1;
    };
    QCOMPARE(coinsOf(s.path()), 500);
    QCOMPARE(coinsOf(s.backupPath(1)), 400);
    QCOMPARE(coinsOf(s.backupPath(2)), 300);
    QCOMPARE(coinsOf(s.backupPath(3)), 200);
    QVERIFY(!QFileInfo::exists(tmp.path() + "/profile.bak4"));
  }

  void corruptMainRestoresFromBackup() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    QVERIFY(s.save(sampleProfile(100)));
    QVERIFY(s.save(sampleProfile(200)));
    corrupt(s.path());
    Profile p;
    const LoadReport rep = s.load(p);
    QCOMPARE(rep.source, LoadReport::Source::Backup);
    QCOMPARE(rep.backupIndex, 1);
    QCOMPARE(p.coins, 100);
    QVERIFY(!rep.notes.isEmpty());
  }

  void corruptBackupsAreSkipped() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    for (int i = 1; i <= 4; ++i) QVERIFY(s.save(sampleProfile(i * 10)));
    corrupt(s.path());
    corrupt(s.backupPath(1));
    Profile p;
    const LoadReport rep = s.load(p);
    QCOMPARE(rep.source, LoadReport::Source::Backup);
    QCOMPARE(rep.backupIndex, 2);
    QCOMPARE(p.coins, 20);
  }

  void everythingCorruptStartsFresh() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    for (int i = 1; i <= 4; ++i) QVERIFY(s.save(sampleProfile(i)));
    corrupt(s.path());
    for (int i = 1; i <= 3; ++i) corrupt(s.backupPath(i));
    Profile p = sampleProfile(5);
    const LoadReport rep = s.load(p);
    QCOMPARE(rep.source, LoadReport::Source::Fresh);
    QCOMPARE(p.coins, 0);
    QVERIFY(rep.notes.join(" ").contains("fresh"));
  }

  void aDamagedMainIsNeverRotatedIntoABackup() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    QVERIFY(s.save(sampleProfile(100)));
    QVERIFY(s.save(sampleProfile(200)));  // main 200, bak1 100
    corrupt(s.path());
    QVERIFY(s.save(sampleProfile(300)));  // damaged main must not overwrite bak1
    QFile f(s.backupPath(1));
    QVERIFY(f.open(QIODevice::ReadOnly));
    Profile p;
    QVERIFY(SaveSystem::deserialize(f.readAll(), p, nullptr));
    QCOMPARE(p.coins, 100);
  }

  void noTemporaryFilesAreLeftBehind() {
    QTemporaryDir tmp;
    SaveSystem s(tmp.path());
    for (int i = 0; i < 3; ++i) QVERIFY(s.save(sampleProfile(i)));
    for (const QString& n : QDir(tmp.path()).entryList(QDir::Files)) {
      QVERIFY2(n == "profile.json" || n.startsWith("profile.bak"), qPrintable(n));
    }
  }

  // ---- session guard -------------------------------------------------------------
  void sessionGuardRefusesASecondInstance() {
    QTemporaryDir tmp;
    SessionGuard a(tmp.path());
    QVERIFY(a.acquire());
    QVERIFY(!a.previousSessionCrashed());
    SessionGuard b(tmp.path());
    QVERIFY(!b.acquire());
  }

  void sessionGuardDetectsACrash() {
    QTemporaryDir tmp;
    {
      QFile f(tmp.path() + "/running.lock");  // left behind by a process that died
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("running");
    }
    {
      SessionGuard g(tmp.path());
      QVERIFY(g.acquire());
      QVERIFY(g.previousSessionCrashed());
    }
    QVERIFY(!QFileInfo::exists(tmp.path() + "/running.lock"));  // clean exit removes the sentinel
    SessionGuard again(tmp.path());
    QVERIFY(again.acquire());
    QVERIFY(!again.previousSessionCrashed());
  }

  // ---- progression -----------------------------------------------------------------
  void playerLevelCurve() {
    Profile p;
    QCOMPARE(p.playerLevel(), 1);
    p.xp = 49;
    QCOMPARE(p.playerLevel(), 1);
    p.xp = 50;
    QCOMPARE(p.playerLevel(), 2);
    p.xp = 200;
    QCOMPARE(p.playerLevel(), 3);
    QCOMPARE(p.xpIntoLevel() + 0, 0);
    QCOMPARE(p.xpForNextLevel(), 250);  // level 3 spans xp 200 .. 450
  }

  void levelsUnlockInOrder() {
    const ContentDb& db = ertest::realContent();
    Profile p;
    QVERIFY(p.isUnlocked(db, 0));
    QVERIFY(!p.isUnlocked(db, 1));
    LevelRecord r;
    r.cleared = true;
    p.levels.insert("1-1", r);
    QVERIFY(p.isUnlocked(db, 1));
    QVERIFY(!p.isUnlocked(db, 2));
    QVERIFY(!p.isUnlocked(db, 999));
  }

  void ranksUseParFractions() {
    LevelDef l;
    l.parScore = 1000;
    RunResult r;
    r.cleared = true;
    auto rank = [&](int64_t s) {
      r.score = s;
      return rankFor(l, r);
    };
    QCOMPARE(rank(100), 1);
    QCOMPARE(rank(550), 2);
    QCOMPARE(rank(749), 2);
    QCOMPARE(rank(750), 3);
    QCOMPARE(rank(899), 3);
    QCOMPARE(rank(900), 4);
    r.cleared = false;
    QCOMPARE(rank(5000), 0);
    l.parScore = 0;
    r.cleared = true;
    QCOMPARE(rank(5000), 1);  // no par calibrated yet: every clear is a C
    QCOMPARE(rankLetter(4), 'S');
    QCOMPARE(rankLetter(0), '-');
  }

  void recordRunFoldsEverythingIn() {
    LevelDef l;
    l.id = "1-1";
    l.parScore = 1000;
    RunResult r;
    r.cleared = true;
    r.flawless = true;
    r.objectiveMet = true;
    r.score = 950;
    r.bestCombo = 30;
    r.catches = 22;
    r.misses = 1;
    r.coins = 80;
    r.xp = 40;
    r.timeSec = 61.0;
    Profile p;
    const RunSummary s = recordRun(p, l, r);
    QVERIFY(s.newBest && s.firstClear && s.newRank);
    QCOMPARE(s.starsNow, 3);
    QCOMPARE(s.rankNow, 4);
    QCOMPARE(p.coins, 80);
    QCOMPARE(p.xp, 40);
    QCOMPARE(p.totalRuns, int64_t(1));
    QCOMPARE(p.bestComboEver, 30);
    QCOMPARE(totalStars(p), 3);

    // A worse second run never lowers anything.
    RunResult worse;
    worse.cleared = false;
    worse.score = 100;
    const RunSummary s2 = recordRun(p, l, worse);
    QVERIFY(!s2.newBest && !s2.firstClear && !s2.newRank);
    QCOMPARE(p.levels.value("1-1").bestScore, int64_t(950));
    QCOMPARE(p.levels.value("1-1").stars(), 3);
    QCOMPARE(p.levels.value("1-1").plays, 2);
    QCOMPARE(p.totalRuns, int64_t(2));
  }
};

QTEST_GUILESS_MAIN(TestSave)
#include "test_save.moc"
