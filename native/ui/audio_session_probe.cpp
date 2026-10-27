#include "studio_controller.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace {
bool writeReport(const QString& path, const QJsonObject& report, QString& error) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

QJsonObject jsonMap(const QVariantMap& map) {
    return QJsonObject::fromVariantMap(map);
}
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wirescape-audio-session-probe"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Manual, explicitly authorized audio-device session probe."));
    parser.addHelpOption();
    const QCommandLineOption confirmOption(QStringLiteral("confirm-audio"), QStringLiteral("Authorize opening an audio device stream."));
    const QCommandLineOption modulesOption(QStringLiteral("modules"), QStringLiteral("Native module directory."), QStringLiteral("DIR"));
    const QCommandLineOption patchOption(QStringLiteral("patch"), QStringLiteral("Patch file to load."), QStringLiteral("FILE"));
    const QCommandLineOption reportOption(QStringLiteral("report"), QStringLiteral("JSON report output path."), QStringLiteral("FILE"));
    const QCommandLineOption durationOption(QStringLiteral("duration"), QStringLiteral("Session duration in seconds (1-10, default 3)."), QStringLiteral("SECONDS"), QStringLiteral("3"));
    parser.addOption(confirmOption);
    parser.addOption(modulesOption);
    parser.addOption(patchOption);
    parser.addOption(reportOption);
    parser.addOption(durationOption);
    parser.process(app);

    const QString modulesPath = QFileInfo(parser.value(modulesOption)).absoluteFilePath();
    const QString patchPath = QFileInfo(parser.value(patchOption)).absoluteFilePath();
    const QString reportPath = QFileInfo(parser.value(reportOption)).absoluteFilePath();
    bool durationOk = false;
    const int durationSeconds = parser.value(durationOption).toInt(&durationOk);
    const QFileInfo reportInfo(reportPath);
    if (!parser.isSet(confirmOption) || parser.value(modulesOption).isEmpty() ||
        parser.value(patchOption).isEmpty() || parser.value(reportOption).isEmpty() ||
        !durationOk || durationSeconds < 1 || durationSeconds > 10 ||
        !QFileInfo(modulesPath).isDir() || !QFileInfo(patchPath).isFile() ||
        !reportInfo.dir().exists() || reportInfo.isDir() || reportPath == patchPath) {
        parser.showHelp(2);
    }

    QJsonObject report{{"probe", "manual_audio_device_session"},
                       {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                       {"scope", "Actual audio-device stream session only; does not assess listening quality or physical capability."},
                       {"authorized_audio", true}, {"duration_seconds", durationSeconds},
                       {"modules_directory", modulesPath}, {"patch_file", patchPath},
                       {"report_file", reportPath}, {"success", false}};
    int exitCode = 1;
    auto finish = [&]() {
        QString writeError;
        if (!writeReport(reportPath, report, writeError)) {
            QTextStream(stderr) << "Could not write probe report: " << writeError << Qt::endl;
            exitCode = 2;
        }
    };

    auto* controller = new StudioController(modulesPath, &app);
    report["patch_loaded"] = controller->loadPatch(QUrl::fromLocalFile(patchPath));
    report["controller_error_after_load"] = controller->errorText();
    if (!report.value("patch_loaded").toBool()) {
        report["failure_stage"] = "load_patch";
        report["diagnostics"] = jsonMap(controller->diagnostics());
        finish();
        return exitCode;
    }

    int noiseNode = -1;
    for (const auto& item : controller->modules()) {
        const auto module = item.toMap();
        if (module.value("moduleId").toString() == QStringLiteral("essentials.noise")) {
            noiseNode = module.value("id").toInt();
            break;
        }
    }
    report["noise_level_lowered_for_probe"] = noiseNode >= 0 && controller->setParameter(noiseNode, 0, 0.05);
    if (noiseNode < 0 || !report.value("noise_level_lowered_for_probe").toBool()) {
        report["failure_stage"] = "prepare_probe_patch";
        report["controller_error"] = controller->errorText();
        report["diagnostics"] = jsonMap(controller->diagnostics());
        finish();
        return exitCode;
    }

    controller->refreshDevices();
    report["device_refresh_error"] = controller->errorText();
    if (!controller->errorText().isEmpty()) {
        report["failure_stage"] = "refresh_devices";
        report["diagnostics"] = jsonMap(controller->diagnostics());
        finish();
        return exitCode;
    }
    controller->startAudio();
    report["start_error"] = controller->errorText();
    report["start_diagnostics"] = jsonMap(controller->diagnostics());
    if (!controller->running()) {
        controller->stopAudio();
        report["failure_stage"] = "start_audio";
        report["diagnostics"] = jsonMap(controller->diagnostics());
        finish();
        return exitCode;
    }

    double maximumPeak = 0;
    QTimer peakSampler;
    QObject::connect(&peakSampler, &QTimer::timeout, &app, [&]() { maximumPeak = qMax(maximumPeak, controller->peak()); });
    peakSampler.start(100);
    QTimer::singleShot(durationSeconds * 1000, &app, [&, controller]() {
        maximumPeak = qMax(maximumPeak, controller->peak());
        report["maximum_observed_output_peak"] = maximumPeak;
        report["session_diagnostics"] = jsonMap(controller->diagnostics());
        report["observed_running"] = controller->running();
        controller->stopAudio();
        const auto finalDiagnostics = jsonMap(controller->diagnostics());
        report["final_diagnostics"] = finalDiagnostics;
        const auto d = finalDiagnostics;
        const bool clean = !d.value("running").toBool() && !d.value("cleanup_pending").toBool() &&
            d.value("callback_faults").toVariant().toULongLong() == 0 && d.value("underruns").toVariant().toULongLong() == 0 &&
            d.value("overload_blocks").toVariant().toULongLong() == 0;
        const bool rendered = d.value("rendered_blocks").toVariant().toULongLong() > 0;
        const bool passed = report.value("observed_running").toBool() && clean && rendered && maximumPeak > 0;
        report["success"] = passed;
        report["failure_stage"] = passed ? QStringLiteral("complete") : QStringLiteral("session_or_cleanup");
        report["acceptance"] = QJsonObject{{"running_observed", report.value("observed_running")},
                                          {"stopped_cleanly", !d.value("running").toBool() && !d.value("cleanup_pending").toBool()},
                                          {"callback_faults_zero", d.value("callback_faults").toVariant().toULongLong() == 0},
                                          {"underruns_zero", d.value("underruns").toVariant().toULongLong() == 0},
                                          {"overload_blocks_zero", d.value("overload_blocks").toVariant().toULongLong() == 0},
                                          {"rendered_blocks_positive", rendered}};
        exitCode = passed ? 0 : 1;
        finish();
        app.exit(exitCode);
    });
    return app.exec();
}
