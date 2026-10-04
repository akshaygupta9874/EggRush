// World: collision, catching, hearts, combo, waves, determinism, invariants.
#include <QtTest>
#include <cmath>

#include "TestUtil.hpp"

using namespace er;

namespace {

// A level with a single hen confined to the middle lanes, so x never clamps.
LevelDef probeLevel(EggType type = EggType::Normal) {
  LevelDef l = ertest::basicLevel(60.f, 100.f);
  l.birds[0].laneMin = 6;
  l.birds[0].laneMax = 9;
  l.birds[0].intervalJitter = 0.f;
  l.eggMix = {0.f, 0.f, 0.f, 0.f};
  l.eggMix[static_cast<size_t>(type)] = 1.f;
  return l;
}

struct Probe {
  World w;
  EventLog log;
  GameData data = defaultGameData();
  Probe(const LevelDef& l, Preset p = Preset::Normal, uint64_t seed = 1) {
    w.setSink(&log);
    w.reset(data, l, p, seed);
  }
  // Steps until the first egg has been planned, returns its x.
  float firstPlanX() {
    InputSnapshot idle;
    for (int i = 0; i < 4 && !w.birds()[0].plan.active; ++i) w.step(idle);
    return w.birds()[0].plan.x;
  }
  // Holds the mouse target at `x` until the first egg is caught or missed.
  void holdUntilResolved(float x, double maxSec = 6.0) {
    InputSnapshot in;
    in.hasMouse = true;
    in.mouseX = x;
    const uint32_t cap = w.tick() + static_cast<uint32_t>(maxSec / cfg::kSimStepSec);
    while (w.status() == RunStatus::Running && w.tick() < cap &&
           log.count(EventType::CatchMade) + log.count(EventType::EggMissed) == 0) {
      w.step(in);
    }
  }
  const Event* first(EventType t) const {
    for (const Event& e : log.events) {
      if (e.type == t) return &e;
    }
    return nullptr;
  }
};

}  // namespace

class TestWorld : public QObject {
  Q_OBJECT
 private slots:
  // ---- catching & collision ------------------------------------------------
  void perfectCenterCatch() {
    Probe p(probeLevel());
    const float x = p.firstPlanX();
    p.holdUntilResolved(x);
    const Event* c = p.first(EventType::CatchMade);
    QVERIFY(c != nullptr);
    QCOMPARE(c->quality, CatchQuality::Perfect);
    QCOMPARE(c->scoreDelta, int64_t(15));  // 10 * 1.5 * depth 1.0 * combo 1.0
    QCOMPARE(p.w.combo(), 1);
    QCOMPARE(p.w.halfHearts(), 6);
  }

  void qualityFollowsOffset() {
    struct Row { float offset; CatchQuality q; int64_t score; };
    const Row rows[] = {{40.f, CatchQuality::Good, 10}, {60.f, CatchQuality::Close, 6}, {90.f, CatchQuality::Close, 6}};
    for (const Row& r : rows) {
      Probe p(probeLevel());
      const float x = p.firstPlanX();
      p.holdUntilResolved(x + r.offset);
      const Event* c = p.first(EventType::CatchMade);
      QVERIFY2(c != nullptr, qPrintable(QString("offset %1").arg(static_cast<double>(r.offset))));
      QCOMPARE(c->quality, r.q);
      QCOMPARE(c->scoreDelta, r.score);
    }
  }

