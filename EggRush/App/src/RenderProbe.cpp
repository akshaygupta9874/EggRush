#include "RenderProbe.hpp"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

namespace er {

RenderChoice chooseRenderer(const QString& forced) {
  RenderChoice rc;
  if (forced == QLatin1String("software")) {
    rc.reason = QStringLiteral("software renderer (requested)");
    return rc;
  }
  const QString platform = QGuiApplication::platformName();
  if (forced.isEmpty() && (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))) {
    rc.reason = QStringLiteral("software renderer (platform '%1' has no GPU)").arg(platform);
    return rc;
  }
  QOpenGLContext ctx;
  QSurfaceFormat fmt;
  fmt.setRenderableType(QSurfaceFormat::DefaultRenderableType);
  ctx.setFormat(fmt);
  if (!ctx.create()) {
    rc.reason = QStringLiteral("software renderer (no OpenGL context could be created)");
    return rc;
  }
  QOffscreenSurface surf;
  surf.setFormat(ctx.format());
  surf.create();
  if (!ctx.makeCurrent(&surf)) {
    rc.reason = QStringLiteral("software renderer (OpenGL context could not be made current)");
    return rc;
  }
  const char* renderer = reinterpret_cast<const char*>(ctx.functions()->glGetString(GL_RENDERER));
  const QString name = renderer != nullptr ? QString::fromLatin1(renderer) : QString();
  ctx.doneCurrent();
  const bool softGl = name.contains(QLatin1String("llvmpipe"), Qt::CaseInsensitive) ||
                      name.contains(QLatin1String("softpipe"), Qt::CaseInsensitive) ||
                      name.contains(QLatin1String("software"), Qt::CaseInsensitive);
  if (softGl && forced != QLatin1String("gl")) {
    rc.reason = QStringLiteral("software renderer (OpenGL is software-emulated: %1)").arg(name);
    return rc;
  }
  rc.useGl = true;
  rc.reason = QStringLiteral("OpenGL viewport (%1)").arg(name);
  return rc;
}

}  // namespace er
