#pragma once
#include <QString>

namespace er {

struct RenderChoice {
  bool useGl = false;
  QString reason;
};

// Decides between the OpenGL viewport and the plain software one (Section 55 fallback):
// software GL drivers and headless platforms are slower than Qt's raster engine, and a
// machine with no usable GL context must still run the game.
RenderChoice chooseRenderer(const QString& forced);

}  // namespace er
