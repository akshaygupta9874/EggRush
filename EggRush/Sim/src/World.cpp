#include "er/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "er/Kinematics.hpp"
#include "er/Scoring.hpp"

namespace er {

namespace {

constexpr float kDt = cfg::kSimStepSec;

float laneX(int lane) { return cfg::kSlotOffsetPx + cfg::kSlotPitchPx * static_cast<float>(lane); }

uint64_t floatBits(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof(u));
  return u;
}

struct Fnv {
  uint64_t h = 1469598103934665603ULL;
  void mix(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      h ^= (v >> (8 * i)) & 0xffu;
      h *= 1099511628211ULL;
    }
  }
};

}  // namespace

World::World() : validator_(0.18) { fairScratch_.reserve(32); }

void World::reset(const GameData& data, const LevelDef& level, Preset preset, uint64_t seed) {
  data_ = data;
  level_ = level;
  preset_ = preset;
  rng_.reseed(seed);
  validator_.setBuffer(presetDef(preset).reactionBufferSec);
  tick_ = 0;
  seq_ = 0;
  nextEggId_ = 1;
  status_ = RunStatus::Running;
  basket_ = BasketState{};
  eggs_ = {};
  birds_.clear();
  eggsPlanned_ = 0;
  rawCoins_ = 0;
  halfHearts_ = presetDef(preset).startHalfHearts;
  maxHalfHearts_ = halfHearts_ + 4;  // hearts cap = start + 2 hearts (Section 65)
  combo_ = 0;
  graceT_ = 0.0;
  clutchBonusesInBlock_ = 0;
  clutchBlockIndex_ = 0;
  waveIndex_ = 0;
  waveHeartsLost_ = 0;
  waveResolved_ = 0;
  perfectStreak_ = 0;
  birdStreak_.fill(0);
  bestBirdStreak_.fill(0);
  objectiveMet_ = false;
  forgiveUsed_ = false;
  result_ = RunResult{};
  result_.levelId = level.id;
  result_.seed = seed;
  result_.preset = preset;

  const int n = std::min<int>(static_cast<int>(level.birds.size()), kMaxBirds);
  for (int i = 0; i < n; ++i) {
    Bird b;
    b.id = i;
    b.spawnIndex = i;
    const LevelBird& lb = level.birds[static_cast<size_t>(i)];
    b.x = b.prevX = b.lastEggX = laneX((lb.laneMin + lb.laneMax) / 2);
    birds_.push_back(b);
  }
  Event e;
  e.type = EventType::RunStarted;
  publish(e);
}

void World::publish(Event e) {
  e.tick = tick_;
  e.seq = seq_++;
  if (sink_ != nullptr) sink_->onEvent(e);
}

double World::telegraphSecOf(const Bird& b) const {
  return data_.birds[static_cast<size_t>(level_.birds[static_cast<size_t>(b.spawnIndex)].birdDef)].telegraphSec;
}

int World::aliveEggCount() const {
  int n = 0;
  for (const Egg& e : eggs_) n += e.alive ? 1 : 0;
  return n;
}

void World::step(const InputSnapshot& in) {
  if (status_ != RunStatus::Running) return;
  seq_ = 0;
  basket_.prevX = basket_.x;
  for (Egg& e : eggs_) {
    if (e.alive) {
      e.prevX = e.x;
      e.prevY = e.y;
    }
  }
  for (Bird& b : birds_) b.prevX = b.x;

  stepBasket(in);
  stepBirds();
  stepEggs();
  if (status_ == RunStatus::Running) stepCombo();
  if (status_ == RunStatus::Running) stepWaves();
  if (status_ == RunStatus::Running) checkEnd();
  ++tick_;
}

