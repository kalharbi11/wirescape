#include "studio_controller.h"
#include "document_reader.h"
#include "wirescape/output_policy.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QMap>
#include <QDateTime>
#include <functional>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {
QJsonObject baseDocument() {
    QJsonArray nodes;
    nodes.append(QJsonObject{{"id", 1}, {"type", "essentials.vco"}, {"x", 80}, {"y", 100}, {"values", QJsonObject{{"0", 220}}}});
    nodes.append(QJsonObject{{"id", 2}, {"type", "essentials.adsr"}, {"x", 400}, {"y", 210}, {"values", QJsonObject{{"0", .025}, {"1", .18}, {"2", .65}, {"3", .5}}}});
    nodes.append(QJsonObject{{"id", 3}, {"type", "essentials.vca"}, {"x", 740}, {"y", 100}, {"values", QJsonObject{{"0", .7}}}});
    nodes.append(QJsonObject{{"id", 4}, {"type", "essentials.output"}, {"x", 1060}, {"y", 230}, {"values", QJsonObject{}}});
    QJsonArray cables;
    cables.append(QJsonObject{{"fromNode", 1}, {"fromPort", "audio"}, {"toNode", 3}, {"toPort", "audio"}, {"kind", "audio"}});
    cables.append(QJsonObject{{"fromNode", 2}, {"fromPort", "envelope"}, {"toNode", 3}, {"toPort", "gain_cv"}, {"kind", "control"}});
    cables.append(QJsonObject{{"fromNode", 3}, {"fromPort", "audio"}, {"toNode", 4}, {"toPort", "audio"}, {"kind", "audio"}});
    return {{"format", "wirescape.native.patch"}, {"version", 1}, {"name", "Essential voice"}, {"outputNode", 4}, {"modules", nodes}, {"connections", cables}};
}
QString text(const char* value) { return value ? QString::fromUtf8(value) : QString(); }
QString display(double value, const QString& unit) {
    if (unit == "s") return value < 1 ? QString::number(value * 1000, 'f', 0) + " ms" : QString::number(value, 'f', 2) + " s";
    if (unit == "Hz") return value >= 1000 ? QString::number(value / 1000, 'f', 2) + " kHz" : QString::number(value, 'f', 1) + " Hz";
    return QString::number(value, 'f', 2) + (unit.isEmpty() ? "" : " " + unit);
}
}

StudioController::StudioController(QString directory, QObject* parent, PatchWriter writer) : QObject(parent), directory_(std::move(directory)), patchWriter_(std::move(writer)) {
    if (!patchWriter_) patchWriter_ = [](const QString& path, const QByteArray& bytes, QString& error) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            error = file.errorString(); return false;
        }
        return true;
    };
    ws_engine_config config{sizeof(config), 48000, 512, 64, 2048};
    catalog_.reset(app_engine_create(&config));
    if (catalog_) {
#ifdef Q_OS_WIN
        const QStringList filters{"ws_*.dll"};
#elif defined(Q_OS_MACOS)
        const QStringList filters{"libws_*.dylib"};
#else
        const QStringList filters{"libws_*.so"};
#endif
        for (const auto& file : QDir(directory_).entryInfoList(filters, QDir::Files, QDir::Name)) {
            const auto path = file.absoluteFilePath().toUtf8();
            if (app_engine_load_module(catalog_.get(), path.constData()) != WS_OK) error_ = text(app_engine_error(catalog_.get()));
        }
    }
    resetPatch();
    timer_.setInterval(50);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (audio_.poll()) {
            if (!audio_.running() && !audio_.engineInUse() && engine_) { lastSessionMeter_.struct_size = sizeof(lastSessionMeter_); app_engine_get_meter(engine_.get(), &lastSessionMeter_); app_engine_reset(engine_.get()); lastFrame_ = 0; peak_ = 0; }
            setError(audio_.error());
        }
        if (engine_) { ws_meter meter{}; meter.struct_size = sizeof(meter); app_engine_get_meter(engine_.get(), &meter); peak_ = running() ? meter.peak * WS_OUTPUT_GAIN : 0; }
        emit metersChanged();

    });
    timer_.start();
}

