// Fairness validator v2 (Section 64): V1..V8 plus agreement with the closed-form
// kinematics. If these pass, "fair" means one precise thing everywhere in the game.
#include <QtTest>

#include "er/FairnessValidator.hpp"
#include "er/Kinematics.hpp"
#include "er/Rng.hpp"

using namespace er;

namespace {
FairEvent must(double t, double x, int id = 0) { return {t, x, FairKind::Must, id}; }
FairEvent hazard(double t, double x, int id = 0) { return {t, x, FairKind::Hazard, id}; }
FairEvent bonus(double t, double x, int id = 0) { return {t, x, FairKind::Bonus, id}; }
}  // namespace

class TestValidator : public QObject {
  Q_OBJECT
 private slots:
  // V1: any single must-catch egg is fine when the basket may start anywhere.
  void v1_singleEggAnywhere() {
    FairnessValidator v(0.18);
    for (double x : {60.0, 500.0, 960.0, 1860.0}) QVERIFY(v.validate({must(3.0, x)}).ok);
  }

  // V2: two eggs at exactly maxDx pass; one pixel more fails. Every preset, many gaps,
  // both directions across the field.
  void v2_pairBoundaryMatchesClosedForm() {
    for (double buf : {0.12, 0.18, 0.25}) {
      FairnessValidator v(buf);
      for (double gap : {0.3, 0.4, 0.6, 0.9, 1.1}) {
        const double dx = kin::maxDx(gap, buf);
        for (int dir : {+1, -1}) {
          const double x0 = dir > 0 ? 300.0 : 1600.0;
          const double xEdge = x0 + dir * dx;
          if (xEdge - 85.0 < 110.0 - 85.0 || xEdge + 85.0 > 1810.0 + 85.0) continue;  // off the field
          const double xFail = x0 + dir * (dx + 1.0);
          if (xFail - 85.0 < 110.0 - 85.0 || xFail + 85.0 > 1810.0 + 85.0) continue;
          QVERIFY2(v.validate({must(2.0, x0), must(2.0 + gap, x0 + dir * (dx - 1e-6))}).ok,
                   qPrintable(QString("buf %1 gap %2 dir %3 should pass").arg(buf).arg(gap).arg(dir)));
          QVERIFY2(!v.validate({must(2.0, x0), must(2.0 + gap, xFail)}).ok,
                   qPrintable(QString("buf %1 gap %2 dir %3 should fail").arg(buf).arg(gap).arg(dir)));
        }
      }
    }
  }

  // V3: a hazard may not sit where the basket is forced to be.
  void v3_hazardInsideForcedPositionFails() {
    FairnessValidator v(0.18);
    // must at x=900 forces the centre into [815, 985]; a hazard zone of 105 px each side
    // centred on 900 covers all of it. 0.2 s later (inside the 0.18 s buffer) the basket
    // has not been able to move at all.
    QVERIFY(!v.validate({must(2.0, 900.0), hazard(2.2, 900.0)}).ok);
    // With 0.4 s the basket can step ~200 px aside and dodge.
    QVERIFY(v.validate({must(2.0, 900.0), hazard(2.4, 900.0)}).ok);
  }

  // V4: simultaneous eggs must be <= 170 px apart (the slack), not more.
  void v4_simultaneousEggs() {
    FairnessValidator v(0.18);
    QVERIFY(v.validate({must(2.0, 800.0, 1), must(2.0, 969.0, 2)}).ok);
    QVERIFY(!v.validate({must(2.0, 800.0, 1), must(2.0, 971.0, 2)}).ok);
  }

  // V5: bonus eggs never constrain anything.
  void v5_bonusIgnored() {
    FairnessValidator v(0.18);
    QVERIFY(v.validate({must(2.0, 200.0), bonus(2.05, 1800.0), must(2.1, 210.0)}).ok);
  }