// ---------------------------------------------------------------------------
// Basket: target-velocity model with separate accel / decel / turn-around rates.
void World::stepBasket(const InputSnapshot& in) {
  float target = static_cast<float>(in.axis) * cfg::kBasketMaxSpeedPxPerSec;
  if (in.hasMouse) {
    const float want = std::clamp(in.mouseX, cfg::kBasketMinXPx, cfg::kBasketMaxXPx);
    const float dist = want - basket_.x;
    if (std::fabs(dist) < 1.5f) {
      target = 0.f;
    } else {
      float v = std::sqrt(2.f * cfg::kBasketDecelPxPerSec2 * 0.7f * std::fabs(dist));
      v = std::min(v, cfg::kBasketMaxSpeedPxPerSec);
      target = dist > 0.f ? v : -v;
    }
  }
  float rate = cfg::kBasketAccelPxPerSec2;
  if (target == 0.f) {
    rate = cfg::kBasketDecelPxPerSec2;
  } else if (basket_.vx != 0.f && ((basket_.vx > 0.f) != (target > 0.f))) {
    rate = cfg::kBasketAccelPxPerSec2 * cfg::kTurnAroundAccelMult;
  } else if (std::fabs(basket_.vx) > std::fabs(target)) {
    rate = cfg::kBasketDecelPxPerSec2;
  }
  const float maxDv = rate * kDt;
  basket_.vx += std::clamp(target - basket_.vx, -maxDv, maxDv);
  basket_.x += basket_.vx * kDt;  // semi-implicit: velocity first, then position
  if (basket_.x <= cfg::kBasketMinXPx) {
    basket_.x = cfg::kBasketMinXPx;
    if (basket_.vx < 0.f) basket_.vx = 0.f;
  } else if (basket_.x >= cfg::kBasketMaxXPx) {
    basket_.x = cfg::kBasketMaxXPx;
    if (basket_.vx > 0.f) basket_.vx = 0.f;
  }
}

// ---------------------------------------------------------------------------
// Birds: plan (fairness gated) -> move -> telegraph -> release.
EggType World::pickEggType() {
  for (const ScriptedEgg& s : level_.scripted) {
    if (s.index == eggsPlanned_) return s.type;
  }
  float total = 0.f;
  for (float w : level_.eggMix) total += w;
  if (total <= 0.f) return EggType::Normal;
  float r = rng_.uniform() * total;
  for (int i = 0; i < kEggTypeCount; ++i) {
    r -= level_.eggMix[static_cast<size_t>(i)];
    if (r < 0.f) return static_cast<EggType>(i);
  }
  return EggType::Normal;
}