  void forgivenessBandStillCatches() {
    Probe p(probeLevel());  // forgivenessPx defaults to 8: catch band is 95 + 8
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 99.f);
    QVERIFY(p.first(EventType::CatchMade) != nullptr);
  }

  void rimBounceThenMiss() {
    Probe p(probeLevel());
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 107.f);  // between the catch band (103) and the basket edge (110)
    QVERIFY(p.first(EventType::EggRimBounced) != nullptr);
    QVERIFY(p.first(EventType::CatchMade) == nullptr);
    QVERIFY(p.first(EventType::EggMissed) != nullptr);
  }

  void rimBounceCanBeRescued() {
    Probe p(probeLevel());
    const float x = p.firstPlanX();
    InputSnapshot in;
    in.hasMouse = true;
    in.mouseX = x + 107.f;
    // Hold until the bounce, then chase the egg with the basket.
    uint32_t guard = 0;
    while (p.log.count(EventType::EggRimBounced) == 0 && guard++ < 1000) p.w.step(in);
    QVERIFY(p.log.count(EventType::EggRimBounced) == 1);
    guard = 0;
    while (p.log.count(EventType::CatchMade) + p.log.count(EventType::EggMissed) == 0 && guard++ < 1000) {
      for (const Egg& e : p.w.eggs()) {
        if (e.alive) in.mouseX = e.x;
      }
      p.w.step(in);
    }
    const Event* c = p.first(EventType::CatchMade);
    QVERIFY2(c != nullptr, "a bounced egg should be catchable on its way down");
    QVERIFY((c->flags & kFlagRescued) != 0);
    QCOMPARE(c->quality, CatchQuality::Close);  // rescues never earn quality or depth bonuses
  }

  void missOutsideTheBasket() {
    Probe p(probeLevel());
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 300.f);
    const Event* m = p.first(EventType::EggMissed);
    QVERIFY(m != nullptr);
    QCOMPARE(m->value, 1);
    QCOMPARE(p.w.halfHearts(), 5);
    QCOMPARE(p.w.result().misses, 1);
    QCOMPARE(p.w.combo(), 0);
  }

  void heavyEggCostsTwoHalfHearts() {
    Probe p(probeLevel(EggType::Heavy));
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 300.f);
    QCOMPARE(p.w.halfHearts(), 4);
  }

  void goldenEggIsAPureBonus() {
    Probe p(probeLevel(EggType::Golden));
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 300.f);  // missing it costs nothing
    QCOMPARE(p.w.halfHearts(), 6);
    QCOMPARE(p.w.result().misses, 0);
    Probe q(probeLevel(EggType::Golden));
    const float x2 = q.firstPlanX();
    q.holdUntilResolved(x2);
    const Event* c = q.first(EventType::CatchMade);
    QVERIFY(c != nullptr);
    QVERIFY((c->flags & kFlagBonusEgg) != 0);
    QVERIFY(c->scoreDelta >= 100);
  }

  void fastEggFallsFasterThanNormal() {
    Probe n(probeLevel(EggType::Normal)), f(probeLevel(EggType::Fast));
    n.firstPlanX();
    f.firstPlanX();
    const double fallN = n.w.birds()[0].plan.landT - n.w.birds()[0].plan.releaseT;
    const double fallF = f.w.birds()[0].plan.landT - f.w.birds()[0].plan.releaseT;
    QVERIFY(fallF < fallN - 0.5);
    QVERIFY(std::fabs(fallN - 1.38) < 0.01);
    QVERIFY(std::fabs(fallF - 0.80) < 0.01);
  }

  void lateCoverEarnsAClutch() {
    Probe p(probeLevel());
    const float x = p.firstPlanX();
    InputSnapshot in;
    in.hasMouse = true;
    in.mouseX = x + 105.f;  // just outside the catch band, still under the basket
    uint32_t guard = 0;
    double landT = 0;
    while (p.log.count(EventType::EggReleased) == 0 && guard++ < 2000) p.w.step(in);
    for (const Egg& e : p.w.eggs()) {
      if (e.alive) landT = e.landT;
    }
    while (p.w.timeSec() < landT - 0.06 && guard++ < 4000) p.w.step(in);
    in.mouseX = x;  // slide inward at the last moment
    while (p.log.count(EventType::CatchMade) + p.log.count(EventType::EggMissed) == 0 && guard++ < 6000) {
      p.w.step(in);
    }
    const Event* c = p.first(EventType::CatchMade);
    QVERIFY(c != nullptr);
    QVERIFY((c->flags & kFlagClutch) != 0);
    QVERIFY(c->scoreDelta >= cfg::kClutchBonusScore);
    QCOMPARE(p.w.result().clutch, 1);
  }

  // ---- hearts, combo, tutorial ------------------------------------------------
  void firstMissIsForgivenInTheTutorialLevel() {
    LevelDef l = probeLevel();
    l.forgiveFirstMiss = true;
    Probe p(l);
    const float x = p.firstPlanX();
    p.holdUntilResolved(x + 300.f);
    QCOMPARE(p.w.halfHearts(), 6);
    QCOMPARE(p.w.result().halfHeartsLost, 0);
    QCOMPARE(p.w.result().misses, 1);
    const Event* lost = p.first(EventType::HeartLost);
    const Event* gained = p.first(EventType::HeartGained);
    QVERIFY(lost != nullptr && gained != nullptr);
    QVERIFY((lost->flags & 1u) != 0);
  }

  void heartsNeverLeaveTheirRange() {
    const ContentDb& db = ertest::realContent();
    for (Preset preset : {Preset::Chill, Preset::Normal, Preset::Hardcore}) {
      World w;
      w.reset(db.game, db.levels[3], preset, 77);
      Bot bot(kNoviceBot, 5);
      while (w.status() == RunStatus::Running && w.tick() < 120u * 120u) {
        w.step(bot.decide(w));
        QVERIFY(w.halfHearts() >= 0 && w.halfHearts() <= w.maxHalfHearts());
        QVERIFY(w.combo() >= 0);
        QVERIFY(w.aliveEggCount() <= kMaxEggs);
        QVERIFY(w.basket().x >= cfg::kBasketMinXPx && w.basket().x <= cfg::kBasketMaxXPx);
      }
    }
  }

  void idlePlayerEventuallyLoses() {
    const ContentDb& db = ertest::realContent();
    World w;
    EventLog log;
    w.setSink(&log);
    w.reset(db.game, db.levels[1], Preset::Normal, 3);
    InputSnapshot idle;
    while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(idle);
    QCOMPARE(w.status(), RunStatus::GameOver);
    QVERIFY(w.result().misses >= 3);
    QCOMPARE(log.count(EventType::GameOver), 1);
    QCOMPARE(w.halfHearts(), 0);
  }

  void finishedRunIgnoresFurtherSteps() {
    const ContentDb& db = ertest::realContent();
    World w;
    w.reset(db.game, db.levels[1], Preset::Hardcore, 3);
    InputSnapshot idle;
    while (w.status() == RunStatus::Running) w.step(idle);
    const uint64_t h = w.stateHash();
    const uint32_t t = w.tick();
    for (int i = 0; i < 50; ++i) w.step(idle);
    QCOMPARE(w.stateHash(), h);
    QCOMPARE(w.tick(), t);
  }

  // ---- waves, objectives, economy -----------------------------------------------
  void flawlessWavesHealHalfAHeart() {
    LevelDef l = ertest::basicLevel(70.f, 2.0f);
    World w;
    EventLog log;
    w.setSink(&log);
    const GameData data = defaultGameData();
    w.reset(data, l, Preset::Normal, 11);
    Bot bot(kOracleBot, 1);
    while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(bot.decide(w));
    QCOMPARE(w.status(), RunStatus::Cleared);
    QCOMPARE(w.result().misses, 0);
    QVERIFY(log.count(EventType::WaveCleared) >= 2);
    QCOMPARE(w.result().flawlessWaves, w.result().wavesCleared);
    QVERIFY(log.count(EventType::HeartGained) >= 2);
    QCOMPARE(w.halfHearts(), 6 + log.count(EventType::HeartGained));
    QVERIFY(w.halfHearts() <= w.maxHalfHearts());
  }

  void hardcoreHasNoWaveHealing() {
    LevelDef l = ertest::basicLevel(70.f, 2.0f);
    World w;
    EventLog log;
    w.setSink(&log);
    w.reset(defaultGameData(), l, Preset::Hardcore, 11);
    Bot bot(kOracleBot, 1);
    while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(bot.decide(w));
    QCOMPARE(log.count(EventType::HeartGained), 0);
    QCOMPARE(w.halfHearts(), 4);
  }

  void coinsAndXpFollowSection65() {
    LevelDef l = ertest::basicLevel(40.f, 2.0f);
    l.coinMultiplier = 1.5f;
    World w;
    EventLog log;
    w.setSink(&log);
    const GameData data = defaultGameData();
    w.reset(data, l, Preset::Normal, 21);
    Bot bot(kExpertBot, 4);
    while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(bot.decide(w));
    QCOMPARE(w.status(), RunStatus::Cleared);
    double raw = 0;
    int64_t scoreSum = 0;
    for (const Event& e : log.events) {
      if (e.type == EventType::CatchMade) {
        raw += data.eggs[static_cast<size_t>(e.eggType)].coins;
        scoreSum += e.scoreDelta;
      }
      if (e.type == EventType::WaveCleared) raw += 10;
    }
    QCOMPARE(scoreSum, w.result().score);
    raw += static_cast<double>(w.result().score / 100);
    QCOMPARE(w.result().coins, static_cast<int>(std::floor(raw * 1.5 + 0.5)));
    QCOMPARE(w.result().xp, static_cast<int>(w.result().score / 40) + 8 * w.result().wavesCleared);
  }

  void objectiveIsTrackedAndAnnouncedOnce() {
    LevelDef l = ertest::basicLevel(40.f, 2.0f);
    l.objective = {ObjectiveType::CatchEggs, 5, 0};
    World w;
    EventLog log;
    w.setSink(&log);
    w.reset(defaultGameData(), l, Preset::Normal, 2);
    Bot bot(kOracleBot, 1);
    while (w.status() == RunStatus::Running && w.tick() < 100u * 120u) w.step(bot.decide(w));
    QVERIFY(w.objectiveMet());
    QCOMPARE(log.count(EventType::ObjectiveMet), 1);
    QVERIFY(w.result().objectiveMet);
    QVERIFY(w.result().objectiveProgress >= 5);
  }

  void comboBreaksWhenAnEggIsMissed() {
    LevelDef l = probeLevel();
    l.birds[0].intervalSec = 3.0f;
    Probe p(l);
    const float x = p.firstPlanX();
    p.holdUntilResolved(x);
    QCOMPARE(p.w.combo(), 1);
    // Now stand far away for the second egg.
    InputSnapshot in;
    in.hasMouse = true;
    in.mouseX = 1700.f;
    for (int i = 0; i < 120 * 8 && p.log.count(EventType::EggMissed) == 0; ++i) p.w.step(in);
    QCOMPARE(p.w.combo(), 0);
    QCOMPARE(p.log.count(EventType::ComboBroken), 1);
  }

  // ---- determinism -----------------------------------------------------------------
  void sameSeedSameRun() {
    const ContentDb& db = ertest::realContent();
    const LevelDef& level = db.levels[3];  // two hens
    uint64_t hashes[2];
    int64_t scores[2];
    uint64_t logHash[2];
    for (int k = 0; k < 2; ++k) {
      World w;
      EventLog log;
      w.setSink(&log);
      w.reset(db.game, level, Preset::Normal, 1234);
      Bot bot(kAverageBot, 99);
      while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(bot.decide(w));
      hashes[k] = w.stateHash();
      scores[k] = w.score();
      logHash[k] = log.hash;
    }
    QCOMPARE(hashes[0], hashes[1]);
    QCOMPARE(scores[0], scores[1]);
    QCOMPARE(logHash[0], logHash[1]);
  }

  void differentSeedDifferentRun() {
    const ContentDb& db = ertest::realContent();
    uint64_t logHash[2];
    for (int k = 0; k < 2; ++k) {
      World w;
      EventLog log;
      w.setSink(&log);
      w.reset(db.game, db.levels[1], Preset::Normal, 100 + static_cast<uint64_t>(k));
      Bot bot(kAverageBot, 99);
      while (w.status() == RunStatus::Running && w.tick() < 200u * 120u) w.step(bot.decide(w));
      logHash[k] = log.hash;
    }
    QVERIFY(logHash[0] != logHash[1]);
  }

  void resetFullyRestoresInitialState() {
    const ContentDb& db = ertest::realContent();
    World w;
    w.reset(db.game, db.levels[0], Preset::Normal, 5);
    const uint64_t fresh = w.stateHash();
    Bot bot(kAverageBot, 1);
    for (int i = 0; i < 2000; ++i) w.step(bot.decide(w));
    w.reset(db.game, db.levels[0], Preset::Normal, 5);
    QCOMPARE(w.stateHash(), fresh);
    QCOMPARE(w.score(), int64_t(0));
    QCOMPARE(w.combo(), 0);
    QCOMPARE(w.aliveEggCount(), 0);
  }
};

QTEST_APPLESS_MAIN(TestWorld)
#include "test_world.moc"
