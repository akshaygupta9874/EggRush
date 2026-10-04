#pragma once
// Fairness validator v2 (Section 64). Answers: "can a player on base stats catch
// every must-catch egg in this timeline and still dodge every hazard?"
#include <cstdint>
#include <vector>

#include "er/Config.hpp"
#include "er/Kinematics.hpp"

namespace er {

enum class FairKind : uint8_t { Must, Hazard, Bonus };

struct FairEvent {
  double t = 0.0;  // landing time: the egg crosses the mouth line
  double x = 0.0;  // landing x
  FairKind kind = FairKind::Must;
  int id = -1;
  // When the player can first know about this egg (the bird's telegraph starts). If it is
  // later than the previous event's landing, the basket may be anywhere in its previous
  // catch zone when the news arrives, so the validator uses the worst case for it.
  double known = -1e18;
};

struct FairResult {
  bool ok = true;
  int failIndex = -1;  // index into the sorted event list
  int failId = -1;
};

// Sorted, non-overlapping [lo, hi] intervals of basket-centre positions.
class IntervalSet {
 public:
  struct Span {
    double lo;
    double hi;
  };

  void setSingle(double lo, double hi) {
    spans_.clear();
    if (hi >= lo) spans_.push_back({lo, hi});
  }
  bool empty() const { return spans_.empty(); }
  double lo() const { return spans_.front().lo; }  // hull; only call when !empty()
  double hi() const { return spans_.back().hi; }
  const std::vector<Span>& spans() const { return spans_; }

  void expand(double r, double clampLo, double clampHi);
  void intersect(double lo, double hi);
  void subtract(double lo, double hi);

 private:
  void normalize();
  std::vector<Span> spans_;
  std::vector<Span> scratch_;
};

class FairnessValidator {
 public:
  explicit FairnessValidator(double reactionBufferSec = 0.18) : buffer_(reactionBufferSec) {
    work_.reserve(32);
  }

  double buffer() const { return buffer_; }
  void setBuffer(double b) { buffer_ = b; }

  // `events` must be sorted by t (use sortByTime). If startX is set the basket is a
  // known point at startTimeSec (level start); otherwise it may be anywhere.
  FairResult validate(const std::vector<FairEvent>& events, bool hasStart = false,
                      double startX = cfg::kBasketStartXPx, double startTimeSec = 0.0);

  static void sortByTime(std::vector<FairEvent>& events);

 private:
  double buffer_;
  IntervalSet set_;
  std::vector<FairEvent> work_;
};

}  // namespace er