void World::planBird(Bird& b) {
  const LevelBird& lb = level_.birds[static_cast<size_t>(b.spawnIndex)];
  const BirdDef& bd = data_.birds[static_cast<size_t>(lb.birdDef)];
  const double now = timeSec();

  double t0;
  if (b.dropCount == 0) {
    t0 = static_cast<double>(level_.firstDropSec + lb.startOffsetSec);
  } else {
    const float jitter = (rng_.uniform() * 2.f - 1.f) * lb.intervalJitter;
    t0 = b.lastReleaseT + static_cast<double>(lb.intervalSec * (1.f + jitter));
  }
  t0 = std::max(t0, now + static_cast<double>(bd.telegraphSec) + 0.02);
  if (t0 >= static_cast<double>(level_.durationSec)) {
    b.finished = true;
    return;
  }

  EggType type = pickEggType();
  const EggDef* ed = &data_.eggs[static_cast<size_t>(type)];
  double fall = kin::fallTime(ed->gravityScale, ed->launchVelPxPerSec);
  const int lanes = std::max(1, lb.laneMax - lb.laneMin + 1);

  // Can the bird get to x, and stay inside its hop cap, in time for releaseT?
  auto reachable = [&](float x, double releaseT) {
    if (std::fabs(x - b.lastEggX) > lb.maxHopPx) return false;
    const double travel = static_cast<double>(std::fabs(x - b.x) / bd.moveSpeedPxPerSec);
    return now + travel + static_cast<double>(bd.telegraphSec) <= releaseT;
  };
  auto fair = [&](float x, double releaseT) {
    fairScratch_.clear();
    for (const Egg& e : eggs_) {
      if (e.alive && (!e.bonus || e.softMust) && !e.crossed) {
        fairScratch_.push_back({e.landT, static_cast<double>(e.x), FairKind::Must, e.id, e.knownT});
      }
    }
    for (const Bird& o : birds_) {
      if (&o != &b && o.plan.active && !o.plan.converted &&
          (!data_.eggs[static_cast<size_t>(o.plan.type)].bonus || o.plan.softMust)) {
        fairScratch_.push_back({o.plan.landT, static_cast<double>(o.plan.x), FairKind::Must, -1, o.plan.knownT});
      }
    }
    const double landT = releaseT + fall;
    // Comfort rule: near-simultaneous eggs must sit close together.
    for (const FairEvent& o : fairScratch_) {
      if (std::fabs(o.t - landT) < cfg::kPairWindowSec &&
          std::fabs(o.x - static_cast<double>(x)) > static_cast<double>(cfg::kPairMaxDxPx)) {
        return false;
      }
    }
    fairScratch_.push_back({landT, static_cast<double>(x), FairKind::Must, -2, releaseT - static_cast<double>(bd.telegraphSec)});
    FairnessValidator::sortByTime(fairScratch_);
    return validator_.validate(fairScratch_).ok;
  };

  constexpr int kAttemptsPerDelay = 16;
  constexpr int kDelaySteps = 5;  // 0.05 s each, up to 0.25 s (Section 64)
  bool found = false;
  bool soft = false;
  float chosenX = b.x;
  double chosenT = t0;
  int delaySteps = 0;
  auto randomLane = [&]() {
    return laneX(lb.laneMin + static_cast<int>(rng_.below(static_cast<uint32_t>(lanes))));
  };
  if (ed->bonus) {
    // Bonus eggs are never required, but a perfect player should be ABLE to catch them
    // (the Objective stars depend on it): prefer a lane that passes the fairness check.
    for (int a = 0; a < kAttemptsPerDelay && !found; ++a) {
      const float x = randomLane();
      if (reachable(x, t0) && fair(x, t0)) {
        found = soft = true;
        chosenX = x;
      }
    }
    for (int a = 0; a < kAttemptsPerDelay && !found; ++a) {
      const float x = randomLane();
      if (reachable(x, t0)) {
        found = true;
        chosenX = x;
      }
    }
    if (!found) found = true;  // the bird is already in position; the egg is a pure bonus
  } else {
    for (int k = 0; k <= kDelaySteps && !found; ++k) {
      const double releaseT = t0 + 0.05 * k;
      for (int a = 0; a < kAttemptsPerDelay && !found; ++a) {
        const float x = randomLane();
        if (!reachable(x, releaseT)) continue;
        if (fair(x, releaseT)) {
          found = true;
          chosenX = x;
          chosenT = releaseT;
          delaySteps = k;
        }
      }
    }
  }

  Plan plan;
  plan.active = true;
  if (!found) {
    // Last resort (Section 47): never drop an impossible egg. Convert to a bonus egg.
    type = EggType::Golden;
    ed = &data_.eggs[static_cast<size_t>(type)];
    fall = kin::fallTime(ed->gravityScale, ed->launchVelPxPerSec);
    chosenX = b.x;
    chosenT = t0;
    for (int a = 0; a < kAttemptsPerDelay; ++a) {
      const float x = randomLane();
      if (reachable(x, t0)) {
        chosenX = x;
        break;
      }
    }
    plan.converted = true;
    Event ev;
    ev.type = EventType::SpawnRepaired;
    ev.birdId = b.id;
    ev.value = 3;
    ev.x = chosenX;
    publish(ev);
    ++result_.repairs;
  } else if (delaySteps > 0) {
    Event ev;
    ev.type = EventType::SpawnRepaired;
    ev.birdId = b.id;
    ev.value = 1;
    ev.tau = 0.05f * static_cast<float>(delaySteps);
    ev.x = chosenX;
    publish(ev);
    ++result_.repairs;
  }
  plan.type = type;
  plan.softMust = soft;
  plan.x = chosenX;
  plan.releaseT = chosenT;
  plan.landT = chosenT + fall;
  plan.knownT = chosenT - static_cast<double>(bd.telegraphSec);
  b.plan = plan;
  ++eggsPlanned_;
}

