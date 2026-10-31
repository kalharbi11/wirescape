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
        check(QJsonDocument::fromJson(before).object()["version"] == 2, "app saves canonical native v2");
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
        for (int i=0; i<nodes.size(); ++i) { auto n = nodes[i].toObject(); if (n["id"].toInt() == 3) { n["module"] = "test.prepare_failure"; n["parameters"] = QJsonObject{{"0", .7}}; nodes[i] = n; } }
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
        check(studio.loadPatch(url), "saved native v2 loads");
        check(studio.ready() && !studio.dirty() && !studio.running(), "saved native v2 ready with audio off");
        const auto resolvedConnections = studio.connections();
        auto unresolved = QJsonDocument::fromJson(before).object();
        auto unresolvedNodes = unresolved["modules"].toArray();
        auto unknown = unresolvedNodes[0].toObject(); unknown["module"] = "future.unknown";
        unknown["parameters"] = QJsonObject{};
        check(!unknown.contains("inactive"), "hand-written unknown v2 has no inactive annotation");
        unknown["preserved"] = QJsonObject{{"type", "future.unknown"}, {"values", QJsonObject{{"opaque", QJsonArray{1, "keep"}}}}};
        unresolvedNodes[0] = unknown; unresolved["modules"] = unresolvedNodes;
        const auto unknownPath = temp.filePath("unknown.json");
        write(unknownPath, QJsonDocument(unresolved).toJson());
        check(studio.loadPatch(QUrl::fromLocalFile(unknownPath)), "native patch with missing module loads");
        check(!studio.ready() && !studio.running() && !studio.dirty(), "missing module stays editable with audio off");
        check(studio.modules()[0].toMap().value("missing").toBool(), "missing module shown");
        check(studio.connections() == resolvedConnections, "unresolved v2 cables preserved");
        check(studio.savePatch(QUrl::fromLocalFile(temp.filePath("unknown_roundtrip.json"))), "save unresolved patch");
        unknown["inactive"] = true;
        unresolvedNodes[0] = unknown;
        unresolved["modules"] = unresolvedNodes;
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
        // No backend is authorized: native event metadata must survive editable v2 loading.
        StudioController events(temp.path());
        auto eventPatch = QJsonDocument::fromJson(before).object();
        QJsonArray eventNodes;
        const QStringList moduleTypes{"midi.in", "midi.to_cv", "essentials.output", "essentials.vco"};
        for (int i = 0; i < moduleTypes.size(); ++i)
            eventNodes.append(QJsonObject{{"id", i + 1},
                                          {"module", moduleTypes[i]},
                                          {"version", 1},
                                          {"parameters", QJsonObject{}},
                                          {"x", i * 250},
                                          {"y", 100}});
        eventPatch["modules"] = eventNodes;
        const QJsonObject midiEdge{{"kind", "event"},
                                   {"from", QJsonObject{{"module", 1}, {"port", "midi"}}},
                                   {"to", QJsonObject{{"module", 2}, {"port", "midi"}}}};
        eventPatch["connections"] =
            QJsonArray{midiEdge, QJsonObject{{"kind", "signal"},
                                             {"from", QJsonObject{{"module", 4}, {"port", "audio"}}},
                                             {"to", QJsonObject{{"module", 3}, {"port", "audio"}}}}};
        eventPatch["outputs"] = QJsonArray{QJsonObject{{"module", 3}, {"port", "audio"}, {"bus_channel", 0}}};
        eventPatch.remove("outputNode");
        const auto eventPath = temp.filePath("events.json");
        write(eventPath, QJsonDocument(eventPatch).toJson());
        check(events.loadPatch(QUrl::fromLocalFile(eventPath)) && events.ready() && !events.running(),
              "v2 MIDI event cable loads without a backend");
        const auto eventModules = events.modules();
        check(eventModules[0].toMap()["outputs"].toList()[0].toMap()["kind"] == "event" &&
                  eventModules[1].toMap()["inputs"].toList()[0].toMap()["kind"] == "event",
              "native event ports are exposed as event, not control");
        const auto savedEvents = QUrl::fromLocalFile(temp.filePath("events-saved.json"));
        check(events.savePatch(savedEvents), "save event patch");
        check(QJsonDocument::fromJson(read(savedEvents.toLocalFile())).object()["connections"].toArray()[0] == midiEdge,
              "event kind and endpoints survive editor save");
        check(events.loadPatch(savedEvents), "reload saved event patch");
        const auto acceptedEvents = events.connections();
        events.connectPorts(1, "midi", 3, "audio");
        check(events.connections() == acceptedEvents && !events.errorText().isEmpty(),
              "event-to-signal edit rejected transactionally");
        events.disconnectCable(0);
        events.connectPorts(1, "midi", 2, "midi");
        check(events.connections().size() == 2 && events.connections().last().toMap()["kind"] == "event",
              "interactive event reconnect retains event kind");
        check(events.savePatch(savedEvents) && events.loadPatch(savedEvents), "reconnected event saves and reloads");
        auto mixed = eventPatch;
        auto mixedEdges = mixed["connections"].toArray();
        auto wrongKind = midiEdge;
        wrongKind["kind"] = "signal";
        mixedEdges[0] = wrongKind;
        mixed["connections"] = mixedEdges;
        const auto mixedPath = temp.filePath("mixed-kind.json");
        write(mixedPath, QJsonDocument(mixed).toJson());
        const auto beforeRejectedLoad = events.connections();
        check(!events.loadPatch(QUrl::fromLocalFile(mixedPath)) && events.connections() == beforeRejectedLoad,
              "signal-labelled event cable load rejected without replacing accepted graph");
        std::cout << "PASS controller document dispatch, transactional failures, unknown-module preservation, and offline diagnostics\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
