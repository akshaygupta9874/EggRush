# Egg Rush

A fair-play egg-catching arcade game in Qt 6 / C++20. Hens lay eggs, you slide the basket, combos multiply your score, and **every must-catch egg is mathematically catchable** (see "The fairness guarantee" below).

This is the **Phase 0/1 vertical slice** of the master prompt (`EggRush_Prompt_UPGRADED_v2.txt`): the whole game loop is playable end to end, but it is not the full game. Section "Status" lists exactly what is in and what is not.

---

## Quick start (Qt Creator)

1. Install **Qt 6.5 or newer** (6.4 also works; it is what this was built against) with the *Desktop* kit for your compiler, plus the **Qt Multimedia** add-on module. Widgets, OpenGL Widgets and Test ship with the base install.
2. Qt Creator → **File → Open File or Project…** → pick the **`CMakeLists.txt` in this folder** (the root one, not one of the sub-folders).
3. Choose your Desktop kit → **Configure Project**.
4. Make sure the run target in the bottom-left is **`eggrush`** (not `eggrush_bot` or a test), then **Build (Ctrl+B)** and **Run (Ctrl+R)**.

Game data (`assets/data`) is copied next to the binary automatically (`build/bin/data`), so the game runs straight from the build folder.

### Command line

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/eggrush
```

CMake options: `ER_BUILD_APP` (default ON; turn OFF to build only the Qt-free Sim + tests + bot tool), `ER_BUILD_TESTS` (ON), `ER_WARNINGS_AS_ERRORS` (OFF).

---

## Controls

| Input | Action |
|---|---|
| ← → or A D | move the basket (hold both: last pressed wins) |
| mouse | the basket follows the pointer (switching to keys takes over again) |
| Esc / P | pause |
| M | mute |
| F11 or Alt+Enter | fullscreen |
| F12 | save a screenshot (to the save folder) |
| F3 | debug overlay (tick, eggs, repairs, seed, state hash) |
| Enter / Space, ↑ ↓ ← → | menus (every menu is fully keyboard-navigable) |

The window auto-pauses when it loses focus.

---

## Status: what is in this slice

**Playable**
- Title, level select (with locks, stars, ranks, difficulty switcher), game, pause, result (animated score, 3 stars, rank, coins, XP), settings.
- **6 levels of World 1**: ids `1-1, 1-2, 1-3, 1-5, 1-6, 1-9`. The gaps are levels not authored yet; adding one is a new JSON file in `assets/data/levels/`, no code.
- Eggs: normal, golden (bonus, never costs a heart), fast. A **heavy** egg type is implemented and tested in the Sim but no level uses it yet.
- Birds: hen (teleports lane to lane), moving bird, fast bird.
- Three difficulty presets (Chill / Normal / Hardcore: hearts, reaction buffer, flawless-wave healing).
- Scoring: catch quality (Close/Good/Perfect) × combo multiplier (cap ×8) × depth, clutch bonuses, waves, per-level objective star, flawless star, coins and XP, ranks C/B/A/S against a par score.
- Save system: atomic writes (`QSaveFile`), rolling backups, SHA-256 checksum, corruption recovery with an on-screen note, crash-sentinel detection, single-instance lock.
- Presentation: procedural vector art (no image files), particles, popups, camera shake (can be turned off), procedural sound effects and a music loop (generated at first run into the cache folder), reduced-motion and high-contrast options.
- Rendering: OpenGL viewport when a GPU context probe succeeds, automatic software fallback otherwise. Force with `--render gl|software`.

**Not in this slice** (still to build from the master prompt): worlds 2+, the other 4 World-1 levels, boss/hazard systems beyond the validator hooks, shop/upgrades/cosmetics, daily challenge, achievements, localization, gamepad, installer/packaging, bundled fonts and image/audio assets.

---

## The fairness guarantee (the part that is actually verified)

The planner (`Sim/src/World.cpp`, `planBird`) asks the **Fairness Validator** (`Sim/src/FairnessValidator.cpp`) before it commits any must-catch egg: given the basket's real physics (1400 px/s max, 9000 px/s² accel, the preset's reaction buffer, and only the information the player actually has, i.e. telegraphs), can the basket reach it in time from *every* position it might be in? If not, the planner delays the egg (up to 5 × 0.05 s) or turns it into a golden bonus egg that is allowed to be missed.

On top of the validator there is a stricter **comfort rule** (decision record 002): two must-catch eggs landing within 0.30 s of each other are at most 100 px apart, because "reachable by exactly one pixel" is possible for a machine and miserable for a person.

**Evidence** (reproduce with `eggrush_bot`):

| Check | Result |
|---|---|
| Oracle bot (perfect play), 3 presets × 6 levels × 400 seeds = 7,200 runs | **0 misses**, 100% clear/objective/flawless |
| Average bot (reaction delay + aim noise), Normal, 300 runs/level | 100% clear; 100% flawless on 5 of 6 levels (94% on 1-9) |
| Novice bot, Normal, 300 runs/level | 100% clear; ~45% Perfect catches; flawless 54–100% |
| Same seed twice | identical event stream and state hash (determinism test) |

What this does **not** prove: that the game is fun, or that the difficulty curve is right for humans. Bots have no nerves. The par scores come from a perfect-play bot, so **rank S (≥90% of par) is very hard by design and may need loosening after real playtests** (see `docs/PROJECT_STATE.md`).

---

## Command-line flags (QA / demos)

```
eggrush --level 1-3            start straight in a level
        --autoplay             a bot plays it (demo / smoke test)
        --seed 42              reproducible run
        --preset chill|normal|hardcore
        --screen title|levels|settings|result|pause
        --render gl|software
        --mute
        --save-dir <dir>       keep profile/log/cache here instead of the user data folder
        --data <dir>           load game data from here
        --screenshot out.png --screenshot-after 2.0   grab the view and quit
        --quit-after 30        exit after N seconds