void World::stepBirds() {
  const double now = timeSec();
  for (Bird& b : birds_) {
    if (!b.plan.active && !b.finished) planBird(b);
    if (!b.plan.active) {
      b.phase = BirdPhase::Idle;
      continue;
    }
    const BirdDef& bd = data_.birds[static_cast<size_t>(level_.birds[static_cast<size_t>(b.spawnIndex)].birdDef)];
    if (b.phase != BirdPhase::Telegraphing) {
      const float dx = b.plan.x - b.x;
      const float stepPx = bd.moveSpeedPxPerSec * kDt;
      if (std::fabs(dx) <= stepPx) {
        b.x = b.plan.x;
        b.phase = BirdPhase::Idle;
      } else {
        b.x += dx > 0.f ? stepPx : -stepPx;
        b.phase = BirdPhase::Moving;
      }
    }
    const double tStart = b.plan.releaseT - static_cast<double>(bd.telegraphSec);
    if (now >= tStart && b.phase != BirdPhase::Telegraphing) {
      b.x = b.plan.x;
      b.phase = BirdPhase::Telegraphing;
      Event ev;
      ev.type = EventType::EggTelegraphed;
      ev.birdId = b.id;
      ev.eggType = b.plan.type;
      ev.x = b.x;
      ev.tau = bd.telegraphSec;
      publish(ev);
    }
    if (now + 1e-9 >= b.plan.releaseT) releaseEgg(b);
  }
}

void World::releaseEgg(Bird& b) {
  const Plan p = b.plan;
  const EggDef& ed = data_.eggs[static_cast<size_t>(p.type)];
  const double now = timeSec();
  for (Egg& e : eggs_) {
    if (e.alive) continue;
    e = Egg{};
    e.alive = true;
    e.bonus = ed.bonus;
    e.softMust = p.softMust;
    e.type = p.type;
    e.id = nextEggId_++;
    e.birdId = b.id;
    e.x = e.prevX = p.x;
    e.y = e.prevY = cfg::kPerchYPx;
    e.vy = ed.launchVelPxPerSec;
    e.gravityScale = ed.gravityScale;
    e.landT = now + kin::fallTime(ed.gravityScale, ed.launchVelPxPerSec);
    e.knownT = p.knownT;
    if (std::fabs(e.x - basket_.x) <= cfg::kMouthWidthPx * 0.5f + level_.forgivenessPx) e.coverStartT = now;
    Event ev;
    ev.type = EventType::EggReleased;
    ev.eggId = e.id;
    ev.birdId = b.id;
    ev.eggType = p.type;
    ev.x = e.x;
    ev.y = e.y;
    publish(ev);
    break;
  }
  b.lastReleaseT = now;
  ++b.dropCount;
  b.lastEggX = p.x;
  b.plan = Plan{};
  b.phase = BirdPhase::Idle;
}

