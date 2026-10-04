// Numbers that several systems depend on must keep agreeing (Section 63 / 69).
#include <QtTest>

#include "TestUtil.hpp"
#include "er/Kinematics.hpp"
#include "er/Rng.hpp"

using namespace er;

class TestInvariants : public QObject {
  Q_OBJECT
 private slots:
  void geometry() {
    QCOMPARE(cfg::kFallHeightPx, 860.f);
    QCOMPARE(cfg::kBasketMinXPx, 110.f);
    QCOMPARE(cfg::kBasketMaxXPx, 1810.f);
    QCOMPARE(kin::kSlackPx, 170.0);
    QVERIFY(cfg::kMouthWidthPx < cfg::kBasketWidthPx);
    QVERIFY(cfg::kMouthYPx < cfg::kGroundYPx);
    QCOMPARE(cfg::kSlotOffsetPx + cfg::kSlotPitchPx * (cfg::kSlotCount - 1) + cfg::kSlotOffsetPx, cfg::kLogicalWidthPx);
  }

  void everyLaneIsCatchable() {
    // Even the outermost lanes need a basket centre inside the clamp range.
    for (int lane = 0; lane < cfg::kSlotCount; ++lane) {
      const double x = static_cast<double>(cfg::kSlotOffsetPx) + static_cast<double>(cfg::kSlotPitchPx) * lane;
      const double lo = std::max<double>(cfg::kBasketMinXPx, x - kin::kSlackPx / 2.0);
      const double hi = std::min<double>(cfg::kBasketMaxXPx, x + kin::kSlackPx / 2.0);
      QVERIFY2(lo <= hi, qPrintable(QString("lane %1 cannot be caught").arg(lane)));
    }
  }

  void fallTimesMatchThePrompt() {
    const GameData d = defaultGameData();
    auto fall = [&](EggType t) {
      const EggDef& e = d.eggs[static_cast<size_t>(t)];
      return kin::fallTime(e.gravityScale, e.launchVelPxPerSec);
    };
    QVERIFY(std::fabs(fall(EggType::Normal) - 1.38) < 0.01);
    QVERIFY(std::fabs(fall(EggType::Fast) - 0.80) < 0.01);
    QVERIFY(std::fabs(fall(EggType::Heavy) - 1.21) < 0.01);
    // A fast egg must still be telegraphed long enough to read (Section 63).
    QVERIFY(fall(EggType::Fast) + 0.45 > 1.0);
  }

  void reachIsContinuousAndInvertible() {
    const double ta = kin::kV / kin::kA;
    QVERIFY(std::fabs(kin::reach(ta - 1e-9) - kin::reach(ta + 1e-9)) < 1e-3);
    for (double t : {0.01, 0.05, 0.1, 0.1556, 0.3, 0.8, 1.5, 2.5}) {
      QVERIFY(std::fabs(kin::inverseReach(kin::reach(t)) - t) < 1e-9);
    }
    QCOMPARE(kin::reach(-1.0), 0.0);
  }

  void minGapAndMaxDxAreInverses() {
    for (double buf : {0.12, 0.18, 0.25}) {
      for (double gap : {0.4, 0.7, 1.0, 1.5, 2.2}) {
        const double dx = kin::maxDx(gap, buf);
        QVERIFY(std::fabs(kin::minGap(dx, buf) - gap) < 1e-6);
      }
    }
  }

  void chillFullWidthSwingMatchesSection68() {
    // "a swing across 1,800 px needs 1.49 s on Chill"
    const double t = kin::minGap(1800.0, presetDef(Preset::Chill).reactionBufferSec);
    QVERIFY2(std::fabs(t - 1.49) < 0.01, qPrintable(QString::number(t)));
  }

  void presetsMatchSection65() {
    QCOMPARE(presetDef(Preset::Chill).startHalfHearts, 10);
    QCOMPARE(presetDef(Preset::Normal).startHalfHearts, 6);
    QCOMPARE(presetDef(Preset::Hardcore).startHalfHearts, 4);
    QVERIFY(presetDef(Preset::Chill).reactionBufferSec > presetDef(Preset::Normal).reactionBufferSec);
    QVERIFY(presetDef(Preset::Normal).reactionBufferSec > presetDef(Preset::Hardcore).reactionBufferSec);
    QVERIFY(!presetDef(Preset::Hardcore).flawlessHeal);
  }

  void fixedStepIs120Hz() { QVERIFY(std::fabs(cfg::kSimStepSec * 120.f - 1.f) < 1e-6f); }

  void pcg32MatchesTheReferenceVector() {
    // pcg32_srandom(42, 54): first output of the reference implementation.
    Rng r(42, 54);
    QCOMPARE(r.next(), 0xa15c02b7u);
    QCOMPARE(r.next(), 0x7b47f409u);
    QCOMPARE(r.next(), 0xba1d3330u);
  }

  void rngIsDeterministicAndUnbiased() {
    Rng a(7), b(7), c(8);
    for (int i = 0; i < 100; ++i) QCOMPARE(a.next(), b.next());
    bool differs = false;
    for (int i = 0; i < 20; ++i) differs |= (a.next() != c.next());
    QVERIFY(differs);
    Rng r(123);
    int bins[6] = {};
    for (int i = 0; i < 60000; ++i) ++bins[r.below(6)];
    for (int n : bins) QVERIFY(std::abs(n - 10000) < 400);
    for (int i = 0; i < 1000; ++i) {
      const float u = r.uniform();
      QVERIFY(u >= 0.f && u < 1.f);
    }
  }

  void wilsonBoundIsSane() {
    QCOMPARE(wilsonLowerBound(0, 0), 0.0);
    QVERIFY(wilsonLowerBound(100, 100) > 0.96);
    QVERIFY(wilsonLowerBound(100, 100) < 1.0);
    QVERIFY(wilsonLowerBound(50, 100) < 0.5);
    QVERIFY(wilsonLowerBound(50, 100) > 0.39);
    QVERIFY(wilsonLowerBound(10, 10) < wilsonLowerBound(100, 100));
  }
};

QTEST_APPLESS_MAIN(TestInvariants)
#include "test_invariants.moc"
