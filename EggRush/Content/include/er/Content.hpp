#pragma once
// JSON content loading and validation (Section 49). Qt Core only.
#include <QString>
#include <QStringList>
#include <vector>

#include "er/Defs.hpp"

namespace er {

struct ContentDb {
  GameData game;
  std::vector<LevelDef> levels;  // in campaign order

  const LevelDef* findLevel(const QString& id) const;
  int levelIndex(const QString& id) const;
};

struct ContentResult {
  bool ok = false;
  ContentDb db;
  QStringList errors;  // human-readable, one problem per line
};

class ContentLoader {
 public:
  // Loads eggs.json, birds.json and levels/*.json from `dir`.
  static ContentResult load(const QString& dir);
  // Looks next to the executable (data/), then at the compile-time source tree.
  static QString defaultDir();
  // "did you mean" helper, exposed for tests.
  static QString closestMatch(const QString& word, const QStringList& candidates);
};

}  // namespace er
