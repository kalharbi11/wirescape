#include "studio_controller.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QDir>
#include <QTimer>
#include <QSaveFile>
#include <QQuickStyle>
#include <QIcon>

int main(int argc, char** argv) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/assets/wirescape.png"));
#ifdef Q_OS_WIN
    // Also supplies fonts to Qt's offscreen test platform; use the installed
    // system font in place, without redistributing proprietary font files.
    const QString windowsDirectory = qEnvironmentVariable("WINDIR");
    if (!windowsDirectory.isEmpty())
        QFontDatabase::addApplicationFont(windowsDirectory + "/Fonts/arial.ttf");
#endif
    app.setApplicationName("Wirescape"); app.setOrganizationName("Wirescape"); app.setApplicationVersion("0.6.0-foundation.1");
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"confirm-lan", "Authorize OSC In modules with LAN enabled to bind beyond localhost."});
    parser.addOption({"confirm-midi", "Allow the user to explicitly enable real MIDI endpoints in settings."});
    parser.addOption({"smoke-test", "Exercise the native UI and engine offline, then exit. No audio device is opened."});
    parser.addOption({"screenshot", "Save an image during the smoke test.", "path"});
    parser.addOption({"report", "Save the smoke-test JSON report.", "path"});
    parser.addOption({"modules", "Directory containing installed native processor libraries.", "directory"});
    parser.addOption({"local-font", "Load an optional private local font file, without installing it.", "path"});
    parser.addOption({"ready-report", "Write startup readiness without opening audio.", "path"});
    parser.addOption({"window-size", "Initial logical window size, at least 1000x650.", "widthxheight"});
    parser.addOption({"startup-check", "Exit after writing a ready report; never start audio."});
    parser.process(app);
    if (parser.isSet("local-font")) {
        const int font = QFontDatabase::addApplicationFont(parser.value("local-font"));
        const auto families = QFontDatabase::applicationFontFamilies(font);
        if (!families.isEmpty()) app.setFont(QFont(families.first()));
    }
    const bool smoke = parser.isSet("smoke-test");
    if (parser.isSet("startup-check") && !parser.isSet("ready-report")) return 3;
    if (smoke) QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    const auto directory = parser.isSet("modules") ? parser.value("modules") : app.applicationDirPath();
    StudioController studio(directory,nullptr,{},parser.isSet("confirm-midi"),parser.isSet("confirm-lan"));
    QQmlApplicationEngine view;
    view.rootContext()->setContextProperty("studio", &studio);
    view.rootContext()->setContextProperty("studioFontFamily", app.font().family());
    QObject::connect(&view, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    view.load(QUrl("qrc:/qml/Main.qml"));
    if (view.rootObjects().isEmpty()) return 2;
    if (parser.isSet("window-size")) {
        const auto dimensions = parser.value("window-size").split('x');
        if (dimensions.size() != 2 || dimensions[0].toInt() < 1000 || dimensions[1].toInt() < 650) return 3;
        if (auto* window = qobject_cast<QQuickWindow*>(view.rootObjects().first())) window->resize(dimensions[0].toInt(), dimensions[1].toInt());
    }
    if (parser.isSet("ready-report")) QTimer::singleShot(300, &app, [&] {
        auto* window = qobject_cast<QQuickWindow*>(view.rootObjects().first());
        const bool ready = window && window->isVisible() && !studio.running();
        QJsonObject report{{"status", ready ? "ready" : "failed"}, {"audio_started", studio.running()},
                           {"normal_window", window && window->isVisible() && QGuiApplication::platformName() != "offscreen" && QGuiApplication::platformName() != "minimal"},
                           {"platform", QGuiApplication::platformName()}, {"patch_ready", studio.ready()}, {"font_family", app.font().family()}};
        const auto path = QFileInfo(parser.value("ready-report")).absoluteFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile file(path);
        const auto bytes = QJsonDocument(report).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit() || !ready)
            QCoreApplication::exit(3);
        else if (parser.isSet("startup-check")) QTimer::singleShot(150, &app, [] { QCoreApplication::exit(0); });
    });
    if (smoke) QTimer::singleShot(900, &app, [&] {
        QJsonObject report{{"scope", "native Qt Quick + native engine / offline only"}};
        bool success = studio.smokeCheck(report);
        auto* window = qobject_cast<QQuickWindow*>(view.rootObjects().first());
        if (!window) success = false;
        if (window) {
            report["logical_width"] = window->width(); report["logical_height"] = window->height();
            report["visible_modules"] = window->property("visibleModuleCount").toInt();
            report["visible_cables"] = window->property("visibleCableCount").toInt();
            success = success && report["visible_modules"].toInt() == 4 && report["visible_cables"].toInt() == 3;
            if (parser.isSet("screenshot")) { const auto path = QFileInfo(parser.value("screenshot")).absoluteFilePath(); QDir().mkpath(QFileInfo(path).absolutePath()); const bool saved = window->grabWindow().save(path); report["screenshot"] = path; report["screenshot_saved"] = saved; success = success && saved; }
        }
        report["status"] = success ? "passed" : "failed";
        if (parser.isSet("report")) { const auto path = parser.value("report"); QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)); else success = false; }
        QCoreApplication::exit(success ? 0 : 1);
    });
    return app.exec();
}
