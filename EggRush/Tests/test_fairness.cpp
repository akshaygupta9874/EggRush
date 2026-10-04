// The promise of the whole game: a player on base stats can catch every must-catch egg.
// The oracle bot (instant perception, perfect aim, same movement model as the player)
// plays every real level on every preset over many seeds and must never miss.
#include <QtTest>
#include <cmath>

#include "TestUtil.hpp"
#include "er/Kinematics.hpp"

using namespace er;

class TestFairness : public QObject {
  Q_OBJECT
 private slots:
  void oracleNeverMissesOnAnyLevelOrPreset() {
    const ContentDb& db = ertest::realContent();
    int runs = 0;
    for (const LevelDef& level : db.levels) {
      for (Preset preset : {Preset::Chill, Preset::Normal, Preset::Hardcore}) {
        for (uint64_t seed = 1; seed <= 12; ++seed) {
          const BotRunStats st = simulateRun(db.game, level, preset, seed * 31 + 7, kOracleBot);
          ++runs;
          QVERIFY2(st.result.cleared && st.result.misses == 0,
                   qPrintable(QString("level %1 preset %2 seed %3: cleared=%4 misses=%5")
                                  .arg(QString::fromStdString(level.id))
                                  .arg(static_cast<int>(preset))
                                  .arg(seed * 31 + 7)
                                  .arg(st.result.cleared)
                                  .arg(st.result.misses)));
        }
      }
    }
    QVERIFY(runs >= 6 * 3 * 12);
  }

  // Every consecutive pair of must-catch eggs obeys the closed-form limit, per preset.
  void consecutiveEggsRespectTheKinematicLimit() {
    const ContentDb& db = ertest::realContent();
    for (const LevelDef& level : db.levels) {
      for (Preset preset : {Preset::Chill, Preset::Normal, Preset::Hardcore}) {
        World w;
        EventLog log;
        w.setSink(&log);
        w.reset(db.game, level, preset, 4242);
        Bot bot(kAverageBot, 3);
        while (w.status() == RunStatus::Running && w.tick() < 300u * 120u) w.step(bot.decide(w));

        struct Land {
          double t;
          double x;
        };
        std::vector<Land> lands;
        for (const Event& e : log.events) {
          if (e.type != EventType::EggReleased) continue;
          const EggDef& ed = db.game.eggs[static_cast<size_t>(e.eggType)];
          if (ed.bonus) continue;
          const double rel = static_cast<double>(e.tick) * static_cast<double>(cfg::kSimStepSec);
          lands.push_back({rel + kin::fallTime(ed.gravityScale, ed.launchVelPxPerSec), static_cast<double>(e.x)});
        }
        std::sort(lands.begin(), lands.end(), [](const Land& a, const Land& b) { return a.t < b.t; });
        const double buf = presetDef(preset).reactionBufferSec;
        for (size_t i = 1; i < lands.size(); ++i) {
          const double gap = lands[i].t - lands[i - 1].t;
          const double dx = std::fabs(lands[i].x - lands[i - 1].x);
          QVERIFY2(dx <= kin::maxDx(gap, buf) + 1.0,
                   qPrintable(QString("level %1 preset %2: eggs %3->%4 gap %5 s dx %6 px > limit %7")
                                  .arg(QString::fromStdString(level.id))
                                  .arg(static_cast<int>(preset))
                                  .arg(i - 1)
                                  .arg(i)
                                  .arg(gap)
                                  .arg(dx)
                                  .arg(kin::maxDx(gap, buf))));
        }
      }
    }
  }

  void everyTelegraphIsLongEnoughToRead() {
    const ContentDb& db = ertest::realContent();
    for (const BirdDef& b : db.game.birds) QVERIFY(b.telegraphSec >= cfg::kMinTelegraphSec);
    // Fast eggs fall in 0.80 s; the telegraph plus the fall must still give > 1 s of warning.
    const EggDef& fast = db.game.eggs[static_cast<size_t>(EggType::Fast)];
    const double fall = kin::fallTime(fast.gravityScale, fast.launchVelPxPerSec);
    for (const BirdDef& b : db.game.birds) QVERIFY(fall + static_cast<double>(b.telegraphSec) > 1.0);
  }

  void impossibleSpawnsAreRepairedNotDropped() {
    // A deliberately brutal level: two fast-dropping hens over the full width. The spawn
    // gate must delay or convert eggs, and the oracle must still never miss.
    GameData data = defaultGameData();
    LevelDef l = ertest::basicLevel(45.f, 0.6f);
    l.birds.push_back(l.birds[0]);
    l.birds[1].startOffsetSec = 0.3f;
    World w;
    EventLog log;
    w.setSink(&log);
    w.reset(data, l, Preset::Hardcore, 9);
    Bot bot(kOracleBot, 2);
    while (w.status() == RunStatus::Running && w.tick() < 120u * 120u) w.step(bot.decide(w));
    QVERIFY(log.count(EventType::SpawnRepaired) > 0);
    QCOMPARE(w.result().misses, 0);
  }
};

QTEST_APPLESS_MAIN(TestFairness)
#include "test_fairness.moc"