  // V6: sortByTime makes the result independent of insertion order.
  void v6_orderIndependentAfterSort() {
    Rng rng(99);
    FairnessValidator v(0.18);
    for (int trial = 0; trial < 300; ++trial) {
      std::vector<FairEvent> a;
      const int n = 2 + static_cast<int>(rng.below(5));
      for (int i = 0; i < n; ++i) {
        a.push_back(must(1.0 + rng.range(0.f, 3.f), 100.0 + rng.range(0.f, 1700.f), i));
      }
      std::vector<FairEvent> b(a.rbegin(), a.rend());
      FairnessValidator::sortByTime(a);
      FairnessValidator::sortByTime(b);
      QCOMPARE(v.validate(a).ok, v.validate(b).ok);
    }
  }

  // V7: a known start point is stricter than "anywhere", never looser.
  void v7_startPointIsStricter() {
    FairnessValidator v(0.18);
    const std::vector<FairEvent> far = {must(0.9, 1700.0)};
    QVERIFY(v.validate(far).ok);                           // anywhere: fine
    QVERIFY(!v.validate(far, true, 200.0, 0.0).ok);        // from x=200 in 0.9 s: too far
    QVERIFY(v.validate({must(3.0, 1700.0)}, true, 200.0, 0.0).ok);  // 3 s is plenty
  }

  // V8: a smaller reaction buffer is never stricter.
  void v8_bufferMonotonic() {
    Rng rng(5);
    FairnessValidator chill(0.25), normal(0.18), hard(0.12);
    for (int trial = 0; trial < 400; ++trial) {
      std::vector<FairEvent> ev;
      const int n = 2 + static_cast<int>(rng.below(5));
      for (int i = 0; i < n; ++i) {
        ev.push_back(must(1.0 + rng.range(0.f, 4.f), 100.0 + rng.range(0.f, 1700.f), i));
      }
      FairnessValidator::sortByTime(ev);
      if (chill.validate(ev).ok) QVERIFY(normal.validate(ev).ok);
      if (normal.validate(ev).ok) QVERIFY(hard.validate(ev).ok);
    }
  }

  // V9: a late-telegraphed egg is judged from the WORST point of the previous catch zone.
  void v9_lateInformationIsWorstCase() {
    FairnessValidator v(0.18);
    FairEvent n = must(3.2, 1600.0, 2);
    QVERIFY(v.validate({must(2.0, 300.0, 1), n}).ok);  // omniscient: fine
    n.known = 1.0;                                     // known before the first landed: same
    QVERIFY(v.validate({must(2.0, 300.0, 1), n}).ok);
    n.known = 2.1;                                     // telegraph only after the first landing
    QVERIFY(!v.validate({must(2.0, 300.0, 1), n}).ok);
    n.known = 2.1;
    n.x = 1100.0;                                      // a nearer egg is still fine
    QVERIFY(v.validate({must(2.0, 300.0, 1), n}).ok);
  }

  void failureReportsTheOffendingEvent() {
    FairnessValidator v(0.18);
    const FairResult r = v.validate({must(2.0, 200.0, 11), must(2.2, 1500.0, 22), must(5.0, 900.0, 33)});
    QVERIFY(!r.ok);
    QCOMPARE(r.failIndex, 1);
    QCOMPARE(r.failId, 22);
  }

  void intervalSetOperations() {
    IntervalSet s;
    s.setSingle(100, 200);
    s.expand(50, 0, 1000);
    QCOMPARE(s.spans().size(), size_t(1));
    QCOMPARE(s.spans()[0].lo, 50.0);
    QCOMPARE(s.spans()[0].hi, 250.0);
    s.subtract(100, 150);
    QCOMPARE(s.spans().size(), size_t(2));
    s.intersect(120, 400);
    QCOMPARE(s.spans().size(), size_t(1));
    QCOMPARE(s.spans()[0].lo, 150.0);
    s.subtract(0, 1000);
    QVERIFY(s.empty());
  }
};

QTEST_APPLESS_MAIN(TestValidator)
#include "test_validator.moc"
