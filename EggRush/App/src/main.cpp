// Egg Rush: entry point. Sets up logging, single-instance + crash detection, content and
// profile loading, the render host, and then hands over to the AppController.
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <cstdio>
#include <memory>

#include "AppController.hpp"
#include "MainWindow.hpp"
#include "RenderProbe.hpp"
#include "er/Content.hpp"
#include "er/SaveSystem.hpp"

namespace {

QFile* gLog = nullptr;

void logHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
  const char* tag = type == QtDebugMsg ? "DEBUG" : type == QtInfoMsg ? "INFO " : type == QtWarningMsg ? "WARN " : "ERROR";
  const QString line = QStringLiteral("%1 [%2] %3\n").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), tag, msg);
  std::fputs(line.toLocal8Bit().constData(), stderr);
  if (gLog != nullptr && gLog->isOpen()) {
    gLog->write(line.toUtf8());
    gLog->flush();
  }
}

void fatalBox(const QString& title, const QString& text) {
  std::fprintf(stderr, "%s: %s\n", qPrintable(title), qPrintable(text));
  if (QGuiApplication::platformName() != QLatin1String("offscreen")) QMessageBox::critical(nullptr, title, text);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("EggRush"));
  QApplication::setOrganizationName(QStringLiteral("EggRush"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

  QCommandLineParser cli;
  cli.setApplicationDescription(QStringLiteral("Egg Rush - a fair-play egg catching arcade game"));
  cli.addHelpOption();
  cli.addVersionOption();
  cli.addOption({"data", "Content directory (eggs.json, birds.json, levels/)", "dir"});
  cli.addOption({"save-dir", "Where to keep the profile, log and cache", "dir"});
  cli.addOption({"level", "Start straight in a level, e.g. 1-3", "id"});
  cli.addOption({"screen", "Open a screen for visual QA: title | levels | settings | result | pause", "name"});
  cli.addOption({"preset", "chill | normal | hardcore (this launch only)", "name"});
  cli.addOption({"seed", "Fixed run seed (reproducible levels)", "n"});
  cli.addOption({"autoplay", "A bot plays the level (demo / smoke test)"});
  cli.addOption({"mute", "No audio"});
  cli.addOption({"render", "gl | software (default: auto)", "mode"});
  cli.addOption({"quit-after", "Quit after this many seconds (smoke tests)", "sec"});
  cli.addOption({"screenshot", "Save a screenshot to this file and quit", "file"});
  cli.addOption({"screenshot-after", "Seconds to wait before the screenshot (default 1.0)", "sec", "1.0"});
  cli.process(app);

  er::LaunchOptions opts;
  opts.dataDir = cli.value("data");
  opts.saveDir = cli.value("save-dir");
  opts.level = cli.value("level");
  opts.screen = cli.value("screen");
  opts.autoplay = cli.isSet("autoplay");
  opts.mute = cli.isSet("mute");
  opts.render = cli.value("render");
  opts.quitAfterSec = cli.value("quit-after").toDouble();
  opts.screenshotPath = cli.value("screenshot");
  opts.screenshotAfterSec = cli.value("screenshot-after").toDouble();
  if (cli.isSet("seed")) {
    opts.seed = cli.value("seed").toULongLong();
    opts.seedSet = true;
  }
  if (cli.isSet("preset")) {
    const QString p = cli.value("preset").toLower();
    opts.preset = p == "chill" ? 0 : p == "hardcore" ? 2 : 1;
  }

  const QString saveDir = opts.saveDir.isEmpty() ? er::SaveSystem::defaultDir() : opts.saveDir;
  QDir().mkpath(saveDir);
  QFile log(saveDir + QStringLiteral("/eggrush.log"));
  if (log.size() > 1024 * 1024) log.remove();
  if (log.open(QIODevice::Append | QIODevice::Text)) gLog = &log;
  qInstallMessageHandler(logHandler);
  qInfo("Egg Rush 0.1.0 starting (Qt %s, platform %s)", qVersion(), qPrintable(QGuiApplication::platformName()));

  er::SessionGuard guard(saveDir);
  if (!guard.acquire()) {
    fatalBox(QStringLiteral("Egg Rush"), QStringLiteral("Egg Rush is already running."));
    return 3;
  }

  const QString dataDir = opts.dataDir.isEmpty() ? er::ContentLoader::defaultDir() : opts.dataDir;
  er::ContentResult content = er::ContentLoader::load(dataDir);
  if (!content.ok) {
    fatalBox(QStringLiteral("Egg Rush - content error"),
             QStringLiteral("The game data in\n%1\ncould not be loaded:\n\n%2").arg(dataDir, content.errors.join(QLatin1Char('\n'))));
    return 2;
  }
  qInfo("content: %zu levels from %s", content.db.levels.size(), qPrintable(dataDir));

  er::SaveSystem saves(saveDir);
  er::Profile profile;
  const er::LoadReport rep = saves.load(profile);
  QString note;
  for (const QString& n : rep.notes) qWarning("save: %s", qPrintable(n));
  if (rep.source == er::LoadReport::Source::Backup) {
    note = QStringLiteral("Your progress file was damaged; restored from backup %1.").arg(rep.backupIndex);
  } else if (!rep.notes.isEmpty()) {
    note = rep.notes.last();
  } else if (guard.previousSessionCrashed()) {
    note = QStringLiteral("Egg Rush did not close cleanly last time. Your progress is safe.");
  }

  const er::RenderChoice rc = er::chooseRenderer(opts.render);
  qInfo("renderer: %s", qPrintable(rc.reason));

  er::AppController controller(opts, std::move(content.db), profile, std::move(saves));
  controller.setNote(note);
  er::MainWindow window(controller, rc.useGl);
  QObject::connect(&controller, &er::AppController::quitRequested, &app, &QApplication::quit);
  window.show();
  controller.applySettings();
  controller.start();
  window.view()->setFocus();

  const int rcode = app.exec();
  controller.saveProfile();
  qInfo("Egg Rush exiting (%d)", rcode);
  qInstallMessageHandler(nullptr);
  return rcode;
}
