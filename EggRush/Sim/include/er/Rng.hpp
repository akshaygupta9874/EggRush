#pragma once
#include <cmath>
#include <cstdint>

namespace er {

// PCG32 (O'Neill). All gameplay randomness comes from here so runs are
// deterministic and replayable (Section 42, determinism rules).
class Rng {
 public:
  explicit Rng(uint64_t seed = 1, uint64_t sequence = 54) { reseed(seed, sequence); }

  void reseed(uint64_t seed, uint64_t sequence = 54) {
    state_ = 0;
    inc_ = (sequence << 1u) | 1u;
    next();
    state_ += seed;
    next();
  }

  uint32_t next() {
    const uint64_t old = state_;
    state_ = old * 6364136223846793005ULL + inc_;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    const uint32_t rot = static_cast<uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
  }

  // Unbiased value in [0, bound).
  uint32_t below(uint32_t bound) {
    if (bound <= 1u) return 0u;
    const uint32_t threshold = (~bound + 1u) % bound;
    for (;;) {
      const uint32_t r = next();
      if (r >= threshold) return r % bound;
    }
  }

  // [0, 1) with 24 random bits.
  float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
  float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }

  // Standard normal (Box-Muller), used by the bots only.
  float gaussian() {
    float u1 = uniform();
    if (u1 < 1e-7f) u1 = 1e-7f;
    const float u2 = uniform();
    return std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530718f * u2);
  }

  uint64_t stateValue() const { return state_ ^ (inc_ << 1); }

 private:
  uint64_t state_ = 0;
  uint64_t inc_ = 0;
};

}  // namespace er
