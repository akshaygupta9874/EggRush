# Changelog

## 0.1.0 (Phase 0/1 vertical slice)
- Qt-free deterministic Sim library (fixed 120 Hz step, seeded PCG32, kinematics, scoring, fairness validator with planner repairs, bots).
- Content loader/validator (JSON), profile model and crash-safe SaveSystem (atomic write, 3 rolling backups, checksum, single-instance lock, crash sentinel).
- Qt game shell: title, level select, game, pause, result and settings screens; procedural art, particles, camera shake, procedural audio and music; GL probe with software fallback; keyboard/mouse input; auto-pause on focus loss; global self-expiring notification toast.
- 6 World 1 levels (1-1, 1-2, 1-3, 1-5, 1-6, 1-9), 3 difficulty presets, 3 stars per level, ranks C/B/A/S against calibrated par scores.
- 7 QtTest suites (invariants, validator, scoring, world, content, save, fairness) and the `eggrush_bot` balance/calibration tool.
- QA command-line flags (`--level --autoplay --seed --preset --screen --screenshot --quit-after --save-dir --data --render --mute`).