```

Headless example: `QT_QPA_PLATFORM=offscreen ER_AUDIO=off ./build/bin/eggrush --level 1-1 --autoplay --seed 5 --screenshot shot.png --screenshot-after 8`

Balance tool:

```
eggrush_bot --level all --bot oracle|expert|average|novice --preset normal --runs 400 --seed 1000
eggrush_bot --calibrate --write     # recompute par scores into assets/data/levels/*.json
```

Re-run `--calibrate --write` after changing anything in the Sim, scoring or level files, then rebuild (the data copy step refreshes `build/bin/data`).

---

## Layout

```
Sim/        er_sim, a static library with NO Qt dependency: fixed-step world (120 Hz), seeded PCG32,
            kinematics, fairness validator, scoring, bots. Deterministic: same seed + inputs = same run.
Content/    er_content: JSON loading + validation (Qt Core only), profile model, SaveSystem.
App/        the Qt game: QGraphicsView host, screens, art, FX, audio, UI kit.
Tools/      eggrush_bot (headless balance / calibration).
Tests/      7 QtTest suites: invariants, validator, scoring, world, content, save, fairness.
assets/data game data (eggs, birds, levels) as JSON.
docs/       PROJECT_STATE.md (read this first when resuming), decisions/ (short ADRs).
```

The rule that keeps this maintainable: **the Sim never draws and the View never decides.** Everything on screen is derived from `World` state plus the events it publishes.

---

## Configuration and files

| What | Where |
|---|---|
| Profile (progress, settings) | the OS app-data folder for `EggRush` (e.g. `~/.local/share/EggRush`, `%APPDATA%\EggRush`) or `--save-dir` |
| Log | `eggrush.log` in the same folder (rotates at 1 MB) |
| Generated audio cache | `cache/` in the same folder (safe to delete) |
| Game data | `<exe folder>/data`, falling back to the source `assets/data` |

To reset progress, delete the profile folder (or its `profile.json` and `profile.bak*` files) while the game is closed.

---

## Known limits (honest list)

- **Built and tested on Linux only** (g++ 13, CMake, Ninja, Qt 6.4.2, headless/offscreen plus screenshots). Windows (MSVC/MinGW) and macOS builds have not been compiled by me. The code uses only portable Qt API and the CMake is standard, but treat the first Windows/macOS build as untested.
- The **OpenGL path** and **real audio output** were not exercised (the build machine has no GPU or sound device); the software renderer and the generated-audio code path run, and playback was only checked for not crashing. If audio misbehaves, run with `--mute` or `ER_AUDIO=off` and tell me.
- **Not playtested by a human.** Feel (basket acceleration, telegraph length, hit box) and difficulty are tuned with bots only. Constants are in `Sim/include/er/Config.hpp`.
- Level 1-1's objective ("catch 20 eggs") needs about 23 eggs to appear in the 60 s level, so it leaves little slack with a first-time player who misses early.
- Fonts: the UI uses the first installed of Poppins, Inter, Segoe UI, Helvetica Neue, Noto Sans, DejaVu Sans, Arial. No font files are bundled (licensing), so spacing differs slightly between machines.
- The hens perch under the top HUD strip and are drawn **in front** of it, so a hen can briefly cover part of the score/level text.