// ---------------------------------------------------------------------------
// Eggs: semi-implicit Euler, swept crossing test of the mouth line.
void World::stepEggs() {
  const double nowStart = timeSec();
  const double nowEnd = nowStart + static_cast<double>(kDt);
  const float halfMouth = cfg::kMouthWidthPx * 0.5f;
  const float coverHalf = halfMouth + level_.forgivenessPx;
  const float halfBasket = cfg::kBasketWidthPx * 0.5f;

  for (Egg& e : eggs_) {
    if (!e.alive || status_ != RunStatus::Running) continue;
    const float prevX = e.x;
    const float prevY = e.y;
    e.vy += cfg::kGravityPxPerSec2 * e.gravityScale * kDt;
    e.y += e.vy * kDt;
    e.x += e.vx * kDt;

    if (e.y < cfg::kMouthYPx) {
      if (std::fabs(e.x - basket_.x) <= coverHalf) {
        if (e.coverStartT < 0.0) e.coverStartT = nowEnd;
      } else {
        e.coverStartT = -1.0;
      }
    }

    if (prevY < cfg::kMouthYPx && e.y >= cfg::kMouthYPx && e.rimCount < 2) {
      const float f = (cfg::kMouthYPx - prevY) / (e.y - prevY);
      const float crossX = prevX + (e.x - prevX) * f;
      const float bx = basket_.prevX + (basket_.x - basket_.prevX) * f;
      const float dx = crossX - bx;
      const float adx = std::fabs(dx);
      const double crossT = nowStart + static_cast<double>(f * kDt);
      if (adx <= coverHalf) {
        catchEgg(e, adx, crossT);
        continue;
      }
      if (e.rimCount == 0 && adx <= halfBasket) {
        e.rimCount = 1;
        e.vx = (dx >= 0.f ? 1.f : -1.f) * cfg::kRimBounceSpeedPxPerSec;
        e.vy = -std::fabs(e.vy) * cfg::kRimBounceRestitution;
        e.y = cfg::kMouthYPx - 0.5f;
        e.crossed = true;
        Event ev;
        ev.type = EventType::EggRimBounced;
        ev.eggId = e.id;
        ev.eggType = e.type;
        ev.x = crossX;
        ev.y = cfg::kMouthYPx;
        publish(ev);
      } else {
        e.crossed = true;
        if (e.rimCount == 1) e.rimCount = 2;
      }
    }

    if (e.alive && e.y >= cfg::kGroundYPx) missEgg(e);
  }
}

void World::catchEgg(Egg& e, float adx, double crossT) {
  const EggDef& ed = data_.eggs[static_cast<size_t>(e.type)];
  const float u = adx / (cfg::kMouthWidthPx * 0.5f);
  CatchQuality q = scoring::classifyQuality(u);
  double tau = (e.coverStartT >= 0.0) ? std::max(0.0, crossT - e.coverStartT) : 0.0;
  uint32_t flags = 0;
  if (e.bonus) flags |= kFlagBonusEgg;
  if (e.rimCount > 0) {  // rescued after a rim bounce: no quality or depth reward
    q = CatchQuality::Close;
    tau = 1.0;
    flags |= kFlagRescued;
  }

  const int block = static_cast<int>(crossT / static_cast<double>(cfg::kClutchBlockSec));
  if (block != clutchBlockIndex_) {
    clutchBlockIndex_ = block;
    clutchBonusesInBlock_ = 0;
  }
  const bool clutchAllowed = clutchBonusesInBlock_ < cfg::kClutchBonusesPerBlock;

  const int oldCombo = combo_;
  ++combo_;
  graceT_ = static_cast<double>(cfg::kComboGraceSec);
  const scoring::CatchScore cs = scoring::scoreCatch(ed.baseScore, q, tau, combo_, 1.0f, clutchAllowed);
  if (cs.clutch) {
    flags |= kFlagClutch;
    if (clutchAllowed) ++clutchBonusesInBlock_;
    ++result_.clutch;
  }

  result_.score += cs.score;
  ++result_.catches;
  rawCoins_ += ed.coins;
  if (e.type == EggType::Golden) ++result_.golden;
  switch (q) {
    case CatchQuality::Perfect: ++result_.perfect; break;
    case CatchQuality::Good: ++result_.good; break;
    case CatchQuality::Close: ++result_.close; break;
  }
  result_.bestCombo = std::max(result_.bestCombo, combo_);
  result_.maxMultiplier = std::max(result_.maxMultiplier, cs.multiplier);
  if (q == CatchQuality::Perfect && (flags & kFlagRescued) == 0) {
    ++perfectStreak_;
  } else {
    perfectStreak_ = 0;
  }
  result_.bestPerfectStreak = std::max(result_.bestPerfectStreak, perfectStreak_);
  if (e.birdId >= 0 && e.birdId < kMaxBirds) {
    const size_t bi = static_cast<size_t>(e.birdId);
    ++birdStreak_[bi];
    bestBirdStreak_[bi] = std::max(bestBirdStreak_[bi], birdStreak_[bi]);
  }
  if (!e.bonus) ++waveResolved_;

  Event ev;
  ev.type = EventType::CatchMade;
  ev.eggId = e.id;
  ev.birdId = e.birdId;
  ev.eggType = e.type;
  ev.quality = q;
  ev.flags = flags;
  ev.x = e.x;
  ev.y = cfg::kMouthYPx;
  ev.combo = combo_;
  ev.multiplier = cs.multiplier;
  ev.scoreDelta = cs.score;
  ev.tau = static_cast<float>(tau);
  publish(ev);

  Event cc;
  cc.type = EventType::ComboChanged;
  cc.combo = combo_;
  cc.value = oldCombo;
  cc.multiplier = cs.multiplier;
  publish(cc);

  e.alive = false;
  checkObjective();
}

