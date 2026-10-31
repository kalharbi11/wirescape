#include "studio_controller.h"
#include <QGuiApplication>
#include <cmath>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QDir>

#include <QProcess>

// The rejection bridge exercises QML's actual mouse/keyboard path without opening audio.
class ParameterBridge : public QObject {
    Q_OBJECT
public:
    bool reject = false;
    double accepted = 220;
    int calls = 0;
    Q_INVOKABLE bool setParameter(int node, int parameter, double value) {
        ++calls;
        if (reject) return false;
        accepted = value; emit parameterChanged(node, parameter, value, QString::number(value, 'f', 1) + " Hz"); return true;
    }
signals:
    void parameterChanged(int node, int parameter, double value, QString display);
};

static QObject* findNamed(QObject* root, const QString& name) {
    if (root->objectName() == name) return root;
    // Repeater delegates are visual children and need not be QObject descendants.
    if (auto* item = qobject_cast<QQuickItem*>(root))
        for (auto* child : item->childItems()) if (auto* found = findNamed(child, name)) return found;
    for (auto* child : root->children()) if (auto* found = findNamed(child, name)) return found;
    return nullptr;
}
static QQuickItem* findButton(QObject* root, const QString& text) {
    if (auto* item = qobject_cast<QQuickItem*>(root)) {
        if (item->property("text").toString().remove('&') == text && item->property("down").isValid()) return item;
        for (auto* child : item->childItems()) if (auto* found = findButton(child, text)) return found;
    }
    for (auto* child : root->children()) if (auto* found = findButton(child, text)) return found;
    return nullptr;
}
static StudioController::PatchWriter failureControlledWriter(bool& fail, int& attempts, const QString& testDirectory, QString* acceptedPath = nullptr) {
    return [&fail, &attempts, testDirectory, acceptedPath](const QString&, const QByteArray& bytes, QString& error) {
        ++attempts;
        // Synthetic chooser acceptance tests transactions, not filename entry.
        // Persist only in the test-owned directory, never a chooser default path.
        const auto destination = QDir(testDirectory).filePath(QStringLiteral("writer-attempt-%1.json").arg(attempts));
        if (acceptedPath) *acceptedPath = destination;
        if (fail) { error = "Injected patch commit failure"; return false; }
        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            error = file.errorString(); return false;
        }
        return true;
    };
}
class UiTests : public QObject {
    Q_OBJECT
    QString directory_ = QCoreApplication::applicationDirPath();
    QTemporaryDir temp_{QCoreApplication::applicationDirPath() + "/ui-test-XXXXXX"};
    QUrl path(const QString& name) const { return QUrl::fromLocalFile(temp_.filePath(name)); }
    QJsonObject read(const QString& name) const { QFile file(temp_.filePath(name)); if (!file.open(QIODevice::ReadOnly)) return {}; return QJsonDocument::fromJson(file.readAll()).object(); }
    void write(const QString& name, const QJsonObject& doc) const { QFile file(temp_.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); QVERIFY(file.write(QJsonDocument(doc).toJson()) > 0); }
    static QPoint center(QQuickItem* item) { return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint(); }
private slots:
    void initTestCase() { QVERIFY(temp_.isValid()); }
    void transportStartReturnsKeyboardFocus() {
        QQmlEngine engine;
        QQmlComponent fakeComponent(&engine);
        fakeComponent.setData(R"QML(
    import QtQml
    QtObject {
        property string patchName: "Fake"; property bool dirty: false
        property var modules: []; property var connections: []; property var palette: []
        property bool running: false; property bool cleanupPending: false; property bool ready: true
        property string errorText: ""; property string noticeText: ""; property string statusText: "Ready"
        property string diagnosticText: ""; property real sampleRate: 48000; property int blockSize: 256
        property real cpuLoad: 0; property int underruns: 0; property real peak: 0
        property var devices: []; property int selectedDevice: -1
        property string audioOutputStatus: "Fake output"
        property bool midiAuthorized: false; property bool midiSettingsAvailable: false
        property var midiEndpoints: []; property var midiCounters: ({received: 0, dropped: 0, malformed: 0, ports: []})
        property int noteOnCount: 0; property int noteOffCount: 0; property int lastPitch: -1
        signal statusChanged()
        function startAudio() { running = true; statusChanged() }
        function stopAudio() { running = false; statusChanged() }
        function note(pitch, on) { lastPitch = pitch; if (on) ++noteOnCount; else ++noteOffCount }
        function setGate(v) {} function panic() {}
        function savePatch(url) { return true } function resetPatch() {} function loadPatch(url) {}
        function renderDemo(url) {} function connectPorts(a,b,c,d) {} function disconnectCable(a) {}
        function addModule(id) {} function refreshDevices() {} function setDevice(id) {}
        function refreshMidi() {} function setMidiAuthorized(v) {} function setMidiEndpoint(a,b,c) {}
    }
    )QML",
                              QUrl("qrc:/qml/FakeStudio.qml"));
        QTRY_VERIFY2(!fakeComponent.isLoading(), qPrintable(fakeComponent.errorString()));
        QVERIFY2(fakeComponent.isReady(), qPrintable(fakeComponent.errorString()));
        QScopedPointer<QObject> fakeStudio(fakeComponent.create());
        QVERIFY2(fakeStudio, qPrintable(fakeComponent.errorString()));
        engine.rootContext()->setContextProperty("studio", fakeStudio.data());