StudioController::~StudioController() { if (!audio_.stop()) {
    // A backend that cannot establish quiescence still owns the engine. Its
    // detached callback context is retained by AudioDevice; retain descriptors too.
    engine_.release(); catalog_.release();
} }

const ws_module_descriptor* StudioController::descriptor(const QString& id) const {
    if (!catalog_) return nullptr;
    for (uint32_t i = 0; i < app_engine_module_count(catalog_.get()); ++i) {
        const auto* d = app_engine_module(catalog_.get(), i);
        if (text(d->id) == id) return d;
    }
    return nullptr;
}

const ws_module_descriptor* StudioController::nodeDescriptor(int id) const {
    for (const auto& value : document_.value("modules").toArray()) { const auto n = value.toObject(); if (n.value("id").toInt() == id) return descriptor(n.value("type").toString()); }
    return nullptr;
}

QVariantList StudioController::palette() const {
    QVariantList result;
    if (!catalog_) return result;
    for (uint32_t i = 0; i < app_engine_module_count(catalog_.get()); ++i) { const auto* d = app_engine_module(catalog_.get(), i); result.append(QVariantMap{{"id", text(d->id)}, {"name", text(d->name)}, {"description", "Essentials / processor"}}); }
    return result;
}

QVariantList StudioController::modules() const {
    QVariantList result;
    for (const auto& item : document_.value("modules").toArray()) {
        const auto node = item.toObject(); const auto* d = descriptor(node.value("type").toString());
        QVariantList inputs, outputs, parameters;
        if (d) {
            for (uint32_t i = 0; i < d->input_count; ++i) inputs.append(QVariantMap{{"id", text(d->inputs[i].id)}, {"label", text(d->inputs[i].name)}, {"kind", d->inputs[i].kind == WS_AUDIO ? "audio" : "control"}});
            for (uint32_t i = 0; i < d->output_count; ++i) outputs.append(QVariantMap{{"id", text(d->outputs[i].id)}, {"label", text(d->outputs[i].name)}, {"kind", d->outputs[i].kind == WS_AUDIO ? "audio" : "control"}});
            const auto values = node.value("values").toObject();
            for (uint32_t i = 0; i < d->parameter_count; ++i) { const auto& p = d->parameters[i]; const double v = values.value(QString::number(p.id)).toDouble(p.initial); parameters.append(QVariantMap{{"id", p.id}, {"name", text(p.name)}, {"minimum", p.minimum}, {"maximum", p.maximum}, {"value", v}, {"unit", text(p.unit)}, {"display", display(v, text(p.unit))}}); }
        }
        if (!d) {
            // Preserve and display unresolved cable endpoints so recovery remains editable.
            QSet<QString> seenInputs, seenOutputs;
            for (const auto& saved : document_.value("connections").toArray()) {
                const auto cable = saved.toObject(); const int id = node.value("id").toInt();
                const auto input = cable.value("toPort").toString(), output = cable.value("fromPort").toString();
                if (cable.value("toNode").toInt() == id && !seenInputs.contains(input)) { inputs.append(QVariantMap{{"id", input}, {"label", input}, {"kind", cable.value("kind").toString()}}); seenInputs.insert(input); }
                if (cable.value("fromNode").toInt() == id && !seenOutputs.contains(output)) { outputs.append(QVariantMap{{"id", output}, {"label", output}, {"kind", cable.value("kind").toString()}}); seenOutputs.insert(output); }
            }
        }
        result.append(QVariantMap{{"id", node.value("id").toInt()}, {"moduleId", node.value("type").toString()}, {"title", d ? text(d->name) : node.value("type").toString()}, {"description", d ? "ESSENTIALS / 01" : "UNAVAILABLE MODULE"}, {"x", node.value("x").toDouble()}, {"y", node.value("y").toDouble()}, {"missing", d == nullptr}, {"inputs", inputs}, {"outputs", outputs}, {"parameters", parameters}});
    }
    return result;
}

