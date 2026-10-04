#pragma once
// Human-readable strings derived from content definitions.
#include <QString>

#include "er/Content.hpp"
#include "er/Types.hpp"

namespace er {

inline QString objectiveText(const ContentDb& db, const LevelDef& l) {
  const int n = l.objective.target;
  switch (l.objective.type) {
    case ObjectiveType::None: return QStringLiteral("Clear the level");
    case ObjectiveType::CatchEggs: return QStringLiteral("Catch %1 eggs").arg(n);
    case ObjectiveType::PerfectStreak: return QStringLiteral("%1 Perfect catches in a row").arg(n);
    case ObjectiveType::GoldenCatches: return QStringLiteral("Catch %1 golden eggs").arg(n);
    case ObjectiveType::ComboReach: return QStringLiteral("Reach a combo of %1").arg(n);
    case ObjectiveType::BirdStreak: {
      QString who = QStringLiteral("one bird");
      const size_t bi = static_cast<size_t>(l.objective.birdIndex);
      if (bi < l.birds.size()) {
        const size_t def = static_cast<size_t>(l.birds[bi].birdDef);
        if (def < db.game.birds.size()) who = QString::fromStdString(db.game.birds[def].displayName);
      }
      return QStringLiteral("Catch %1 eggs in a row from the %2").arg(n).arg(who);
    }
  }
  return QString();
}

inline QString presetName(Preset p) {
  switch (p) {
    case Preset::Chill: return QStringLiteral("Chill");
    case Preset::Normal: return QStringLiteral("Normal");
    case Preset::Hardcore: return QStringLiteral("Hardcore");
  }
  return QString();
}

inline QString presetInfo(Preset p) {
  const PresetDef d = presetDef(p);
  return QStringLiteral("%1 hearts to start  ·  %2  ·  %3 s reaction margin")
      .arg(d.startHalfHearts / 2.0, 0, 'g', 2)
      .arg(d.flawlessHeal ? QStringLiteral("flawless waves heal") : QStringLiteral("no healing"))
      .arg(d.reactionBufferSec, 0, 'f', 2);
}

}  // namespace er