void World::missEgg(Egg& e) {
  const EggDef& ed = data_.eggs[static_cast<size_t>(e.type)];
  Event ev;
  ev.type = EventType::EggMissed;
  ev.eggId = e.id;
  ev.birdId = e.birdId;
  ev.eggType = e.type;
  ev.x = e.x;
  ev.y = cfg::kGroundYPx;
  ev.value = e.bonus ? 0 : ed.missHalfHearts;
  publish(ev);
  const bool bonus = e.bonus;
  const int cost = ed.missHalfHearts;
  const int birdId = e.birdId;
  e.alive = false;
  if (!bonus) {
    ++result_.misses;
    ++waveResolved_;
    perfectStreak_ = 0;
    if (birdId >= 0 && birdId < kMaxBirds) birdStreak_[static_cast<size_t>(birdId)] = 0;
    breakCombo(0);
    if (level_.forgiveFirstMiss && !forgiveUsed_) {
      // Section 53: show the heart breaking, then heal it. Nothing is actually lost.
      forgiveUsed_ = true;
      Event lost;
      lost.type = EventType::HeartLost;
      lost.birdId = birdId;
      lost.value = cost;
      lost.combo = halfHearts_;
      lost.flags = 1;  // tutorial: will be healed
      publish(lost);
      Event healed;
      healed.type = EventType::HeartGained;
      healed.value = cost;
      healed.combo = halfHearts_;
      healed.flags = 1;
      publish(healed);
    } else {
      loseHearts(cost, birdId);
    }
  }
}

void World::breakCombo(int cause) {
  if (combo_ <= 0) return;
  Event ev;
  ev.type = EventType::ComboBroken;
  ev.combo = combo_;
  ev.value = cause;  // 0 = miss, 1 = grace timer ran out
  publish(ev);
  combo_ = 0;
}

void World::loseHearts(int halves, int birdId) {
  if (halves <= 0) return;
  const int before = halfHearts_;
  halfHearts_ = std::max(0, halfHearts_ - halves);
  result_.halfHeartsLost += before - halfHearts_;
  waveHeartsLost_ += before - halfHearts_;
  Event ev;
  ev.type = EventType::HeartLost;
  ev.birdId = birdId;
  ev.value = before - halfHearts_;
  ev.combo = halfHearts_;
  publish(ev);
  if (halfHearts_ == 0) finishRun(false);
}

void World::stepCombo() {
  if (combo_ <= 0) return;
  if (aliveEggCount() == 0) return;  // the grace timer only runs while an egg is in flight
  graceT_ -= static_cast<double>(kDt);
  if (graceT_ <= 0.0) breakCombo(1);
}

void World::checkEnd() {
  if (timeSec() < static_cast<double>(level_.durationSec) - 5.0) return;
  for (const Bird& b : birds_) {
    if (!b.finished || b.plan.active) return;
  }
  if (aliveEggCount() == 0) finishRun(true);
}