QVariantList StudioController::connections() const { return document_.value("connections").toArray().toVariantList(); }
QString StudioController::statusText() const { return running() ? "AUDIO RUNNING" : ready_ ? "READY / AUDIO OFF" : "PATCH NEEDS ATTENTION"; }
void StudioController::setError(QString value) { error_ = std::move(value); emit statusChanged(); }

int StudioController::portIndex(const ws_port_descriptor* ports, uint32_t count, const QString& id) { for (uint32_t i = 0; i < count; ++i) if (text(ports[i].id) == id) return static_cast<int>(i); return -1; }


bool StudioController::validateDocument(const QJsonObject& doc, QString& error) const {
    auto fail = [&](const QString& message) { error = message; return false; };
    auto integer = [](const QJsonValue& v, int minimum) { const double n = v.toDouble(-1); return v.isDouble() && std::isfinite(n) && n >= minimum && n <= std::numeric_limits<int>::max() && n == std::floor(n); };
    if (doc.value("format") != "wirescape.native.patch" || !integer(doc.value("version"), 1) || doc.value("version").toInt() != 1)
        return fail("This build opens native v1 patches only.");
    if (!doc.value("name").isString() || !doc.value("modules").isArray() || !doc.value("connections").isArray() || !integer(doc.value("outputNode"), 0)) return fail("Patch fields have invalid types.");
    const auto nodes = doc.value("modules").toArray(), cables = doc.value("connections").toArray();
    if (nodes.size() > 64 || cables.size() > 128) return fail("Patch exceeds the graph size limit.");
    QMap<int, const ws_module_descriptor*> types;
    for (const auto& item : nodes) {
        if (!item.isObject()) return fail("Each module must be an object.");
        const auto n = item.toObject();
        if (!integer(n.value("id"), 1) || types.contains(n.value("id").toInt()) || !n.value("type").isString() || n.value("type").toString().isEmpty() || !n.value("x").isDouble() || !n.value("y").isDouble() || !std::isfinite(n.value("x").toDouble()) || !std::isfinite(n.value("y").toDouble()) || !n.value("values").isObject()) return fail("A module has invalid identity, coordinates or parameter data.");
        const auto* d = descriptor(n.value("type").toString()); types.insert(n.value("id").toInt(), d);
        const auto values = n.value("values").toObject();
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (!d) continue; // Unknown processor payload is preserved without interpreting its parameter IDs or value schema.
            if (!it.value().isDouble() || !std::isfinite(it.value().toDouble())) return fail("Saved parameter values must be finite numbers.");
            const ws_parameter_descriptor* p = nullptr;
            for (uint32_t i = 0; i < d->parameter_count; ++i) if (it.key() == QString::number(d->parameters[i].id)) p = &d->parameters[i];
            if (!p || it.value().toDouble() < p->minimum || it.value().toDouble() > p->maximum) return fail("A saved parameter is unknown or outside its declared range.");
        }
    }
    const int output = doc.value("outputNode").toInt();
    if (output && (!types.contains(output) || (types[output] && (!types[output]->output_count || types[output]->outputs[0].kind != WS_AUDIO)))) return fail("The selected output is not a valid audio destination.");
    QSet<QString> inputs; QMap<int, QList<int>> edges;
    for (const auto& item : cables) {
        if (!item.isObject()) return fail("Each cable must be an object.");
        const auto c = item.toObject();
        if (!integer(c.value("fromNode"), 1) || !integer(c.value("toNode"), 1) || !c.value("fromPort").isString() || c.value("fromPort").toString().isEmpty() || !c.value("toPort").isString() || c.value("toPort").toString().isEmpty() || (c.value("kind") != "audio" && c.value("kind") != "control")) return fail("A cable has invalid field types.");
        const int a = c.value("fromNode").toInt(), b = c.value("toNode").toInt();
        if (!types.contains(a) || !types.contains(b) || a == b) return fail("A cable references an invalid module.");
        const QString input = QString::number(b) + ":" + c.value("toPort").toString();
        if (inputs.contains(input)) return fail("An input can have only one cable.");
        inputs.insert(input); edges[a].append(b);
        const auto* source = types[a]; const auto* destination = types[b];
        const int sp = source ? portIndex(source->outputs, source->output_count, c.value("fromPort").toString()) : -1;
        const int dp = destination ? portIndex(destination->inputs, destination->input_count, c.value("toPort").toString()) : -1;
        const auto kind = c.value("kind") == "audio" ? WS_AUDIO : WS_CONTROL;
        if ((source && (sp < 0 || source->outputs[sp].kind != kind)) || (destination && (dp < 0 || destination->inputs[dp].kind != kind))) return fail("A cable has an invalid port or incompatible signal type.");
    }
    QSet<int> visiting, visited;
    std::function<bool(int)> cycle = [&](int node) { if (visiting.contains(node)) return true; if (visited.contains(node)) return false; visiting.insert(node); for (int next : edges.value(node)) if (cycle(next)) return true; visiting.remove(node); visited.insert(node); return false; };
    for (int node : types.keys()) if (cycle(node)) return fail("Feedback cycles require an explicit delay and are unsupported.");
    error.clear(); return true;
}

