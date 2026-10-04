#include "MainWindow.hpp"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>

#include "AppController.hpp"
#include "Art.hpp"
#include "Theme.hpp"

namespace er {

GameView::GameView(AppController& app, QWidget* parent) : QGraphicsView(app.scene(), parent), app_(app) {
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setBackgroundBrush(Qt::black);
  setAlignment(Qt::AlignCenter);
  setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setOptimizationFlag(QGraphicsView::DontSavePainterState, false);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  viewport()->setMouseTracking(true);
  setSceneRect(0, 0, ui::kW, ui::kH);
}

void GameView::useOpenGL(bool gl) {
  gl_ = gl;
  if (gl) {
    auto* w = new QOpenGLWidget;
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSamples(4);
    fmt.setSwapInterval(1);
    w->setFormat(fmt);
    setViewport(w);
  } else {
    setViewport(new QWidget);
  }
  viewport()->setMouseTracking(true);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void GameView::keyPressEvent(QKeyEvent* e) { app_.keyPressed(e); }
void GameView::keyReleaseEvent(QKeyEvent* e) { app_.keyReleased(e); }

void GameView::resizeEvent(QResizeEvent* e) {
  QGraphicsView::resizeEvent(e);
  fitInView(sceneRect(), Qt::KeepAspectRatio);
}

void GameView::mouseMoveEvent(QMouseEvent* e) {
  QGraphicsView::mouseMoveEvent(e);
  const QPointF s = mapToScene(e->pos());
  app_.mouseMoved(s.x(), s.y());
}

void GameView::changeEvent(QEvent* e) {
  QGraphicsView::changeEvent(e);
  if (e->type() == QEvent::ActivationChange && !isActiveWindow()) app_.windowDeactivated();
}

void GameView::focusOutEvent(QFocusEvent* e) {
  QGraphicsView::focusOutEvent(e);
  app_.windowDeactivated();
}

MainWindow::MainWindow(AppController& app, bool useGl) : app_(app) {
  setWindowTitle(QStringLiteral("Egg Rush"));
  setWindowIcon(QIcon(art::windowIcon()));
  view_ = new GameView(app, this);
  view_->useOpenGL(useGl);
  setCentralWidget(view_);
  app.attachView(view_);
  resize(1280, 720);
  setMinimumSize(640, 360);
  connect(&app, &AppController::fullscreenRequested, this, &MainWindow::setFullscreen);
  if (const QScreen* s = QGuiApplication::primaryScreen()) {
    const QRect a = s->availableGeometry();
    move(a.center() - rect().center());
  }
  if (useGl) QTimer::singleShot(800, this, &MainWindow::verifyGl);
}

void MainWindow::setFullscreen(bool on) {
  if (on && !isFullScreen()) showFullScreen();
  if (!on && isFullScreen()) showNormal();
  view_->setFocus();
}

void MainWindow::verifyGl() {
  auto* gl = qobject_cast<QOpenGLWidget*>(view_->viewport());
  if (gl != nullptr && !gl->isValid()) {
    qWarning("OpenGL viewport failed to initialise; falling back to the software renderer");
    view_->useOpenGL(false);
  }
}

void MainWindow::closeEvent(QCloseEvent* e) {
  app_.saveProfile();
  e->accept();
}

}  // namespace er
