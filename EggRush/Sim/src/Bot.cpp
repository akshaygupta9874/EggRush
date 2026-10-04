#include "er/Bot.hpp"

#include <algorithm>
#include <cmath>

#include "er/Kinematics.hpp"

namespace er {

Bot::Target& Bot::touch(int64_t key, int birdId, float x, double landT, bool must, double now) {
  for (Target& t : targets_) {
    if (t.key == key) {
      t.landT = landT;
      return t;
    }
  }
  Target t;
  t.key = key;
  t.birdId = birdId;
  t.x = x;
  t.landT = landT;
  t.must = must;
  const float delay = profile_.reactionSec * std::exp(0.2f * rng_.gaussian());
  t.knownAt = now + static_cast<double>(delay);
  t.aimX = x + profile_.aimSigmaPx * rng_.gaussian();
  targets_.push_back(t);
  return targets_.back();
}

float Bot::chooseAim(const Target& best, double now) const {
  constexpr float kHalfCatchPx = (cfg::kMouthWidthPx * 0.5f) - cfg::kCatchMarginPx;  // 85
  const float half = std::max(12.f, kHalfCatchPx - 2.f * profile_.aimSigmaPx);
  float lo = best.aimX - half;
  float hi = best.aimX + half;
  double clusterEnd = best.landT;
  // Eggs landing almost together: stand where one basket catches all of them.
  for (const Target& t : targets_) {
    if (&t == &best || !t.must || t.knownAt > now) continue;
    if (std::fabs(t.landT - best.landT) >= 0.35) continue;
    const float nlo = std::max(lo, t.aimX - half);
    const float nhi = std::min(hi, t.aimX + half);
    if (nlo > nhi) continue;  // too far apart to share a basket: treated as the "next" egg
    lo = nlo;
    hi = nhi;
    clusterEnd = std::max(clusterEnd, t.landT);
  }
  float aim = 0.5f * (lo + hi);

  // The next egg after the cluster: lean toward it only as far as needed to reach it.
  const Target* next = nullptr;
  for (const Target& t : targets_) {
    if (!t.must || t.knownAt > now || t.landT <= clusterEnd + 1e-9) continue;
    if (std::fabs(t.landT - best.landT) < 0.35) continue;
    if (next == nullptr || t.landT < next->landT) next = &t;
  }
  if (next != nullptr) {
    const double spare = next->landT - clusterEnd - 0.04;
    const float reachPx = static_cast<float>(std::max(0.0, kin::reach(spare) - 20.0));
    const float wlo = next->aimX - half - reachPx;
    const float whi = next->aimX + half + reachPx;
    const float a = std::max(lo, wlo);
    const float b = std::min(hi, whi);
    if (a <= b) {
      aim = std::clamp(aim, a, b);
    } else {
      aim = (next->aimX > aim) ? hi : lo;
    }
  }
  return aim;
}

InputSnapshot Bot::decide(const World& w) {
  const double now = w.timeSec();
  const GameData& data = w.data();

  for (Target& t : targets_) t.seen = false;

  // Telegraphing birds reveal their next egg (x and landing time are visible via the
  // lay animation and the landing shadow).
  for (const Bird& b : w.birds()) {
    if (b.phase != BirdPhase::Telegraphing || !b.plan.active) continue;
    const bool must = !data.eggs[static_cast<size_t>(b.plan.type)].bonus || b.plan.softMust;
    const int64_t key = -(static_cast<int64_t>(b.id) * 100000 + b.dropCount + 1);
    touch(key, b.id, b.plan.x, b.plan.landT, must, now).seen = true;
  }
  // Falling eggs: if a telegraph target for the same bird and x exists, inherit it so
  // the reaction delay is not paid twice.
  for (const Egg& e : w.eggs()) {
    if (!e.alive || e.crossed) continue;
    Target* inherited = nullptr;
    for (Target& t : targets_) {
      if (t.key < 0 && t.birdId == e.birdId && std::fabs(t.x - e.x) < 1.f) {
        inherited = &t;
        break;
      }
    }
    if (inherited != nullptr) {
      inherited->key = e.id;
      inherited->landT = e.landT;
      inherited->seen = true;
    } else {
      touch(e.id, e.birdId, e.x, e.landT, !e.bonus || e.softMust, now).seen = true;
    }
  }
  targets_.erase(std::remove_if(targets_.begin(), targets_.end(),
                                [](const Target& t) { return !t.seen; }),
                 targets_.end());

  // Choose: earliest-landing known must-catch egg. Bonus eggs only when no must-catch
  // egg lands within half a second of them and they are reachable.
  const Target* best = nullptr;
  for (const Target& t : targets_) {
    if (t.knownAt > now) continue;
    if (t.must) {
      if (best == nullptr || t.landT < best->landT) best = &t;
    }
  }
  if (best == nullptr) {
    for (const Target& t : targets_) {
      if (t.knownAt > now || t.must) continue;
      bool clash = false;
      for (const Target& o : targets_) {
        if (o.must && std::fabs(o.landT - t.landT) < 0.5) clash = true;
      }
      const double dist = std::fabs(static_cast<double>(t.aimX - w.basket().x));
      if (!clash && kin::reach(t.landT - now - 0.1) >= dist) {
        if (best == nullptr || t.landT < best->landT) best = &t;
      }
    }
  }

  InputSnapshot in;
  if (best == nullptr) return in;
  const float aim = std::clamp(best->must ? chooseAim(*best, now) : best->aimX, cfg::kBasketMinXPx,
                               cfg::kBasketMaxXPx);
  const float dist = aim - w.basket().x;
  const float v = w.basket().vx;
  const int dir = dist > 0.f ? 1 : -1;
  const float vdes = std::min(cfg::kBasketMaxSpeedPxPerSec,
                              std::sqrt(2.f * cfg::kBasketDecelPxPerSec2 * 0.85f * std::fabs(dist)));
  if (std::fabs(dist) < 3.f && std::fabs(v) < 30.f) {
    in.axis = 0;
  } else if (v * static_cast<float>(dir) < 0.f) {
    in.axis = static_cast<int8_t>(dir);
  } else if (std::fabs(v) <= vdes) {
    in.axis = static_cast<int8_t>(dir);
  } else {
    in.axis = 0;
  }
  return in;
}

BotRunStats simulateRun(const GameData& data, const LevelDef& level, Preset preset, uint64_t seed,
                        const BotProfile& profile, double maxSeconds) {
  World w;
  EventLog log;
  w.setSink(&log);
  w.reset(data, level, preset, seed);
  Bot bot(profile, seed ^ 0x9e3779b97f4a7c15ULL);
  const uint32_t maxTicks = static_cast<uint32_t>(maxSeconds / static_cast<double>(cfg::kSimStepSec));
  while (w.status() == RunStatus::Running && w.tick() < maxTicks) {
    w.step(bot.decide(w));
  }
  BotRunStats out;
  out.result = w.result();
  out.telegraphs = log.count(EventType::EggTelegraphed);
  out.repairs = log.count(EventType::SpawnRepaired);
  return out;
}

double wilsonLowerBound(int successes, int trials, double z) {
  if (trials <= 0) return 0.0;
  const double n = static_cast<double>(trials);
  const double p = static_cast<double>(successes) / n;
  const double d = 1.0 + z * z / n;
  const double c = p + z * z / (2.0 * n);
  const double r = z * std::sqrt(p * (1.0 - p) / n + z * z / (4.0 * n * n));
  return (c - r) / d;
}

}  // namespace er