QString StudioController::incompleteReason(const QJsonObject& doc) const {
    for (const auto& saved : doc.value("modules").toArray()) {
        const auto type = saved.toObject().value("type").toString();
        if (!descriptor(type)) return QStringLiteral("Module %1 is unavailable. Its data and cables remain editable; audio is disabled.").arg(type);
    }
    if (!doc.value("outputNode").toInt()) return "Select an Output destination to enable audio. The patch remains editable.";
    return {};
}

StudioController::Engine StudioController::makeEngine(const QJsonObject& doc, QString& error) const {
    ws_engine_config config{sizeof(config),48000,512,64,2048};
    auto patch=wirescape::document::editor_patch(doc,catalog_.get(),documentSource_);
    return Engine(wirescape::document::build_app_patch(catalog_.get(),patch,config,error),app_engine_destroy);
}

bool StudioController::applyParameters(AppEngine* e, const QJsonObject& doc) const {
    for (const auto& value : doc.value("modules").toArray()) {
        const auto n = value.toObject(); const auto* d = descriptor(n.value("type").toString()); if (!d) return false;
        const auto values = n.value("values").toObject();
        for (uint32_t i = 0; i < d->parameter_count; ++i) { const auto& p = d->parameters[i]; const auto entry = values.value(QString::number(p.id)); if (!entry.isUndefined() && !entry.isDouble()) return false; const auto v = entry.toDouble(p.initial); if (!std::isfinite(v) || app_engine_initialize_parameter(e, static_cast<uint32_t>(n.value("id").toInt()), p.id, static_cast<float>(v)) != WS_OK) return false; }
    }
    return true;
}

bool StudioController::compileDocument() {
    if (audio_.engineInUse()) { setError("Audio cleanup is pending. Stop audio again before preparing a patch."); return false; }
    QString error;
    auto candidate = makeEngine(document_, error);
    engine_ = std::move(candidate); ready_ = bool(engine_); lastFrame_ = 0; peak_ = 0;
    setError(error); emit metersChanged(); return ready_;
}

bool StudioController::commitTopology(QJsonObject candidate) {
    if (running() || audio_.engineInUse()) { setError("Stop audio and complete device cleanup before changing modules or cables."); return false; }
    QString error;
    if (!validateDocument(candidate, error)) { setError(error); return false; }
    const auto incomplete = incompleteReason(candidate);
    auto next = incomplete.isEmpty() ? makeEngine(candidate, error) : Engine(nullptr, app_engine_destroy);
    if (!next && incomplete.isEmpty()) { setError(error); return false; }
    if (!incomplete.isEmpty()) error = incomplete;
    engine_ = std::move(next); document_ = std::move(candidate); ready_ = bool(engine_); dirty_ = true; lastFrame_ = 0;
    setError(error); emit documentChanged(); return true;
}

