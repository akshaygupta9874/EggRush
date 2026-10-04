# 001: Render host = QGraphicsView on a 1920x1080 logical scene

**Status:** accepted (Phase 1)

## Decision
All screens are `QGraphicsObject`s in one `QGraphicsScene` of fixed logical size 1920x1080. `GameView` (a `QGraphicsView`)
calls `fitInView(..., KeepAspectRatio)`, so the game letterboxes cleanly at any window size and every coordinate in the
Sim, art and UI is in the same logical pixels. The viewport is a `QOpenGLWidget` when a probe says a real GPU context is
available, otherwise a plain `QWidget` (software raster).

## Why
- The Sim is in logical pixels already (fall height 860 px, basket 220 px, ...); a fixed scene means no unit conversion in the view.
- Retained items give us focus handling, hover and mouse events for the menus for free; the gameplay layer paints procedurally inside one `WorldView` item, so it can be shaken/rotated as one piece.
- Software fallback matters: remote desktops, VMs and CI have no usable GL. The probe (`RenderProbe.cpp`) treats llvmpipe/softpipe as "no GPU" and picks the raster path, which at 1080p logical is plenty for this art.
- Everything draws from vector primitives, so there are no image assets to ship or mis-scale.

## Consequences
- `FullViewportUpdate` redraws the whole scene each frame. Fine for this scene size; if profiling on weak GPUs says otherwise, cache the static background into a `QPixmap`.
- Menus are a small custom kit (`UiKit.*`), not `QWidget`s: accessibility (screen readers) is therefore limited. Keyboard navigation and high-contrast / reduced-motion are covered, screen-reader support is not.
- `--render gl|software` exists so a bad GL driver never traps a player.
- Alternatives rejected: Qt Quick (extra runtime + QML toolchain for a game whose Sim is C++), widgets-only (no clean way to scale a fixed-size game).