void World::finishRun(bool cleared) {
  if (status_ != RunStatus::Running) return;
  if (cleared) finishWave();  // the last (possibly partial) wave still counts
  status_ = cleared ? RunStatus::Cleared : RunStatus::GameOver;
  result_.cleared = cleared;
  result_.flawless = cleared && result_.halfHeartsLost == 0;
  result_.timeSec = timeSec();
  const double raw = static_cast<double>(rawCoins_) + static_cast<double>(result_.score / 100);
  result_.coins = static_cast<int>(std::floor(raw * static_cast<double>(level_.coinMultiplier) + 0.5));
  result_.xp = static_cast<int>(result_.score / 40) + 8 * result_.wavesCleared;
  result_.objectiveProgress = objectiveProgress();
  Event ev;
  ev.type = cleared ? EventType::LevelCleared : EventType::GameOver;
  ev.value = result_.flawless ? 1 : 0;
  ev.scoreDelta = result_.score;
  publish(ev);
}

// ---------------------------------------------------------------------------
// Waves (Phase 1 stand-in: one wave per cfg::kClutchBlockSec seconds of play).
void World::stepWaves() {
  const double tEnd = timeSec() + static_cast<double>(kDt);
  const int idx = static_cast<int>(tEnd / static_cast<double>(cfg::kClutchBlockSec));
  while (waveIndex_ < idx) finishWave();
}

void World::finishWave() {
  const bool counts = waveResolved_ >= 3;
  const bool flawless = counts && waveHeartsLost_ == 0;
  if (counts) {
    constexpr int kWaveCoins = 10;
    ++result_.wavesCleared;
    rawCoins_ += kWaveCoins;
    Event ev;
    ev.type = EventType::WaveCleared;
    ev.value = waveIndex_;
    ev.flags = flawless ? 1u : 0u;
    ev.scoreDelta = kWaveCoins;
    publish(ev);
    if (flawless) {
      ++result_.flawlessWaves;
      // A Flawless Wave heals half a heart where the preset allows it (Section 69).
      if (presetDef(preset_).flawlessHeal && halfHearts_ < maxHalfHearts_) {
        ++halfHearts_;
        Event h;
        h.type = EventType::HeartGained;
        h.value = 1;
        h.combo = halfHearts_;
        publish(h);
      }
    }
  }
  ++waveIndex_;
  waveHeartsLost_ = 0;
  waveResolved_ = 0;
}

// ---------------------------------------------------------------------------
int World::objectiveProgress() const {
  const LevelObjective& o = level_.objective;
  switch (o.type) {
    case ObjectiveType::None: return 0;
    case ObjectiveType::CatchEggs: return result_.catches;
    case ObjectiveType::PerfectStreak: return result_.bestPerfectStreak;
    case ObjectiveType::GoldenCatches: return result_.golden;
    case ObjectiveType::ComboReach: return result_.bestCombo;
    case ObjectiveType::BirdStreak:
      if (o.birdIndex >= 0 && o.birdIndex < kMaxBirds) return bestBirdStreak_[static_cast<size_t>(o.birdIndex)];
      return 0;
  }
  return 0;
}

void World::checkObjective() {
  if (objectiveMet_ || level_.objective.type == ObjectiveType::None) return;
  if (objectiveProgress() < level_.objective.target) return;
  objectiveMet_ = true;
  result_.objectiveMet = true;
  Event ev;
  ev.type = EventType::ObjectiveMet;
  ev.value = level_.objective.target;
  publish(ev);
}

uint64_t World::stateHash() const {
  Fnv f;
  f.mix(tick_);
  f.mix(static_cast<uint64_t>(result_.score));
  f.mix(static_cast<uint64_t>(combo_));
  f.mix(static_cast<uint64_t>(halfHearts_));
  f.mix(floatBits(basket_.x));
  f.mix(floatBits(basket_.vx));
  for (const Bird& b : birds_) {
    f.mix(floatBits(b.x));
    f.mix(static_cast<uint64_t>(b.dropCount));
  }
  for (const Egg& e : eggs_) {
    if (!e.alive) continue;
    f.mix(static_cast<uint64_t>(e.id));
    f.mix(floatBits(e.x));
    f.mix(floatBits(e.y));
  }
  f.mix(rng_.stateValue());
  return f.h;
}

}  // namespace er