bool StudioController::setParameter(int node, int parameter, double value) {
    const auto* d = nodeDescriptor(node); if (!d || !std::isfinite(value)) return false;
    const ws_parameter_descriptor* p = nullptr; for (uint32_t i = 0; i < d->parameter_count; ++i) if (d->parameters[i].id == static_cast<uint32_t>(parameter)) p = &d->parameters[i];
    if (!p || value < p->minimum || value > p->maximum) return false;
    if (running() && !send(node, WS_PARAMETER, static_cast<float>(value), parameter)) return false;
    auto nodes = document_.value("modules").toArray(); for (int i = 0; i < nodes.size(); ++i) { auto n = nodes[i].toObject(); if (n.value("id").toInt() == node) { auto values = n.value("values").toObject(); values[QString::number(parameter)] = value; n["values"] = values; nodes[i] = n; break; } }
    document_["modules"] = nodes; dirty_ = true; emit parameterChanged(node, parameter, value, display(value, text(p->unit))); emit statusChanged(); return true;
}

void StudioController::moveModule(int node, double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    auto nodes = document_.value("modules").toArray(); for (int i = 0; i < nodes.size(); ++i) { auto n = nodes[i].toObject(); if (n.value("id").toInt() == node) { n["x"] = x; n["y"] = y; nodes[i] = n; } }
    document_["modules"] = nodes; dirty_ = true; emit statusChanged();
}

void StudioController::addModule(const QString& id) {
    const auto* d = descriptor(id); if (!d) return;
    auto next = document_; auto nodes = next.value("modules").toArray(); int idNumber = 1;
    QSet<int> usedIds; for (const auto& n : nodes) usedIds.insert(n.toObject().value("id").toInt());
    while (usedIds.contains(idNumber)) ++idNumber;
    QJsonObject values; for (uint32_t i = 0; i < d->parameter_count; ++i) values[QString::number(d->parameters[i].id)] = d->parameters[i].initial;
    nodes.append(QJsonObject{{"id", idNumber}, {"type", id}, {"x", 160 + (nodes.size() % 4) * 180}, {"y", 120 + (nodes.size() % 3) * 120}, {"values", values}});
    next["modules"] = nodes; if (id == "essentials.output" && !next.value("outputNode").toInt()) next["outputNode"] = idNumber; commitTopology(next);
}

void StudioController::removeModule(int id) {
    auto next = document_; QJsonArray nodes, cables;
    for (const auto& n : next.value("modules").toArray()) if (n.toObject().value("id").toInt() != id) nodes.append(n);
    for (const auto& c : next.value("connections").toArray()) if (c.toObject().value("fromNode").toInt() != id && c.toObject().value("toNode").toInt() != id) cables.append(c);
    next["modules"] = nodes; next["connections"] = cables; if (next.value("outputNode").toInt() == id) next["outputNode"] = 0; commitTopology(next);
}

void StudioController::connectPorts(int from, const QString& port, int to, const QString& input) {
    const auto* source = nodeDescriptor(from); const auto* destination = nodeDescriptor(to); if (!source || !destination) return;
    const int a = portIndex(source->outputs, source->output_count, port), b = portIndex(destination->inputs, destination->input_count, input);
    if (a < 0 || b < 0 || source->outputs[a].kind != destination->inputs[b].kind) { setError("Audio and control ports need an explicit converter."); return; }
    auto next = document_; auto cables = next.value("connections").toArray();
    cables.append(QJsonObject{{"fromNode", from}, {"fromPort", port}, {"toNode", to}, {"toPort", input}, {"kind", source->outputs[a].kind == WS_AUDIO ? "audio" : "control"}});
    next["connections"] = cables; commitTopology(next);
}

void StudioController::disconnectCable(int index) { auto next = document_; auto cables = next.value("connections").toArray(); if (index < 0 || index >= cables.size()) return; cables.removeAt(index); next["connections"] = cables; commitTopology(next); }

