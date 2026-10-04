// Unified scoring (Section 65): score = floor(base * quality * depth * combo * powerup) + clutch.
#include <QtTest>

#include "er/Scoring.hpp"

using namespace er;
using namespace er::scoring;

class TestScoring : public QObject {
  Q_OBJECT
 private slots:
  void comboMultiplierTable() {
    QCOMPARE(comboMultiplier(0), 1.0f);
    QCOMPARE(comboMultiplier(4), 1.0f);
    QCOMPARE(comboMultiplier(5), 1.25f);
    QCOMPARE(comboMultiplier(10), 1.5f);
    QCOMPARE(comboMultiplier(49), 3.25f);
    QCOMPARE(comboMultiplier(50), 3.5f);
    QCOMPARE(comboMultiplier(55), 3.625f);
    QCOMPARE(comboMultiplier(100), 4.75f);
    QCOMPARE(comboMultiplier(229), 7.875f);
    QCOMPARE(comboMultiplier(230), 8.0f);
    QCOMPARE(comboMultiplier(100000), 8.0f);
    QCOMPARE(comboMultiplier(-5), 1.0f);
  }

  void comboMultiplierIsMonotonic() {
    float prev = 0.f;
    for (int c = 0; c < 500; ++c) {
      const float m = comboMultiplier(c);
      QVERIFY(m >= prev);
      QVERIFY(m <= 8.0f);
      prev = m;
    }
  }

  void qualityBands() {
    QCOMPARE(classifyQuality(0.0f), CatchQuality::Perfect);
    QCOMPARE(classifyQuality(0.22f), CatchQuality::Perfect);
    QCOMPARE(classifyQuality(0.2201f), CatchQuality::Good);
    QCOMPARE(classifyQuality(0.55f), CatchQuality::Good);
    QCOMPARE(classifyQuality(0.5501f), CatchQuality::Close);
    QCOMPARE(classifyQuality(1.2f), CatchQuality::Close);
    QCOMPARE(qualityMult(CatchQuality::Close), 0.6f);
    QCOMPARE(qualityMult(CatchQuality::Good), 1.0f);
    QCOMPARE(qualityMult(CatchQuality::Perfect), 1.5f);
  }

  void depthBands() {
    QCOMPARE(depthFromTau(1.0).mult, 1.0f);
    QCOMPARE(depthFromTau(0.55).mult, 1.0f);
    QCOMPARE(depthFromTau(0.54).mult, 1.2f);
    QCOMPARE(depthFromTau(0.25).mult, 1.2f);
    QCOMPARE(depthFromTau(0.24).mult, 1.5f);
    QCOMPARE(depthFromTau(0.10).mult, 1.5f);
    QCOMPARE(depthFromTau(0.09).mult, 2.0f);
    QVERIFY(depthFromTau(0.09).clutch);
    QVERIFY(depthFromTau(0.0).clutch);
    QVERIFY(!depthFromTau(0.10).clutch);
  }

  void basicCatchScores() {
    // 10 * Good(1.0) * depth 1.0 * combo 1 = 10
    QCOMPARE(scoreCatch(10, CatchQuality::Good, 1.0, 1, 1.f, true).score, int64_t(10));
    // Perfect (1.5) * depth 1.2 = 1.8, combo 5 -> x1.25: 10 * 1.8 * 1.25 = 22.5 -> 22
    QCOMPARE(scoreCatch(10, CatchQuality::Perfect, 0.4, 5, 1.f, true).score, int64_t(22));
    // Close 0.6 -> 6
    QCOMPARE(scoreCatch(10, CatchQuality::Close, 1.0, 1, 1.f, true).score, int64_t(6));
  }

  void qualityTimesDepthIsCapped() {
    // Perfect 1.5 * Clutch 2.0 = 3.0, exactly the cap, then +50.
    const CatchScore s = scoreCatch(10, CatchQuality::Perfect, 0.05, 1, 1.f, true);
    QCOMPARE(s.score, int64_t(10 * 3 + 50));
    QVERIFY(s.clutch);
  }

  void clutchBonusRespectsTheBlockCap() {
    const CatchScore with = scoreCatch(10, CatchQuality::Good, 0.05, 1, 1.f, true);
    const CatchScore without = scoreCatch(10, CatchQuality::Good, 0.05, 1, 1.f, false);
    QCOMPARE(with.score - without.score, int64_t(50));
    QVERIFY(without.clutch);  // the catch is still a clutch catch, it just earns no bonus
  }

  void powerupMultiplies() {
    QCOMPARE(scoreCatch(100, CatchQuality::Good, 1.0, 1, 2.f, true).score, int64_t(200));
  }

  void scoresAreNeverNegativeOrShrinkWithCombo() {
    int64_t prev = 0;
    for (int combo = 1; combo < 300; ++combo) {
      const int64_t s = scoreCatch(10, CatchQuality::Good, 1.0, combo, 1.f, true).score;
      QVERIFY(s >= prev);
      prev = s;
    }
  }
};

QTEST_APPLESS_MAIN(TestScoring)
#include "test_scoring.moc"
