# 002: Comfort rule stricter than the fairness validator

**Status:** accepted (Phase 1)

## Problem
The Fairness Validator proves a must-catch egg is *reachable* given basket physics, the reaction buffer and what the player
can know. Reachable is not the same as comfortable: two must-catch eggs landing 0.05 s apart and ~170 px apart (the
validator's limit) leave the basket exactly one pixel of freedom. A perfect bot makes it; a human sees an unfair game.
An early build produced exactly these pairs and the *oracle* bot missed on two-bird levels because its own aim was naive,
which exposed that the planner was happy to schedule them.

## Decision
`Config.hpp`: `kPairWindowSec = 0.30`, `kPairMaxDxPx = 100`. When the planner places a must-catch egg it also requires that any
other must-catch egg landing within 0.30 s is at most 100 px away (two such eggs can then be caught by the same basket
position, since the mouth is 190 px wide). This is checked in the planner's `fair()` lambda in addition to the validator;
it can only reject more placements, never accept more.

Related: golden eggs are placed "softMust" (catchable *together with* nearby must-catch eggs when possible) so the Objective
star for "catch N golden eggs" is not a coin flip.

## Consequences
- Repairs (delay up to 5 x 0.05 s, then conversion to a golden bonus egg) happen a little more often; the bot tool reports
  the repair count per level (1-6: ~3.8 per run, 1-9: ~3.0).
- Tuned values live in one place; `Tests/test_invariants.cpp` checks they still agree with the validator constants.
- If playtests say the game is too forgiving, lower `kPairWindowSec`/raise `kPairMaxDxPx` toward the validator limit, re-run
  `eggrush_bot --level all --bot oracle` (must stay at 0 misses) and `--calibrate --write`.