bool StudioController::send(int node, uint32_t type, float value, uint32_t parameter, uint32_t noteId) {
    if (!engine_) return false;
    ws_meter meter{}; meter.struct_size = sizeof(meter); app_engine_get_meter(engine_.get(), &meter);
    lastFrame_ = std::max(lastFrame_, meter.frame_position + (running() ? blockSize() : 0));
    ws_event event{sizeof(event), lastFrame_, static_cast<uint32_t>(node), type, parameter, value, 1, noteId, app_engine_generation(engine_.get())};
    const int result = app_engine_enqueue(engine_.get(), &event);
    if (result != WS_OK) {
        if (result == WS_CAPACITY && type != WS_PARAMETER) { stopAudio(); setError(QStringLiteral("Control queue is full. Playback stopped to prevent a stuck gate; restart explicitly when ready. %1").arg(audio_.error())); }
        else if (result == WS_CAPACITY) setError("Control queue is full. This change was not accepted; release and retry.");
        else if (result == WS_STATE || result == WS_BUSY) setError("The engine cannot accept controls in its current state.");
        else setError("The engine rejected an invalid control event.");
        return false;
    }
    return true;
}

void StudioController::refreshDevices() { if (audio_.hasStream()) { setError("Stop audio and complete device cleanup before refreshing outputs."); return; } if (audio_.refresh(directory_)) setError({}); else setError(audio_.error()); emit statusChanged(); }
void StudioController::setDevice(int id) { if (!running()) { device_ = id; emit statusChanged(); } }

void StudioController::startAudio() {
    if (running()) return;
    if (audio_.hasStream()) { setError("Device cleanup is pending. Stop audio again before restarting."); return; }
    if (!compileDocument()) return;
    if (!audio_.initialize(directory_) || !audio_.startRender(engine_.get(), [](void* e,float* out,uint32_t n){return app_engine_render(static_cast<AppEngine*>(e),out,n);}, device_, 48000, blockSize())) { setError(audio_.error()); return; }
    lastSessionMeter_ = {}; setError({}); emit statusChanged();
}

void StudioController::stopAudio() { if (!audio_.stop()) { setError(audio_.error()); return; } if (engine_) { lastSessionMeter_.struct_size = sizeof(lastSessionMeter_); app_engine_get_meter(engine_.get(), &lastSessionMeter_); app_engine_reset(engine_.get()); } lastFrame_ = 0; peak_ = 0; setError(audio_.error()); emit metersChanged(); }
void StudioController::setGate(bool on) { if (!running()) return; for (const auto& item : document_.value("modules").toArray()) if (!send(item.toObject().value("id").toInt(), WS_GATE, on ? 1.f : 0.f)) break; }
void StudioController::note(int pitch, bool on) { if (!running() || pitch < 0 || pitch > 127) return; for (const auto& item : document_.value("modules").toArray()) if (!send(item.toObject().value("id").toInt(), on ? WS_NOTE_ON : WS_NOTE_OFF, static_cast<float>(pitch), 0, static_cast<uint32_t>(pitch + 2))) break; }
void StudioController::panic() { if (engine_) { app_engine_panic(engine_.get()); lastFrame_ = 0; } emit statusChanged(); }
void StudioController::resetPatch() { if (audio_.hasStream()) stopAudio(); if (audio_.engineInUse()) return; documentSource_ = {}; document_ = baseDocument(); name_ = "Essential voice"; dirty_ = false; compileDocument(); emit documentChanged(); }

void StudioController::selectOutput(int node) { auto next = document_; next["outputNode"] = node; commitTopology(next); }

bool StudioController::savePatch(const QUrl& url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString(); if (path.isEmpty()) return false;
    auto patch=wirescape::document::editor_patch(document_,catalog_.get(),documentSource_);
    if(!patch.valid()){setError(patch.error);return false;}
    QString error;
    const auto bytes=QJsonDocument(patch.object).toJson(QJsonDocument::Indented);
    if(!wirescape::document::save_patch(path,patch,&error,patchWriter_)){setError(error);return false;}
    documentSource_=patch;documentSource_.source_version=2;documentSource_.original_bytes=bytes;
    name_=QFileInfo(path).completeBaseName();dirty_=false;setError({});return true;
}

