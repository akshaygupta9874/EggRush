#include "er/FairnessValidator.hpp"

#include <algorithm>

namespace er {

void IntervalSet::normalize() {
  if (spans_.size() < 2) return;
  std::sort(spans_.begin(), spans_.end(), [](const Span& a, const Span& b) { return a.lo < b.lo; });
  scratch_.clear();
  for (const Span& s : spans_) {
    if (!scratch_.empty() && s.lo <= scratch_.back().hi) {
      scratch_.back().hi = std::max(scratch_.back().hi, s.hi);
    } else {
      scratch_.push_back(s);
    }
  }
  spans_.swap(scratch_);
}

void IntervalSet::expand(double r, double clampLo, double clampHi) {
  for (Span& s : spans_) {
    s.lo = std::max(clampLo, s.lo - r);
    s.hi = std::min(clampHi, s.hi + r);
  }
  normalize();
}

void IntervalSet::intersect(double lo, double hi) {
  scratch_.clear();
  for (const Span& s : spans_) {
    const double a = std::max(s.lo, lo);
    const double b = std::min(s.hi, hi);
    if (b >= a) scratch_.push_back({a, b});
  }
  spans_.swap(scratch_);
}

void IntervalSet::subtract(double lo, double hi) {
  scratch_.clear();
  for (const Span& s : spans_) {
    if (s.hi < lo || s.lo > hi) {
      scratch_.push_back(s);
      continue;
    }
    if (s.lo < lo) scratch_.push_back({s.lo, lo});
    if (s.hi > hi) scratch_.push_back({hi, s.hi});
  }
  spans_.swap(scratch_);
  normalize();
}

void FairnessValidator::sortByTime(std::vector<FairEvent>& events) {
  std::stable_sort(events.begin(), events.end(),
                   [](const FairEvent& a, const FairEvent& b) { return a.t < b.t; });
}

FairResult FairnessValidator::validate(const std::vector<FairEvent>& events, bool hasStart,
                                       double startX, double startTimeSec) {
  const double half = cfg::kMouthWidthPx * 0.5;
  const double margin = cfg::kCatchMarginPx;
  if (hasStart) {
    set_.setSingle(startX, startX);
  } else {
    set_.setSingle(cfg::kBasketMinXPx, cfg::kBasketMaxXPx);
  }
  bool havePrev = hasStart;
  double tPrev = startTimeSec;

  FairResult result;
  for (size_t i = 0; i < events.size(); ++i) {
    const FairEvent& e = events[i];
    if (e.kind == FairKind::Bonus) continue;
    if (havePrev) {
      if (e.known > tPrev + 1e-9) {
        // The egg is unknown while the basket settles after the previous catch: from any
        // point of the previous catch zone it must still be able to get there.
        const double r = kin::reach(e.t - e.known - buffer_);
        set_.setSingle(std::max<double>(cfg::kBasketMinXPx, set_.hi() - r),
                       std::min<double>(cfg::kBasketMaxXPx, set_.lo() + r));
      } else {
        set_.expand(kin::reach(e.t - tPrev - buffer_), cfg::kBasketMinXPx, cfg::kBasketMaxXPx);
      }
      if (set_.empty()) {
        result.ok = false;
        result.failIndex = static_cast<int>(i);
        result.failId = e.id;
        return result;
      }
    }
    if (e.kind == FairKind::Must) {
      set_.intersect(e.x - (half - margin), e.x + (half - margin));
    } else {
      set_.subtract(e.x - (half + margin), e.x + (half + margin));
    }
    if (set_.empty()) {
      result.ok = false;
      result.failIndex = static_cast<int>(i);
      result.failId = e.id;
      return result;
    }
    havePrev = true;
    tPrev = e.t;
  }
  return result;
}

}  // namespace er
