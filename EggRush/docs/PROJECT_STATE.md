# Project state (read this first when resuming)

Last updated: 2026-10-04 · version 0.1.0 · Phase 0/1 vertical slice of `EggRush_Prompt_UPGRADED_v2.txt`.

## What works (verified by running it)
- Sim, Content, Tools and 7 test suites build warning-free (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, also with `-DER_WARNINGS_AS_ERRORS=ON`) and `ctest` passes 7/7.
- The Qt app builds and runs (Linux, Qt 6.4.2, software renderer): title → level select → game → pause → settings → resume → level select → title → quit, driven with real key events under Xvfb; full 60 s autoplay level to the result screen with clean exit (audio off, and audio engine on with no device).
- Fairness: oracle bot 0 misses over 7,200 runs (3 presets × 6 levels × 400 seeds). Determinism test (same seed → same event hash).

## What has NOT been verified
- Windows/macOS/other Qt versions (6.5+ should be fine; nothing 6.5-only is used on purpose).
- OpenGL viewport and real audio playback (no GPU/sound device in the build sandbox).
- Any human playtest. Difficulty, basket feel and rank thresholds are bot-tuned only.

## Decisions worth remembering
- 001 render host (QGraphicsView, fixed 1920×1080 logical scene, GL probe + software fallback).
- 002 pair-comfort rule (stricter than the validator).
- No `CMakePresets.json` on purpose: Qt Creator treats a presets file as an alternative to its own kits and the plain "open CMakeLists.txt → pick kit → Run" flow is the one that must never break.
- Bot runs (`--autoplay`) never write progress to the profile.
- Hens are drawn in front of the top HUD strip (they perch directly under it); the strip is a lower z-layer than the world on purpose.
- Screens may be `opaque()`: Settings and Result hide whatever is below them; Pause is see-through.
- `emit` and `slots` are Qt macros: the Sim must not use them as identifiers (it is `World::publish`).

## Known gaps / next steps (in suggested order)
1. **Playtest** with real hands; adjust `Config.hpp` (basket accel, telegraph) and the par/rank thresholds. Then `eggrush_bot --calibrate --write`.
2. Level 1-1 objective ("catch 20 eggs") is tight (~23 eggs appear). Either lower to 18 or lengthen the level.
3. Author the missing World-1 levels (1-4, 1-7, 1-8, 1-10); use the heavy egg somewhere (implemented, unused).
4. Verify on Windows (MSVC) and macOS; add `windeployqt`/`macdeployqt` packaging steps.
5. Bundle a font (see `assets/fonts/README.md`) and replace procedural audio where it sounds thin.
6. Remaining master-prompt systems: worlds 2+, shop/upgrades, daily challenge, achievements, localization, gamepad.

## Commands
```
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt>      # configure
cmake --build build                                         # build (also copies assets/data)
ctest --test-dir build --output-on-failure                  # 7 suites
build/bin/eggrush_bot --level all --bot oracle --preset normal --runs 400
build/bin/eggrush_bot --calibrate --write                   # refresh par scores
QT_QPA_PLATFORM=offscreen ER_AUDIO=off build/bin/eggrush --level 1-1 --autoplay --screenshot out.png --screenshot-after 8
```

## Gotchas
- After `--calibrate --write` rebuild once so `build/bin/data` picks up the new par scores (otherwise ranks are computed against stale par; par 0 ranks everything C).
- `build/bin/data` is a copy; edit `assets/data`, not the copy.
- Qt 6.4: `QSoundEffect` members need the full type in headers using `unique_ptr`.