QVariantMap StudioController::diagnostics() const {
    auto result = audio_.diagnostics();
    result["created_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    result["scope"] = "session counters; not an end-to-end latency measurement";
    result["patch_ready"] = ready_; result["node_count"] = modules().size();
    if (engine_) {
        ws_meter meter = lastSessionMeter_; meter.struct_size = sizeof(meter); if (running() || !meter.rendered_blocks) app_engine_get_meter(engine_.get(), &meter);
        result["overload_blocks"] = QVariant::fromValue<qulonglong>(meter.overload_blocks);
        result["overload_reason"] = meter.overload_reason == 1 ? "event_budget" : meter.overload_reason == 2 ? "graph_span_budget" : "none";
        result["queue_overflows"] = QVariant::fromValue<qulonglong>(meter.overflow_events);
        result["late_events"] = QVariant::fromValue<qulonglong>(meter.late_events);
        result["queued_events"] = meter.queued_events;
        result["rendered_blocks"] = QVariant::fromValue<qulonglong>(meter.rendered_blocks);
    }
    return result;
}
QString StudioController::diagnosticText() const {
    ws_meter meter = lastSessionMeter_; meter.struct_size = sizeof(meter);
    if (engine_ && (running() || !meter.rendered_blocks)) app_engine_get_meter(engine_.get(), &meter);
    return meter.overload_blocks || audio_.faults() ? QStringLiteral("faults %1 / overloads %2").arg(audio_.faults()).arg(meter.overload_blocks) : QString();
}
bool StudioController::saveDiagnostics(const QUrl& url) {
    const auto path = url.isLocalFile() ? url.toLocalFile() : url.toString(); if (path.isEmpty()) return false;
    QSaveFile file(path); const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(diagnostics())).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { setError(file.errorString()); return false; }
    notice_ = "Session diagnostics saved."; setError({}); return true;
}

bool StudioController::loadPatch(const QUrl& url) {
    if (running() || audio_.engineInUse()) { setError("Stop audio and complete device cleanup before loading a patch."); return false; }
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString(); QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setError(QStringLiteral("Patch could not be read: %1").arg(file.errorString())); return false; }
    if (file.size() > wirescape::document::MaximumDocumentBytes) { setError("Document exceeds the 2 MiB inspection limit; original file unchanged."); return false; }
    // QJsonDocument and the shared inspector consume UTF-8 bytes directly.
    const auto bytes = file.read(wirescape::document::MaximumDocumentBytes + 1);
    if (bytes.size() > wirescape::document::MaximumDocumentBytes) { setError("Document exceeds the 2 MiB inspection limit; original file unchanged."); return false; }
    if (file.error() != QFileDevice::NoError) { setError(QStringLiteral("Patch could not be read: %1").arg(file.errorString())); return false; }
    auto inspected = wirescape::document::inspect(bytes);
    if(QJsonDocument::fromJson(bytes).object()["version"]==2 && QJsonDocument::fromJson(bytes).object()["format"]=="wirescape.native.patch")inspected.format=wirescape::document::Format::NativeV1;
    using wirescape::document::Format;
    switch (inspected.format) {
    case Format::NativeV1: break;
    case Format::LegacyPatchV1:
    case Format::LegacyPatchV2:
    case Format::LegacyPatchV3:
    case Format::VisualStudyV4:
        setError(QStringLiteral("Recognized legacy document (%1, version %2) requires migration and is not supported for execution. Original file unchanged.")
                 .arg(inspected.object.value("format").toString()).arg(inspected.object.value("version").toInt())); return false;
    case Format::UnsupportedVersion:
    case Format::UnknownFormat:
        setError(QStringLiteral("%1 (format %2, version %3). Original file unchanged.")
                 .arg(inspected.diagnostic, inspected.object.value("format").toString()).arg(inspected.object.value("version").toInt())); return false;
    case Format::Invalid:
        setError(inspected.diagnostic + QStringLiteral(" Original file unchanged.")); return false;
    }
    const auto loaded=wirescape::document::read_patch(bytes,wirescape::document::app_catalog(catalog_.get()));
    if(!loaded.valid()){setError(loaded.error);return false;}
    const auto candidate=wirescape::document::editor_view(loaded,catalog_.get()); QString error;
    auto previousSource=documentSource_;documentSource_=loaded;
    if (!validateDocument(candidate, error)) { documentSource_=previousSource;setError(error); return false; }
    const auto incomplete = incompleteReason(candidate);
    auto next = incomplete.isEmpty() ? makeEngine(candidate, error) : Engine(nullptr, app_engine_destroy);
    if (!next && incomplete.isEmpty()) { documentSource_=previousSource;setError(error); return false; }
    if (!incomplete.isEmpty()) error = incomplete;
    document_ = candidate; engine_ = std::move(next); ready_ = bool(engine_); lastFrame_ = 0; peak_ = 0;
    name_ = QFileInfo(path).completeBaseName(); dirty_ = false; setError(error); emit documentChanged(); emit metersChanged(); return true;
}

