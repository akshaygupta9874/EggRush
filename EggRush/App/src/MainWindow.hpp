#pragma once
#include <QGraphicsView>
#include <QMainWindow>

namespace er {

class AppController;

// The one widget that shows the scene: letterboxed 16:9 logical space, GL or software.
class GameView : public QGraphicsView {
  Q_OBJECT
 public:
  explicit GameView(AppController& app, QWidget* parent = nullptr);
  void useOpenGL(bool gl);
  bool usingOpenGL() const { return gl_; }

 protected:
  void keyPressEvent(QKeyEvent* e) override;
  void keyReleaseEvent(QKeyEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void changeEvent(QEvent* e) override;
  void focusOutEvent(QFocusEvent* e) override;

 private:
  AppController& app_;
  bool gl_ = false;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow(AppController& app, bool useGl);
  GameView* view() { return view_; }

 protected:
  void closeEvent(QCloseEvent* e) override;

 private slots:
  void setFullscreen(bool on);
  void verifyGl();

 private:
  AppController& app_;
  GameView* view_;
};

}  // namespace er
