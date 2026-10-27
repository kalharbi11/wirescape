#include "studio_controller.h"
#include "document_reader.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QStringList>
#include <iostream>
#include <stdexcept>
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static QByteArray read(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "read"); return f.readAll(); }
static void write(const QString& path, const QByteArray& bytes) {
    QFile f(path); check(f.open(QIODevice::WriteOnly), "open fixture");
    check(f.write(bytes) == bytes.size(), "write fixture");
    f.close(); check(f.error() == QFileDevice::NoError, "close fixture");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        check(argc == 3, "fixture and example path arguments"); QTemporaryDir temp(app.applicationDirPath()+"/controller-test-XXXXXX"); check(temp.isValid(), "temporary folder");
        for (const auto& file : QDir(app.applicationDirPath()).entryInfoList({"ws_*.dll", "libws_*.so", "libws_*.dylib"}, QDir::Files))
            check(QFile::copy(file.absoluteFilePath(), temp.filePath(file.fileName())), "copy stock module");
        check(QFile::copy(QString::fromLocal8Bit(argv[1]), temp.filePath(QFileInfo(QString::fromLocal8Bit(argv[1])).fileName())), "copy test-only module");
        bool failCommit = false;
        StudioController studio(temp.path(), nullptr, [&](const QString& path, const QByteArray& bytes, QString& error) {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) { error = file.errorString(); return false; }
            if (failCommit) { file.cancelWriting(); error = "Injected commit failure"; return false; }
            return file.commit();
        });
        const auto url = QUrl::fromLocalFile(temp.filePath("patch.json"));
        check(studio.ready() && studio.savePatch(url), "baseline saved"); const auto before = read(url.toLocalFile());
        check(studio.setParameter(1, 0, 550), "edit accepted"); const auto accepted = studio.modules();
        const auto acceptedConnections = studio.connections();
        const auto acceptedName = studio.patchName();
        const auto acceptedOutput = studio.outputNode();
        const auto rejectionLeavesPatch = [&](const QString& filename, const QByteArray& bytes, const QByteArray& diagnostic) {
            const auto path = temp.filePath(filename);
            write(path, bytes);
            check(!studio.loadPatch(QUrl::fromLocalFile(path)), "unsafe document rejected");
            check(studio.errorText().toUtf8().contains(diagnostic), "specific open diagnostic");
            check(studio.modules() == accepted && studio.connections() == acceptedConnections &&
                  studio.patchName() == acceptedName && studio.outputNode() == acceptedOutput &&
                  studio.dirty() && studio.ready() && !studio.running(), "failed open preserves accepted patch and audio-off state");
            check(read(path) == bytes, "failed open preserves exact source bytes");
        };
        failCommit = true; check(!studio.savePatch(url) && studio.dirty(), "commit failure preserves dirty state");
        check(read(url.toLocalFile()) == before && studio.modules() == accepted, "commit failure preserves old bytes and edits");
        auto candidate = QJsonDocument::fromJson(before).object(); auto nodes = candidate["modules"].toArray();
        for (int i=0; i<nodes.size(); ++i) { auto n = nodes[i].toObject(); if (n["id"].toInt() == 3) { n["type"] = "test.prepare_failure"; nodes[i] = n; } }
        candidate["modules"] = nodes;
        rejectionLeavesPatch("failure.json", QJsonDocument(candidate).toJson(), "prepare");
        const QByteArray legacy = R"({"format":"wirescape.patch","version":3,"modules":[{"id":"voice","type":"chaos","sequence":{"notes":[60]},"module_state":{"opaque":"keep"}}],"connections":[]})";
        rejectionLeavesPatch("legacy_v3.json", legacy, "requires migration");
        check(studio.errorText().contains("not supported for execution"), "legacy execution diagnostic");
        auto future = QJsonDocument::fromJson(before).object(); future["version"] = 8;
        rejectionLeavesPatch("future.json", QJsonDocument(future).toJson(), "Unsupported document version");
        rejectionLeavesPatch("malformed.json", QByteArray("{\"format\": "), "Invalid JSON");
        rejectionLeavesPatch("oversized.json", QByteArray(wirescape::document::MaximumDocumentBytes + 1, 'x'), "2 MiB");
        failCommit = false;
        check(studio.loadPatch(url), "valid native v1 loads");
        check(studio.ready() && !studio.dirty() && !studio.running(), "valid native v1 ready with audio off");
        auto unresolved = QJsonDocument::fromJson(before).object();
        auto unresolvedNodes = unresolved["modules"].toArray();
        auto unknown = unresolvedNodes[0].toObject(); unknown["type"] = "future.unknown";
        unknown["values"] = QJsonObject{{"opaque", QJsonArray{1, "keep"}}};
        unresolvedNodes[0] = unknown; unresolved["modules"] = unresolvedNodes;
        const auto unknownPath = temp.filePath("unknown.json");
        write(unknownPath, QJsonDocument(unresolved).toJson());
        check(studio.loadPatch(QUrl::fromLocalFile(unknownPath)), "native patch with missing module loads");
        check(!studio.ready() && !studio.running() && !studio.dirty(), "missing module stays editable with audio off");
        check(studio.modules()[0].toMap().value("missing").toBool(), "missing module shown");
        check(studio.connections() == acceptedConnections, "unresolved cables preserved");
        check(studio.savePatch(QUrl::fromLocalFile(temp.filePath("unknown_roundtrip.json"))), "save unresolved patch");
        check(QJsonDocument::fromJson(read(temp.filePath("unknown_roundtrip.json"))).object() == unresolved,
              "unknown module payload and cables survive roundtrip");
        check(studio.saveDiagnostics(QUrl::fromLocalFile(temp.filePath("session.json"))), "offline diagnostics save");
        check(!QJsonDocument::fromJson(read(temp.filePath("session.json"))).object()["running"].toBool(), "diagnostic export never starts audio");
        // Exercise all six returned modules through the actual app controller:
        // typed wiring, accepted parameters, persistence and offline rendering.
        studio.resetPatch();
        const QStringList additions{"noise", "clock", "function_envelope", "slew", "vcf", "ladder"};
        for (const auto& name : additions) {
            const QString type = "essentials." + name;
            bool listed = false;
            for (const auto& entry : studio.palette()) if (entry.toMap()["id"].toString() == type) listed = true;
            check(listed, "returned module appears in app palette");
            const auto count = studio.modules().size();
            studio.addModule(type);
            check(studio.modules().size() == count + 1, "app adds returned module");
        }
        auto node = [&](const QString& type) {
            for (const auto& entry : studio.modules()) {
                const auto value = entry.toMap();
                if (value["moduleId"].toString() == "essentials." + type) return value["id"].toInt();
            }
            throw std::runtime_error("missing integration node");
        };
        while (!studio.connections().isEmpty()) studio.disconnectCable(0);
        const auto cable = [&](const QString& from, const QString& port, const QString& to, const QString& input) {
            const auto count = studio.connections().size();
            studio.connectPorts(node(from), port, node(to), input);
            check(studio.connections().size() == count + 1, "typed integration connection accepted");
        };
        cable("noise", "audio", "vcf", "audio");
        cable("vcf", "low24", "ladder", "audio");
        cable("ladder", "audio", "vca", "audio");
        cable("clock", "clock", "function_envelope", "gate");
        cable("function_envelope", "envelope", "slew", "control");
        cable("slew", "control", "vca", "gain_cv");
        cable("vca", "audio", "output", "audio");
        check(studio.setParameter(node("clock"), 0, 137) &&
              studio.setParameter(node("vcf"), 1, .7) &&
              studio.setParameter(node("ladder"), 2, .5), "returned parameters accepted");
        check(studio.ready() && !studio.running(), "combined graph ready without device");
        const auto combined = QUrl::fromLocalFile(temp.filePath("six_essentials.json"));
        check(studio.savePatch(combined), "save combined Essentials graph");
        const auto savedModules = studio.modules(), savedCables = studio.connections();
        const auto wav = QUrl::fromLocalFile(temp.filePath("six_before.wav"));
        studio.renderDemo(wav);
        const auto firstWav = read(wav.toLocalFile());
        check(firstWav.size() == 44 + 192000 * 2, "complete combined offline WAV");
        bool nonzero = false;
        for (qsizetype i = 44; i + 1 < firstWav.size(); i += 2)
            if (firstWav[i] != 0 || firstWav[i + 1] != 0) nonzero = true;
        check(nonzero, "combined graph generates nonzero offline audio");
        studio.resetPatch();
        check(studio.loadPatch(combined) && studio.ready() && !studio.running(), "reload combined graph");
        check(studio.modules() == savedModules && studio.connections() == savedCables,
              "all returned identities, values and cables persist");
        const auto reloadedWav = QUrl::fromLocalFile(temp.filePath("six_after.wav"));
        studio.renderDemo(reloadedWav);
        check(read(reloadedWav.toLocalFile()) == firstWav, "reload preserves deterministic offline rendering");
        const auto demo = QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2]));
        check(studio.loadPatch(demo) && studio.ready() && !studio.running(), "shipped demo loads with audio off");
        check(studio.modules().size() == 8 && studio.connections().size() == 7, "shipped demo graph shape");
        const auto demoWav = QUrl::fromLocalFile(temp.filePath("shipped_demo.wav"));
        studio.renderDemo(demoWav);
        const auto demoBytes = read(demoWav.toLocalFile());
        check(demoBytes.size() == 44 + 192000 * 2, "shipped demo offline render completes");
        bool demoSignal = false;
        for (qsizetype i = 44; i + 1 < demoBytes.size(); i += 2)
            if (demoBytes[i] != 0 || demoBytes[i + 1] != 0) demoSignal = true;
        check(demoSignal, "shipped demo produces nonzero offline signal");
        std::cout << "PASS controller document dispatch, transactional failures, unknown-module preservation, and offline diagnostics\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