        QQmlComponent mainComponent(&engine, QUrl("qrc:/qml/Main.qml"));
        QScopedPointer<QObject> root(mainComponent.create());
        QVERIFY2(root, qPrintable(mainComponent.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *audio = qobject_cast<QQuickItem *>(findNamed(window, "audioButton"));
        auto *keyboard = qobject_cast<QQuickItem *>(findNamed(window, "keyboardSurface"));
        QVERIFY(audio);
        QVERIFY(keyboard);

        audio->forceActiveFocus();
        QTRY_COMPARE(window->activeFocusItem(), audio);
        QVERIFY(window->activeFocusItem() != keyboard); // FocusScope also reports focus for active children.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(audio));
        QTRY_VERIFY(fakeStudio->property("running").toBool());
        QTRY_COMPARE(window->activeFocusItem(), keyboard);
        QTest::keyPress(window, Qt::Key_A);
        QTRY_COMPARE(fakeStudio->property("noteOnCount").toInt(), 1);
        QCOMPARE(fakeStudio->property("lastPitch").toInt(), 60);
        QTest::keyRelease(window, Qt::Key_A);
        QTRY_COMPARE(fakeStudio->property("noteOffCount").toInt(), 1);
        QCOMPARE(fakeStudio->property("lastPitch").toInt(), 60);

        audio->forceActiveFocus();
        QTRY_COMPARE(window->activeFocusItem(), audio);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(audio));
        QTRY_VERIFY(!fakeStudio->property("running").toBool());
        QTRY_COMPARE(window->activeFocusItem(), keyboard);
        QTest::keyPress(window, Qt::Key_A);
        QTRY_COMPARE(fakeStudio->property("noteOnCount").toInt(), 2);
        QCOMPARE(fakeStudio->property("lastPitch").toInt(), 60);
        QTest::keyRelease(window, Qt::Key_A);
        QTRY_COMPARE(fakeStudio->property("noteOffCount").toInt(), 2);
        QCOMPARE(fakeStudio->property("lastPitch").toInt(), 60);
    }
    void keyboardChordOffscreen() {
        StudioController studio(directory_);
        QJsonObject result;
        QVERIFY(studio.keyboardChordCheck(result));
        QCOMPARE(result["active_channels"].toInt(), 4);
        studio.panic();
        QVERIFY(studio.keyboardChordCheck(result));
        QCOMPARE(result["active_channels"].toInt(), 4);
        for (const auto &value : studio.connections()) QVERIFY(value.toMap()["channels"].toInt() > 1);
    }
    void fullscreenWindowControls() {
        StudioController studio(directory_);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
        QVERIFY(window);
        window->resize(1000, 650);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *full = qobject_cast<QQuickItem *>(findNamed(window, "fullscreenButton"));
        auto *minimize = qobject_cast<QQuickItem *>(findNamed(window, "minimizeButton"));
        auto *save = qobject_cast<QQuickItem *>(findNamed(window, "saveButton"));
        QVERIFY(full);
        QVERIFY(minimize);
        QVERIFY(save);
        QTRY_VERIFY(window->width() == 1000);
        auto *actions = qobject_cast<QQuickItem *>(findNamed(window, "toolbarActions"));
        QVERIFY(actions);
        QVERIFY(actions->mapToScene(QPointF()).x() >= 202);
        QVERIFY(!(window->visibility() == QWindow::FullScreen));
        QCOMPARE(full->property("text").toString(), QString("Full"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(full));
        QTRY_VERIFY((window->visibility() == QWindow::FullScreen));
        QCOMPARE(full->property("text").toString(), QString("Exit"));
        save->forceActiveFocus();
        QTRY_VERIFY(save->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!(window->visibility() == QWindow::FullScreen));
        QCOMPARE(full->property("text").toString(), QString("Full"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(full));
        QTRY_VERIFY((window->visibility() == QWindow::FullScreen));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(full));
        QTRY_VERIFY(!(window->visibility() == QWindow::FullScreen));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(minimize));
        QTRY_VERIFY((window->visibility() == QWindow::Minimized));
        window->showNormal();
        QTRY_VERIFY(!(window->visibility() == QWindow::Minimized));
        QVERIFY(!studio.running());
    }
    void fullscreenLaunchFlagOffline() {
        const QString executable = QString::fromUtf8(WS_STUDIO_TEST_EXECUTABLE);
        QVERIFY2(QFile::exists(executable), qPrintable(executable));
        const QString reportPath = temp_.filePath("fullscreen-launch.json");
        QProcess child;
        child.setProgram(executable);
        child.setArguments({"--fullscreen", "--startup-check", "--ready-report", reportPath});
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start();
        QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));
        QVERIFY2(child.waitForFinished(15000), child.readAll().constData());
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QFile report(reportPath);
        QVERIFY(report.open(QIODevice::ReadOnly));
        const auto json = QJsonDocument::fromJson(report.readAll()).object();
        QCOMPARE(json.value("status").toString(), QString("ready"));
        QVERIFY(!json.value("audio_started").toBool());
    }
    void clockRunToggleUsesStableParameter() {
        ParameterBridge bridge;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("studio", &bridge);
        QQmlComponent component(&engine, QUrl("qrc:/qml/ParameterControl.qml"));
        auto *object = component.beginCreate(engine.rootContext());
        QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("moduleId", "essentials.clock");
        object->setProperty("nodeId", 42);
        QVariantMap runDescriptor;
        runDescriptor.insert("id", 3);
        runDescriptor.insert("name", "Run");
        runDescriptor.insert("minimum", 0.);
        runDescriptor.insert("maximum", 1.);
        runDescriptor.insert("value", 1.);
        runDescriptor.insert("unit", "");
        runDescriptor.insert("display", "1");
        object->setProperty("descriptor", runDescriptor);
        component.completeCreate();
        QScopedPointer<QObject> owner(object);
        auto *control = qobject_cast<QQuickItem *>(object);
        QVERIFY(control);
        QQuickWindow window;
        window.resize(360, 160);
        control->setParentItem(window.contentItem());
        control->setWidth(280);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *toggle = qobject_cast<QQuickItem *>(findNamed(control, "clockRunToggle"));
        auto *slider = qobject_cast<QQuickItem *>(findNamed(control, "parameterSlider"));
        QVERIFY(toggle);
        QVERIFY(slider);
        QVERIFY(toggle->isVisible());
        QVERIFY(!slider->isVisible());
        QCOMPARE(toggle->property("text").toString(), QString("Stop Clock"));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, center(toggle));
        QCOMPARE(bridge.accepted, 0.);
        QCOMPARE(bridge.calls, 1);
        QTRY_COMPARE(toggle->property("text").toString(), QString("Start Clock"));
        toggle->forceActiveFocus();
        QTRY_VERIFY(toggle->hasActiveFocus());
        QTest::keyClick(&window, Qt::Key_Space);
        QTRY_COMPARE(bridge.accepted, 1.);
        QCOMPARE(bridge.calls, 2);
        QTRY_COMPARE(toggle->property("text").toString(), QString("Stop Clock"));
        bridge.accepted = 0.;
        emit bridge.parameterChanged(42, 3, 0., "0");
        QTRY_COMPARE(toggle->property("text").toString(), QString("Start Clock"));
        bridge.reject = true;
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, center(toggle));
        QCOMPARE(bridge.calls, 3);
        QCOMPARE(bridge.accepted, 0.);
        QCOMPARE(toggle->property("text").toString(), QString("Start Clock"));
        // Parameter ID 3 alone must never turn another module's slider into Clock transport.
        control->setProperty("moduleId", "essentials.vco");
        QTRY_VERIFY(!toggle->isVisible());
        QTRY_VERIFY(slider->isVisible());
    }
    void genericPaletteDiscovery() {
        StudioController studio(directory_); QSet<QString> ids;
        for (const auto& item : studio.palette()) ids.insert(item.toMap().value("id").toString());
        QVERIFY(ids.contains("essentials.vco")); QVERIFY(ids.contains("essentials.adsr")); QVERIFY(ids.contains("essentials.vca")); QVERIFY(ids.contains("essentials.output")); QVERIFY(ids.contains("essentials.mixer"));
        // Installed types and the default patch's node count are independent.
        QCOMPARE(studio.modules().size(), 5);
        studio.addModule("essentials.mixer");
        QCOMPARE(studio.modules().size(), 6);
    }
    void transactionalValidation() {
        StudioController studio(directory_); QVERIFY(studio.ready());
        QVERIFY(studio.setParameter(1, 0, 440)); studio.moveModule(1, 120, 170);
        QVERIFY(studio.savePatch(path("good.json"))); const auto good = read("good.json");
        QCOMPARE(good["version"].toInt(), 2);
        QList<QJsonObject> malformed;
        auto bad = good; bad["version"] = 1.5; malformed.append(bad);
        bad = good; auto outputs = good["outputs"].toArray(); auto output = outputs[0].toObject(); output["module"] = "4"; outputs[0] = output; bad["outputs"] = outputs; malformed.append(bad);
        bad = good; auto nodes = good["modules"].toArray(); auto n = nodes[0].toObject(); n["x"] = "120"; nodes[0] = n; bad["modules"] = nodes; malformed.append(bad);
        bad = good; nodes = good["modules"].toArray(); n = nodes[0].toObject(); n["id"] = 2; nodes[0] = n; bad["modules"] = nodes; malformed.append(bad);
        bad = good; nodes = good["modules"].toArray(); n = nodes[0].toObject(); n["parameters"] = QJsonObject{{"frequency", 1e9}}; nodes[0] = n; bad["modules"] = nodes; malformed.append(bad);
        bad = good; auto cables = good["connections"].toArray(); auto c = cables[0].toObject(); auto from = c["from"].toObject(); from["module"] = 999; c["from"] = from; cables[0] = c; bad["connections"] = cables; malformed.append(bad);
        // Existing VCO audio -> existing VCA control input: a signal-type mismatch.
        bad = good; cables = good["connections"].toArray(); c = cables[0].toObject(); auto to = c["to"].toObject(); to["port"] = "gain_cv"; c["to"] = to; cables[0] = c; bad["connections"] = cables; malformed.append(bad);
        write("kind_mismatch.json", bad);
        QVERIFY(!studio.loadPatch(path("kind_mismatch.json")));
        QVERIFY(studio.errorText().contains("incompatible signal type"));
        QVERIFY(studio.setParameter(1, 0, 550));
        const auto accepted = studio.modules();
        for (const auto& document : malformed) { write("bad.json", document); QVERIFY(!studio.loadPatch(path("bad.json"))); QCOMPARE(studio.modules(), accepted); QVERIFY(studio.dirty()); QVERIFY(studio.ready()); }
        QVERIFY(studio.loadPatch(path("good.json"))); QCOMPARE(studio.modules()[0].toMap()["parameters"].toList()[0].toMap()["value"].toDouble(), 440.);
        QVERIFY(!studio.dirty()); QVERIFY(!studio.running());
    }
    void incompleteAndMissing() {
        StudioController studio(directory_);
        studio.removeModule(4);
        QVERIFY(!studio.ready());
        QCOMPARE(studio.modules().size(), 4);
        QVERIFY(studio.savePatch(path("incomplete.json"))); QVERIFY(studio.loadPatch(path("incomplete.json"))); QVERIFY(!studio.ready());
        studio.addModule("essentials.output"); QCOMPARE(studio.outputNode(), 4); studio.connectPorts(3, "audio", 4, "audio"); QVERIFY(studio.ready());
        const int count = studio.connections().size(); studio.connectPorts(1, "audio", 3, "gain_cv"); QCOMPARE(studio.connections().size(), count);
        QVERIFY(studio.savePatch(path("before_missing.json"))); auto doc = read("before_missing.json");
        doc["futureRoot"] = QJsonObject{{"untouched", QJsonArray{1, "two"}}}; auto nodes = doc["modules"].toArray(); auto n = nodes[0].toObject(); n["module"] = "future.missing"; n["inactive"] = true; n["parameters"] = QJsonObject{}; n["preserved"] = QJsonObject{{"type", "future.missing"}, {"values", QJsonObject{{"futureMode", "poly"}, {"curve", QJsonArray{1, 2, 3}}, {"opaque", QJsonObject{{"enabled", true}}}}}}; n["futurePayload"] = QJsonObject{{"blob", "keep"}}; nodes[0] = n; doc["modules"] = nodes;
        write("missing.json", doc); QVERIFY(studio.loadPatch(path("missing.json"))); QVERIFY(!studio.ready()); QVERIFY(studio.savePatch(path("missing_roundtrip.json"))); QCOMPARE(read("missing_roundtrip.json"), doc);
        studio.moveModule(1, 500, 600); studio.removeModule(1); QVERIFY(studio.ready()); QVERIFY(!studio.running());
    }
    void atomicSaveFailure() {
        StudioController studio(directory_); QVERIFY(studio.savePatch(path("atomic.json"))); const auto original = read("atomic.json"); QVERIFY(studio.setParameter(1, 0, 660));
        QVERIFY(!studio.savePatch(path("no_such_directory/atomic.json"))); QVERIFY(studio.dirty()); QCOMPARE(read("atomic.json"), original);
        QVERIFY(studio.savePatch(path("atomic.json"))); QVERIFY(!studio.dirty()); QVERIFY(read("atomic.json") != original);
    }
    void targetedParameterFeedback() {
        StudioController studio(directory_); QSignalSpy reset(&studio, &StudioController::documentChanged), change(&studio, &StudioController::parameterChanged);
        QVERIFY(studio.setParameter(1, 0, 330)); QCOMPARE(change.count(), 1); QCOMPARE(reset.count(), 0);
        QVERIFY(!studio.setParameter(1, 0, -1)); QCOMPARE(change.count(), 1);
    }
    void actualSliderInteraction() {
        ParameterBridge bridge; QQmlEngine engine; engine.rootContext()->setContextProperty("studio", &bridge);
        QQmlComponent component(&engine, QUrl("qrc:/qml/ParameterControl.qml"));
        auto* object = component.beginCreate(engine.rootContext()); QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("nodeId", 1); object->setProperty("descriptor", QVariantMap{{"id", 0}, {"name", "Base frequency"}, {"minimum", 20.}, {"maximum", 20000.}, {"value", 220.}, {"unit", "Hz"}, {"display", "220.0 Hz"}});
        component.completeCreate(); QScopedPointer<QObject> owner(object); auto* control = qobject_cast<QQuickItem*>(object); QVERIFY(control);
        QQuickWindow window; window.resize(360,120); control->setParentItem(window.contentItem()); control->setWidth(320); window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* slider = qobject_cast<QQuickItem*>(findNamed(control,"parameterSlider")); QVERIFY(slider);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, center(slider)); QVERIFY(bridge.calls > 0); const double accepted = bridge.accepted; QVERIFY(accepted != 220);
        QCOMPARE(control->property("acceptedValue").toDouble(), accepted); QCOMPARE(findNamed(control,"parameterDisplay")->property("text").toString(), QString::number(accepted,'f',1)+" Hz");
        const auto* stableSlider = slider;
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, center(slider));
        QTest::mouseMove(&window, slider->mapToScene(QPointF(slider->width()*.65,slider->height()/2)).toPoint());
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, slider->mapToScene(QPointF(slider->width()*.7,slider->height()/2)).toPoint());
        QCOMPARE(findNamed(control,"parameterSlider"), stableSlider);
        slider->forceActiveFocus(); QTest::keyClick(&window, Qt::Key_Right); QVERIFY(bridge.accepted != accepted);
        bridge.reject = true; const double before = slider->property("value").toDouble(); const double acceptedBefore = bridge.accepted;
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, slider->mapToScene(QPointF(slider->width()*.85,slider->height()/2)).toPoint());
        QCOMPARE(slider->property("value").toDouble(), before); QCOMPARE(bridge.accepted, acceptedBefore);
        object->setProperty("descriptor", QVariantMap{{"id", 0}, {"name", "Fixed"}, {"minimum", 1.}, {"maximum", 1.}, {"value", 1.}, {"unit", ""}, {"display", "1"}});
        QVERIFY(!slider->isEnabled()); QVERIFY(std::isfinite(slider->property("value").toDouble()));
    }
    void rotaryMappingAndRollback() {
        ParameterBridge bridge;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("studio", &bridge);
        QQmlComponent component(&engine, QUrl("qrc:/qml/ParameterControl.qml"));
        auto *object = component.beginCreate(engine.rootContext());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVariantMap descriptor;
        descriptor.insert("id", 0);
        descriptor.insert("name", "Frequency");
        descriptor.insert("minimum", 20.);
        descriptor.insert("maximum", 20000.);
        descriptor.insert("value", 220.);
        descriptor.insert("unit", "Hz");
        descriptor.insert("display", "220.0 Hz");
        object->setProperty("nodeId", 1);
        object->setProperty("presentation", "knob");
        object->setProperty("hierarchy", "primary");
        object->setProperty("descriptor", descriptor);
        component.completeCreate();
        QScopedPointer<QObject> owner(object);
        auto *control = qobject_cast<QQuickItem *>(object);
        QVERIFY(control);
        QQuickWindow window;
        window.resize(360, 230);
        control->setParentItem(window.contentItem());
        control->setWidth(236);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *knob = qobject_cast<QQuickItem *>(findNamed(control, "parameterKnob"));
        QVERIFY(knob);
        const auto value = [&] { return knob->property("value").toDouble(); };
        QVERIFY(std::abs(value() - std::log(220. / 20.) / std::log(1000.)) < 0.00001);
        const auto draggedTo = knob->mapToScene(QPointF(knob->width() / 2, knob->height() / 2 - 32)).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, center(knob));
        QTest::mouseMove(&window, draggedTo);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, draggedTo);
        QVERIFY(bridge.calls > 0);
        QVERIFY(bridge.accepted > 220.);
        bridge.accepted = 440.; // External updates also update the bridge’s authoritative value.
        emit bridge.parameterChanged(1, 0, 440., "440.0 Hz");
        QTRY_VERIFY(std::abs(value() - std::log(440. / 20.) / std::log(1000.)) < 0.00001);
        QCOMPARE(findNamed(control, "parameterDisplay")->property("text").toString(), QString("440.0 Hz"));
        knob->forceActiveFocus();
        const double beforeFine = bridge.accepted;
        QTest::keyClick(&window, Qt::Key_Right, Qt::ShiftModifier);
        QVERIFY(bridge.accepted > beforeFine);
        const double beforeReject = value(), acceptedBeforeReject = bridge.accepted;
        bridge.reject = true;
        QTest::keyClick(&window, Qt::Key_Right);
        QCOMPARE(bridge.accepted, acceptedBeforeReject);
        QCOMPARE(value(), beforeReject);
        emit bridge.parameterChanged(1, 0, 20., "20.0 Hz");
        QTRY_COMPARE(value(), 0.);
        emit bridge.parameterChanged(1, 0, 20000., "20000.0 Hz");
        QTRY_COMPARE(value(), 1.);
    }
    void humanReadableParameterTooltips() {
        ParameterBridge bridge;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("studio", &bridge);
        QQmlComponent component(&engine, QUrl("qrc:/qml/ParameterControl.qml"));
        auto *object = component.beginCreate(engine.rootContext());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVariantMap descriptor{{"id", 0},
                               {"name", "Frequency"},
                               {"minimum", 0.009999999776482582},
                               {"maximum", 20000.},
                               {"value", 440.},
                               {"unit", "Hz"},
                               {"display", "440 Hz"}};
        object->setProperty("nodeId", 1);
        object->setProperty("moduleId", "essentials.vco");
        object->setProperty("presentation", "knob");
        object->setProperty("descriptor", descriptor);
        component.completeCreate();
        QScopedPointer<QObject> owner(object);
        auto *control = qobject_cast<QQuickItem *>(object);
        QVERIFY(control);
        QQuickWindow window;
        window.resize(360, 230);
        control->setParentItem(window.contentItem());
        control->setWidth(236);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(findNamed(control, "parameterKnob"));
        const auto tooltip = [&] { return object->property("tooltipText").toString(); };
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · 0.01 Hz–20 kHz"));
        QCOMPARE(object->property("acceptedValue").toDouble(), 440.);
        QCOMPARE(object->property("acceptedDisplay").toString(), QString("440 Hz"));
        const auto range = [&](double lo, const QString &unit, double hi) {
            descriptor["minimum"] = lo;
            descriptor["maximum"] = hi;
            descriptor["unit"] = unit;
            object->setProperty("descriptor", descriptor);
        };
        range(440., "Hz", 440.);
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · 440 Hz–440 Hz"));
        range(0., "%", 100.);
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · 0 %–100 %"));
        range(-12.345, "dB", 6.);
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · -12.3 dB–6 dB"));
        range(0.005, "s", 2.5);
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · 5 ms–2.5 s"));
        object->setProperty("presentation", "slider");
        QTRY_VERIFY(findNamed(control, "parameterSlider"));
        QCOMPARE(tooltip(), QString::fromUtf8("Frequency · 5 ms–2.5 s"));
        QCOMPARE(object->property("acceptedValue").toDouble(), 440.);
        QCOMPARE(bridge.calls, 0);
    }
    void steppedSelectorRollback() {
        ParameterBridge bridge;
        bridge.accepted = 1.;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("studio", &bridge);
        QQmlComponent component(&engine, QUrl("qrc:/qml/ParameterControl.qml"));
        auto *object = component.beginCreate(engine.rootContext());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVariantMap descriptor;
        descriptor.insert("id", 1);
        descriptor.insert("name", "Ticks per beat");
        descriptor.insert("minimum", 1.);
        descriptor.insert("maximum", 24.);
        descriptor.insert("value", 1.);
        descriptor.insert("unit", "");
        descriptor.insert("display", "1");
        object->setProperty("nodeId", 7);
        object->setProperty("presentation", "stepper");
        object->setProperty("discrete", true);
        object->setProperty("descriptor", descriptor);
        component.completeCreate();
        QScopedPointer<QObject> owner(object);
        auto *control = qobject_cast<QQuickItem *>(object);
        QVERIFY(control);
        QQuickWindow window;
        window.resize(320, 120);
        control->setParentItem(window.contentItem());
        control->setWidth(220);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *selector = qobject_cast<QQuickItem *>(findNamed(control, "parameterStepper"));
        QVERIFY(selector);
        selector->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(bridge.accepted, 2.);
        QCOMPARE(selector->property("value").toInt(), 2);
        bridge.reject = true;
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(bridge.accepted, 2.);
        QCOMPARE(selector->property("value").toInt(), 2);
        emit bridge.parameterChanged(7, 1, 12., "12");
        QTRY_COMPARE(selector->property("value").toInt(), 12);
    }
    void moduleParameterHierarchyKeepsIds() {
        StudioController studio(directory_);
        for (const auto &type : {"essentials.clock", "essentials.mixer", "essentials.ladder", "essentials.lfo"})
            studio.addModule(type);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().front();
        const auto check = [&](const QString &module, const QList<int> &ids, const QStringList &presentations) {
            for (const auto &item : studio.modules()) {
                const auto data = item.toMap();
                if (data.value("moduleId").toString() != module) continue;
                const auto params = data.value("parameters").toList();
                QCOMPARE(params.size(), ids.size());
                for (int i = 0; i < ids.size(); ++i) {
                    QCOMPARE(params[i].toMap().value("id").toInt(), ids[i]);
                    auto *parameter =
                        findNamed(root, QString("parameter_%1_%2").arg(data.value("id").toInt()).arg(ids[i]));
                    QVERIFY(parameter);
                    QCOMPARE(parameter->property("presentation").toString(), presentations[i]);
                    if (module == "essentials.clock" && ids[i] == 3) {
                        auto *toggle = findNamed(parameter, "clockRunToggle");
                        QVERIFY(toggle);
                        QVERIFY(toggle->property("visible").toBool());
                    }
                }
                return;
            }
            QFAIL("Expected installed module absent from controller");
        };
        check("essentials.vco", {0}, {"knob"});
        check("essentials.adsr", {0, 1, 2, 3}, {"slider", "slider", "knob", "slider"});
        check("essentials.clock", {0, 1, 2, 3}, {"knob", "stepper", "slider", "slider"});
        check("essentials.mixer", {0, 1}, {"fader", "fader"});
        check("essentials.ladder", {0, 1, 2}, {"knob", "knob", "knob"});
        check("essentials.lfo", {0, 1, 2, 3, 4}, {"knob", "knob", "knob", "slider", "slider"});
    }
    void parameterHeavyOfflineRender() {
        StudioController studio(directory_); QVERIFY(studio.savePatch(path("heavy_base.json")));
        auto document = read("heavy_base.json"); QJsonArray nodes;
        for (int i=1; i<=64; ++i) {
            const QString type = i<=44 ? "essentials.adsr" : i<=63 ? "essentials.vco" : "essentials.output";
            const QJsonObject parameters = i<=44 ? QJsonObject{{"attack",.025},{"decay",.18},{"sustain",.65},{"release",.5}} : i<=63 ? QJsonObject{{"frequency",220.}} : QJsonObject{};
            nodes.append(QJsonObject{{"id",i},{"module",type},{"version",2},{"x",0.},{"y",0.},{"parameters",parameters}});
        }
        document["modules"] = nodes; document["connections"] = QJsonArray{}; document.remove("outputNode"); document["outputs"] = QJsonArray{QJsonObject{{"module",64},{"port","audio"},{"bus_channel",0}}};
        write("heavy.json",document); QVERIFY(studio.loadPatch(path("heavy.json"))); QVERIFY(studio.ready()); QVERIFY(!studio.running());
        studio.renderDemo(path("heavy.wav")); QVERIFY2(studio.errorText().isEmpty(),qPrintable(studio.errorText())); QFile wav(temp_.filePath("heavy.wav")); QVERIFY(wav.size()>44); QVERIFY(!studio.running());
    }
    void keyboardFocusAndMinimumLayout() {
        StudioController studio(directory_); QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio); engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects()[0]); QVERIFY(window);
        window->resize(1000,650); QVERIFY(QTest::qWaitForWindowExposed(window)); window->requestActivate();
        auto* surface = qobject_cast<QQuickItem*>(findNamed(window,"keyboardSurface")); QVERIFY(surface);
        auto* slider = qobject_cast<QQuickItem*>(findNamed(window,"parameterSlider")); QVERIFY(slider);
        auto* button = qobject_cast<QQuickItem*>(findNamed(window,"panicButton")); QVERIFY(button);
        auto held = [&] { return window->property("heldKeyCount").toInt(); };
        // Actual QML key events, with audio stopped. This proves release reconciliation,
        // not hardware playback or engine ownership (covered by native/controller tests).
        for (int key : {Qt::Key_A, Qt::Key_Space}) {
            QVERIFY(QMetaObject::invokeMethod(window,"takeKeyboardFocus")); QCOMPARE(window->activeFocusItem(),surface); QTest::keyPress(window,Qt::Key(key)); QCOMPARE(held(),1);
            slider->forceActiveFocus(); QTRY_COMPARE(held(),0);
            QTest::keyRelease(window,Qt::Key(key)); QCOMPARE(held(),0);
            QVERIFY(QMetaObject::invokeMethod(window,"takeKeyboardFocus")); QCOMPARE(window->activeFocusItem(),surface); QTest::keyPress(window,Qt::Key(key)); QCOMPARE(held(),1);
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(button)); QTRY_COMPARE(held(),0);
            QTest::keyRelease(window,Qt::Key(key));
        }
        QVERIFY(QMetaObject::invokeMethod(window,"takeKeyboardFocus")); QCOMPARE(window->activeFocusItem(),surface); QTest::keyPress(window,Qt::Key_A); QTest::keyPress(window,Qt::Key_S); QCOMPARE(held(),2);
        QTest::keyRelease(window,Qt::Key_S); QCOMPARE(held(),1); // registry only; no held-note fallback
        QVERIFY(studio.setParameter(1,0,330));
        QVERIFY(QMetaObject::invokeMethod(window,"requestAction",Q_ARG(QVariant,QVariant("new")))); QTRY_COMPARE(held(),0);
        auto* dialog = findNamed(window,"unsavedDialog"); QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyRelease(window,Qt::Key_A); QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(QMetaObject::invokeMethod(window,"takeKeyboardFocus")); QCOMPARE(window->activeFocusItem(),surface); QTest::keyPress(window,Qt::Key_Space); QCOMPARE(held(),1);
        studio.stopAudio(); QTRY_COMPARE(held(),0); QTest::keyRelease(window,Qt::Key_Space);
        slider->forceActiveFocus(); const double before = slider->property("value").toDouble();
        QTest::keyClick(window,Qt::Key_Right); QVERIFY(slider->property("value").toDouble() != before); QCOMPARE(held(),0);
        QVERIFY(QMetaObject::invokeMethod(window,"takeKeyboardFocus")); QCOMPARE(window->activeFocusItem(),surface); QTest::keyPress(window,Qt::Key_A); QCOMPARE(held(),1);
        QQuickWindow other; other.resize(100,100); other.show(); other.requestActivate();
        QTRY_COMPARE(held(),0); QTest::keyRelease(&other,Qt::Key_A); other.close();
        auto* performance = qobject_cast<QQuickItem*>(findNamed(window,"performanceControls"));
        auto* devices = qobject_cast<QQuickItem*>(findNamed(window,"deviceControls"));
        auto* actions = qobject_cast<QQuickItem*>(findNamed(window,"toolbarActions")); QVERIFY(performance); QVERIFY(devices); QVERIFY(actions);
        const QRectF performanceRect(performance->mapToScene(QPointF()),performance->size());
        const QRectF deviceRect(devices->mapToScene(QPointF()),devices->size());
        QVERIFY(!performanceRect.intersects(deviceRect));
        QVERIFY(deviceRect.right() <= window->width()); QVERIFY(performanceRect.bottom() <= window->height());
        QVERIFY(actions->mapToScene(QPointF()).x() >= 202); QVERIFY(!studio.running());
    }
    void studioSidebarLibraryReservesLegend() {
        StudioController studio(directory_);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *library = qobject_cast<QQuickItem *>(findNamed(window, "moduleLibraryScroll"));
        auto *legend = qobject_cast<QQuickItem *>(findNamed(window, "sidebarLegend"));
        QVERIFY(library);
        QVERIFY(legend);
        for (const QSize size : {QSize(1366, 768), QSize(1093, 650), QSize(1000, 650)}) {
            window->resize(size);
            QTRY_COMPARE(window->size(), size);
            // A resized QQuickWindow reports its size before anchored children are polished.
            QTRY_VERIFY(legend->mapToScene(QPointF()).y() + legend->height() < window->height());
            const QRectF libraryBounds(library->mapToScene(QPointF()), library->size());
            const QRectF legendBounds(legend->mapToScene(QPointF()), legend->size());
            QVERIFY(libraryBounds.height() > 120);
            QVERIFY(libraryBounds.bottom() + 8 <= legendBounds.top());
            QVERIFY(legendBounds.bottom() < window->height());
            QVERIFY(library->property("contentHeight").toDouble() > library->height());
            for (double zoom : {0.75, 1., 1.5}) {
                window->setProperty("zoom", zoom);
                QVERIFY(libraryBounds.bottom() + 8 <= legendBounds.top());
            }
        }
        auto *last =
            qobject_cast<QQuickItem *>(findNamed(window, QString("paletteEntry_%1").arg(studio.palette().size() - 1)));
        QVERIFY(last);
        last->forceActiveFocus();
        QTRY_VERIFY(library->property("contentY").toDouble() > 0);
        const QRectF lastBounds(last->mapToItem(library, QPointF()), last->size());
        QVERIFY(lastBounds.top() >= -1);
        QVERIFY(lastBounds.bottom() <= library->height() + 1);
        QVERIFY(!studio.running());
    }
    void canvasMouseEditing_data() {
        QTest::addColumn<double>("zoom");
        QTest::newRow("100-percent") << 1.;
        QTest::newRow("85-percent") << .85;
        QTest::newRow("62-percent") << .62;
    }
    void canvasMouseEditing() {
        QFETCH(double, zoom);
        StudioController studio(directory_);
        // Compact, non-overlapping cards, with a real output selected throughout.
        studio.removeModule(2);
        studio.removeModule(5);
        studio.moveModule(1, 20, 20);
        studio.moveModule(3, 400, 20);
        studio.moveModule(4, 780, 20);
        studio.addModule("essentials.lfo");
        studio.moveModule(2, 20, 370);
        while (!studio.connections().isEmpty()) studio.disconnectCable(0);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
        QVERIFY(window);
        window->resize(1600, 1050);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->setProperty("zoom", zoom);
        window->setProperty("panX", 25.);
        window->setProperty("panY", 20.);
        auto item = [&](const QString &name) { return qobject_cast<QQuickItem *>(findNamed(window, name)); };
        auto *card = item("module_1");
        auto *header = item("moduleHeader_1");
        QVERIFY(card);
        QVERIFY(header);
        QTest::qWait(30); // Let anchors and scaled item geometry polish before mouse delivery.
        const QPoint start = center(header);
        const QPoint end = start + QPoint(42, 28);
        const QPointF oldPosition = card->position();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(20, 14), 20);
        QTest::mouseMove(window, end, 20);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(card->x() > oldPosition.x() + 10);
        QVERIFY(card->y() > oldPosition.y() + 10);
        const QPointF moved = card->position();
        auto cableDrag = [&](const QString &source, const QString &destination, double targetX = .5) {
            auto *from = item(source), *to = item(destination);
            if (!from || !to) return false;
            const QPoint a = center(from);
            const QPoint b = to->mapToScene(QPointF(to->width() * targetX, to->height() / 2)).toPoint();
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, a);
            QTest::mouseMove(window, (a + b) / 2, 20);
            QTest::mouseMove(window, b, 20);
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, b);
            return true;
        };
        QVERIFY(cableDrag("output_1_audio", "input_3_audio", .02));
        QTRY_COMPARE(studio.connections().size(), 1);
        QVERIFY(cableDrag("output_3_audio", "input_4_audio"));
        QTRY_COMPARE(studio.connections().size(), 2);
        auto *handle = item("cableHandle_0");
        QVERIFY(handle);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(handle));
        QTRY_COMPARE(window->property("selectedCable").toInt(), 0);
        auto *disconnect = item("disconnectCableButton");
        QVERIFY(disconnect);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(disconnect));
        QTRY_COMPARE(studio.connections().size(), 1);
        // Click-click remains supported alongside the new drag gesture.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(item("output_1_audio")));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(item("input_3_audio")));
        QTRY_COMPARE(studio.connections().size(), 2);
        QVERIFY(cableDrag("output_2_control", "input_3_gain_cv"));
        QTRY_COMPARE(studio.connections().size(), 3);
        // An incompatible release never inserts a cable; Escape cancels the pending gesture.
        QVERIFY(cableDrag("output_1_audio", "input_3_gain_cv"));
        QCOMPARE(studio.connections().size(), 3);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(window->property("pendingNode").toInt(), -1);
        const auto expected = studio.connections();
        QVERIFY(studio.savePatch(path("mouse-edit.json")));
        QVERIFY(studio.loadPatch(path("mouse-edit.json")));
        const auto restored = studio.connections();
        QCOMPARE(restored.size(), expected.size());
        // Reload preserves nested v2 endpoint metadata in the editor view; compare all link fields.
        for (int i = 0; i < expected.size(); ++i)
            for (const auto &key : {"fromNode", "fromPort", "toNode", "toPort", "kind", "channels"})
                QCOMPARE(restored[i].toMap().value(key), expected[i].toMap().value(key));
        QVERIFY(studio.savePatch(path("mouse-edit-reloaded.json")));
        QCOMPARE(read("mouse-edit-reloaded.json")["connections"], read("mouse-edit.json")["connections"]);
        card = item("module_1");
        QVERIFY(card);
        QCOMPARE(card->position(), moved);
        // A later-painted LFO header covers VCA's CV input. Never connect through it.
        studio.moveModule(2, 400, 100);
        studio.disconnectCable(2); // Rebuild delegates with the overlapping layout; CV input is empty.
        QTest::qWait(30);
        QVERIFY(cableDrag("output_2_control", "input_3_gain_cv"));
        QCOMPARE(studio.connections().size(), 2);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(window->property("pendingNode").toInt(), -1);
        QVERIFY(!studio.running());
    }
    void studioCableHandlesAreDistanceBounded() {
        StudioController studio(directory_);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
        QVERIFY(window);
        // Coincident, adjacent, far, reversed, vertical and diagonal endpoints.
        QList<QVariantList> endpoints;
        endpoints.append(QVariantList{0., 0., 0., 0.});
        endpoints.append(QVariantList{0., 0., 2., 0.});
        endpoints.append(QVariantList{0., 0., 20., 0.});
        endpoints.append(QVariantList{0., 0., 900., 0.});
        endpoints.append(QVariantList{100., 25., -100., 25.});
        endpoints.append(QVariantList{20., 0., 20., 220.});
        endpoints.append(QVariantList{70., 40., 30., -10.});
        double prior = -1;
        for (const auto &p : endpoints) {
            QVariant handle;
            const bool invoked =
                QMetaObject::invokeMethod(window, "cableBend", Q_RETURN_ARG(QVariant, handle), Q_ARG(QVariant, p[0]),
                                          Q_ARG(QVariant, p[1]), Q_ARG(QVariant, p[2]), Q_ARG(QVariant, p[3]));
            QVERIFY(invoked);
            const double length = std::hypot(p[2].toDouble() - p[0].toDouble(), p[3].toDouble() - p[1].toDouble());
            const double bend = handle.toDouble();
            QVERIFY(std::isfinite(bend));
            QVERIFY(bend >= 0.);
            QVERIFY(bend <= std::min(150., length * 0.38) + 1e-9);
            if (length == 0.) QCOMPARE(bend, 0.);
            if (prior >= 0. && length == 20.) QVERIFY(bend > prior);
            prior = bend;
        }
        QVERIFY(!studio.running());
    }
    void saveFailureRetryDoesNotResumeAction() {
        bool failWrite = true; int writeAttempts = 0; QString acceptedPath;
        StudioController studio(directory_, nullptr, failureControlledWriter(failWrite, writeAttempts, temp_.path(), &acceptedPath)); QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("studio",&studio); engine.load(QUrl("qrc:/qml/Main.qml")); QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects()[0]); QVERIFY(window); QVERIFY(QTest::qWaitForWindowExposed(window));
        for (const QString action : {QString("new"), QString("open"), QString("close")}) {
            failWrite = true; const int attemptsBefore = writeAttempts;
            QVERIFY(studio.setParameter(1,0,440 + action.size())); const auto current = studio.modules();
            QVERIFY(QMetaObject::invokeMethod(window,"requestAction",Q_ARG(QVariant,QVariant(action))));
            auto* unsaved = findNamed(window,"unsavedDialog"); QTRY_VERIFY(unsaved->property("visible").toBool());
            auto* save = findButton(unsaved,"Save"); QVERIFY(save); QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(save));
            auto* file = findNamed(window,"saveDialog"); QTRY_VERIFY(file->property("visible").toBool());
            // The chooser must accept a valid destination before persistence can fail.
            QVERIFY(file->setProperty("selectedFile",path("failure_" + action + ".json"))); QVERIFY(QMetaObject::invokeMethod(file,"accept"));
            QTRY_COMPARE(writeAttempts, attemptsBefore + 1);
            auto* failure = findNamed(window,"saveFailureDialog"); QTRY_VERIFY(failure->property("visible").toBool());
            QCOMPARE(studio.errorText(),QString("Injected patch commit failure"));
            QVERIFY(studio.dirty()); QCOMPARE(studio.modules(),current);
            QVERIFY(!QFile::exists(temp_.filePath("failure_" + action + ".json")));
            failWrite = false;
            auto* retry = findButton(failure,"Retry"); QVERIFY(retry); QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(retry));
            QTRY_VERIFY(file->property("visible").toBool()); QCOMPARE(window->property("saveContinuation").toString(),QString());
            QVERIFY(file->setProperty("selectedFile",path("retry_" + action + ".json"))); QVERIFY(QMetaObject::invokeMethod(file,"accept"));
            QTRY_COMPARE(writeAttempts, attemptsBefore + 2);
            QCOMPARE(studio.modules(),current); QVERIFY(!studio.dirty()); QVERIFY(window->isVisible()); QVERIFY(!window->property("closeApproved").toBool());
            QFile saved(acceptedPath); QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto savedNodes = QJsonDocument::fromJson(saved.readAll()).object().value("modules").toArray();
            QCOMPARE(savedNodes.size(), 5);
            QCOMPARE(savedNodes.last().toObject().value("module").toString(), QString("host.keyboard"));
            QCOMPARE(savedNodes.at(0).toObject().value("parameters").toObject().value("frequency").toDouble(),440. + action.size());
        }
        // A successful save in the original transaction still performs New.
        QVERIFY(studio.setParameter(1,0,880));
        QVERIFY(QMetaObject::invokeMethod(window,"requestAction",Q_ARG(QVariant,QVariant("new"))));
        auto* unsaved = findNamed(window,"unsavedDialog"); QTRY_VERIFY(unsaved->property("visible").toBool());
        auto* save = findButton(unsaved,"Save"); QVERIFY(save); QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(save));
        auto* file = findNamed(window,"saveDialog"); QTRY_VERIFY(file->property("visible").toBool());
        QCOMPARE(window->property("saveContinuation").toString(),QString("new"));
        QVERIFY(file->setProperty("selectedFile",path("before_new.json"))); QVERIFY(QMetaObject::invokeMethod(file,"accept"));
        QVERIFY(!studio.dirty()); QCOMPARE(studio.modules()[0].toMap()["parameters"].toList()[0].toMap()["value"].toDouble(),220.);
        QFile saved(acceptedPath); QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto savedNodes = QJsonDocument::fromJson(saved.readAll()).object().value("modules").toArray();
        QCOMPARE(savedNodes.size(), 5);
        QCOMPARE(savedNodes.last().toObject().value("module").toString(), QString("host.keyboard"));
        QCOMPARE(savedNodes.at(0).toObject().value("parameters").toObject().value("frequency").toDouble(), 880.);
    }
    void actualDocumentActionsAndCable() {
        bool failWrite = true; int writeAttempts = 0; QString acceptedPath;
        StudioController studio(directory_, nullptr, failureControlledWriter(failWrite, writeAttempts, temp_.path(), &acceptedPath)); QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("studio", &studio); engine.load(QUrl("qrc:/qml/Main.qml")); QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects()[0]); QVERIFY(window); QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(studio.setParameter(1, 0, 550)); studio.moveModule(1,100,100); const auto accepted = studio.modules(); const auto cables = studio.connections();
        for (const auto& buttonName : {"newButton", "openButton"}) {
            auto* button = qobject_cast<QQuickItem*>(findNamed(window,buttonName)); QVERIFY(button); QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(button));
            auto* dialog = findNamed(window,"unsavedDialog"); QVERIFY(dialog); QTRY_VERIFY(dialog->property("visible").toBool()); QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!dialog->property("visible").toBool());
            QCOMPARE(studio.modules(),accepted); QCOMPARE(studio.connections(),cables); QVERIFY(studio.dirty());
        }
        window->close(); auto* dialog = findNamed(window,"unsavedDialog"); QTRY_VERIFY(dialog->property("visible").toBool()); QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!dialog->property("visible").toBool()); QVERIFY(window->isVisible()); QCOMPARE(studio.modules(),accepted);
        // Choose Save in the actual unsaved dialog before exercising file acceptance.
        QVERIFY(QMetaObject::invokeMethod(window,"requestAction",Q_ARG(QVariant,QVariant("new"))));
        QTRY_VERIFY(dialog->property("visible").toBool());
        auto* saveChoice = findButton(dialog, "Save"); QVERIFY(saveChoice);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(saveChoice));
        auto* fileDialog = findNamed(window, "saveDialog"); QVERIFY(fileDialog);
        QTRY_VERIFY(fileDialog->property("visible").toBool());
        QCOMPARE(window->property("pendingAction").toString(),QString());
        QCOMPARE(window->property("saveContinuation").toString(),QString("new"));
        // A valid chooser destination reaches the injected persistence failure.
        QVERIFY(fileDialog->setProperty("selectedFile", path("failure_save.json")));
        QVERIFY(QMetaObject::invokeMethod(fileDialog,"accept"));
        QTRY_COMPARE(writeAttempts,1);
        auto* failure = findNamed(window,"saveFailureDialog"); QVERIFY(failure);
        QTRY_VERIFY(failure->property("visible").toBool());
        QCOMPARE(studio.errorText(),QString("Injected patch commit failure"));
        QVERIFY(studio.dirty()); QCOMPARE(studio.modules(),accepted);
        QCOMPARE(window->property("pendingAction").toString(),QString());
        QCOMPARE(window->property("saveContinuation").toString(),QString());
        QTRY_VERIFY(failure->property("opened").toBool());
        auto* failureContent = qvariant_cast<QQuickItem*>(failure->property("contentItem"));
        auto* failureOwner = failureContent ? failureContent->window() : nullptr;
        QVERIFY(failureOwner);
        // Programmatic acceptance of the non-native chooser can leave its hidden
        // window as the process focus window. Restore only window activation;
        // the popup must acquire item focus itself and still handle real Escape.
        QTRY_VERIFY(!fileDialog->property("visible").toBool());
        failureOwner->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), failureOwner);
        QTRY_VERIFY(failure->property("activeFocus").toBool());
        QTest::keyClick(failureOwner,Qt::Key_Escape); QTRY_VERIFY(!failure->property("visible").toBool());
        failWrite = false;
        // A later ordinary Save must preserve the patch rather than performing old New.
        auto* saveButton = qobject_cast<QQuickItem*>(findNamed(window,"saveButton")); QVERIFY(saveButton);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(saveButton));
        QTRY_VERIFY(fileDialog->property("visible").toBool());
        QVERIFY(fileDialog->setProperty("selectedFile", path("qml_saved.json")));
        QVERIFY(QMetaObject::invokeMethod(fileDialog,"accept"));
        QTRY_COMPARE(writeAttempts,2);
        QCOMPARE(studio.modules(),accepted); QVERIFY(!studio.dirty());
        // Save-dialog cancellation also consumes a destructive continuation.
        QVERIFY(studio.setParameter(1,0,660));
        QVERIFY(QMetaObject::invokeMethod(window,"requestAction",Q_ARG(QVariant,QVariant("new"))));
        QTRY_VERIFY(dialog->property("visible").toBool());
        saveChoice = findButton(dialog,"Save"); QVERIFY(saveChoice);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center(saveChoice));
        QTRY_VERIFY(fileDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(fileDialog,"reject"));
        QCOMPARE(window->property("saveContinuation").toString(),QString()); QVERIFY(studio.dirty());
        studio.disconnectCable(0);
        // Fit first so the real port hit areas are in the viewport.
        QVERIFY(QMetaObject::invokeMethod(window,"fitPatch")); QTest::qWait(50);
        auto* output = qobject_cast<QQuickItem*>(findNamed(window,"output_1_audio")); auto* input = qobject_cast<QQuickItem*>(findNamed(window,"input_3_audio")); QVERIFY(output); QVERIFY(input);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(output));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(input));
        QCOMPARE(studio.connections().size(), 5);
        QVERIFY(!studio.running());
        // Discard executes the pending New action only after the dialog choice.
        QVERIFY(QMetaObject::invokeMethod(window, "requestAction", Q_ARG(QVariant, QVariant("new"))));
        QVERIFY(QMetaObject::invokeMethod(dialog, "discarded"));
        QCOMPARE(studio.modules().size(), 5);
        QVERIFY(!studio.dirty());
    }
};
int main(int argc,char** argv) { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QQuickStyle::setStyle("Basic"); QGuiApplication app(argc,argv); app.setQuitOnLastWindowClosed(false); UiTests tests; return QTest::qExec(&tests,argc,argv); }
#include "ui_tests.moc"