void StudioController::renderDemo(const QUrl& url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString(); if (path.isEmpty()) return;
    if (QFileInfo::exists(path)) { setError("Choose a new WAV filename; existing files are not overwritten."); return; }
    QString error; auto e = makeEngine(document_, error); if (!e) { setError(error); return; }
    for (const auto& n : document_.value("modules").toArray()) { ws_event on{sizeof(on), 0, static_cast<uint32_t>(n.toObject().value("id").toInt()), WS_GATE, 0, 1, 1, 1, app_engine_generation(e.get())}; app_engine_enqueue(e.get(), &on); }
    for (const auto& n : document_.value("modules").toArray()) { ws_event off{sizeof(off), 96000, static_cast<uint32_t>(n.toObject().value("id").toInt()), WS_GATE, 0, 0, 1, 1, app_engine_generation(e.get())}; app_engine_enqueue(e.get(), &off); }
    const uint32_t frames = 192000; std::vector<qint16> samples(frames); float block[256];
    for (uint32_t at = 0; at < frames; at += 256) { const auto n = std::min<uint32_t>(256, frames - at); if (app_engine_render(e.get(), block, n) != WS_OK) { setError("Offline render failed."); return; } for (uint32_t i = 0; i < n; ++i) samples[at + i] = static_cast<qint16>(std::clamp(block[i] * WS_OUTPUT_GAIN, -1.f, 1.f) * 32767); }
    QSaveFile file(path); if (!file.open(QIODevice::WriteOnly)) { setError(file.errorString()); return; }
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    file.write("RIFF", 4); stream << quint32(36 + frames * 2); file.write("WAVEfmt ", 8); stream << quint32(16) << quint16(1) << quint16(1) << quint32(48000) << quint32(96000) << quint16(2) << quint16(16); file.write("data", 4); stream << quint32(frames * 2);
    for (const auto sample : samples) stream << sample;
    if (!file.commit()) { setError(file.errorString()); return; }
    notice_ = QStringLiteral("Saved test render: 4 s mono, gate 2 s, output gain 0.2. Long releases may be cut off. %1").arg(QFileInfo(path).fileName());
    setError({});
    QTimer::singleShot(8000, this, [this] { notice_.clear(); emit statusChanged(); });
}

bool StudioController::smokeCheck(QJsonObject& result) {
    result["audio_started"] = running(); result["module_count"] = modules().size(); result["cable_count"] = connections().size();
    if (!ready_ || running()) { result["error"] = error_; return false; }
    const bool parameterAccepted = setParameter(1, 0, 330);
    QString error; auto e = makeEngine(document_, error); if (!e) { result["error"] = error; return false; }
    for (const auto& n : document_.value("modules").toArray()) { ws_event on{sizeof(on), 0, static_cast<uint32_t>(n.toObject().value("id").toInt()), WS_GATE, 0, 1, 1, 1, app_engine_generation(e.get())}; if (app_engine_enqueue(e.get(), &on) != WS_OK) return false; }
    float block[256]; float peak = 0;
    for (int i = 0; i < 30; ++i) { if (app_engine_render(e.get(), block, 256) != WS_OK) return false; for (const auto v : block) { if (!std::isfinite(v)) return false; peak = std::max(peak, std::abs(v)); } }
    result["parameter_bridge"] = parameterAccepted; result["offline_peak"] = peak;
    result["engine_abi"] = static_cast<int>(app_engine_abi_version());
    return parameterAccepted && peak > .01f && peak <= 1;
}

