#pragma once
#include <cstdint>

namespace er {

enum class EggType : uint8_t { Normal = 0, Golden, Fast, Heavy, Count };
inline constexpr int kEggTypeCount = static_cast<int>(EggType::Count);

enum class CatchQuality : uint8_t { Close = 0, Good, Perfect };

enum class Preset : uint8_t { Chill = 0, Normal, Hardcore };

struct PresetDef {
  int startHalfHearts;  // hearts are stored in halves
  double reactionBufferSec;
  bool flawlessHeal;
};

constexpr PresetDef presetDef(Preset p) {
  switch (p) {
    case Preset::Chill: return {10, 0.25, true};
    case Preset::Hardcore: return {4, 0.12, false};
    case Preset::Normal: break;
  }
  return {6, 0.18, true};
}

inline const char* eggTypeName(EggType t) {
  switch (t) {
    case EggType::Normal: return "normal";
    case EggType::Golden: return "golden";
    case EggType::Fast: return "fast";
    case EggType::Heavy: return "heavy";
    case EggType::Count: break;
  }
  return "?";
}

inline const char* qualityName(CatchQuality q) {
  switch (q) {
    case CatchQuality::Close: return "Close";
    case CatchQuality::Good: return "Good";
    case CatchQuality::Perfect: return "Perfect";
  }
  return "?";
}

}  // namespace er
