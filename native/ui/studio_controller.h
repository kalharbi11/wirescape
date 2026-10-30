#pragma once
#include "app_patch.h"
#include "audio_device.h"
#include "midi_host.h"
#include <QJsonObject>
#include <QObject>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <cstdint>
#include <functional>
#include <memory>
class QSettings;

class StudioController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap midiCounters READ midiCounters NOTIFY metersChanged)
    Q_PROPERTY(QVariantList midiEndpoints READ midiEndpoints NOTIFY statusChanged)
    Q_PROPERTY(bool midiAuthorized READ midiAuthorized NOTIFY statusChanged)
    Q_PROPERTY(bool midiSettingsAvailable READ midiSettingsAvailable CONSTANT)
    Q_PROPERTY(QVariantList modules READ modules NOTIFY documentChanged)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY documentChanged)
    Q_PROPERTY(QVariantList palette READ palette CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
    Q_PROPERTY(bool cleanupPending READ cleanupPending NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY statusChanged)
    Q_PROPERTY(QString diagnosticText READ diagnosticText NOTIFY metersChanged)
    Q_PROPERTY(int outputNode READ outputNode NOTIFY documentChanged)
    Q_PROPERTY(double peak READ peak NOTIFY metersChanged)
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY metersChanged)
    Q_PROPERTY(int underruns READ underruns NOTIFY metersChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY statusChanged)
    Q_PROPERTY(QString patchName READ patchName NOTIFY statusChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY statusChanged)
    Q_PROPERTY(int selectedDevice READ selectedDevice NOTIFY statusChanged)
    Q_PROPERTY(int sampleRate READ sampleRate CONSTANT)
    Q_PROPERTY(int blockSize READ blockSize CONSTANT)
public:
  enum class MidiPreference : std::uint8_t { Ignore, Interactive };
  static MidiPreference midiMode(const QString &platform, bool probe, const QProcessEnvironment &env) {
      return !probe && platform != QStringLiteral("offscreen") && platform != QStringLiteral("minimal") &&
                     !env.contains(QStringLiteral("CI")) && !env.contains(QStringLiteral("GITHUB_ACTIONS"))
                 ? MidiPreference::Interactive
                 : MidiPreference::Ignore;
  }
  using PatchWriter = std::function<bool(const QString &, const QByteArray &, QString &)>;
  explicit StudioController(QString directory, QObject *parent = nullptr, PatchWriter writer = {},
                            bool confirmMidi = false, MidiPreference midiPreference = MidiPreference::Ignore,
                            std::unique_ptr<wirescape::midi::Backend> fakeMidi = {},
                            const QString &midiSettingsPath = {});
  ~StudioController() override;
  QVariantMap midiCounters() const;
  QVariantList midiEndpoints();
  bool midiAuthorized() const { return midi_.authorized(); }
  bool midiSettingsAvailable() const { return bool(midiPreferences_); }
  Q_INVOKABLE void setMidiAuthorized(bool);
  Q_INVOKABLE void setMidiEndpoint(const QString &, const QString &, bool);
  Q_INVOKABLE void refreshMidi();
  QVariantList modules() const;
  QVariantList connections() const;
  QVariantList palette() const;
  bool running() const { return audio_.running(); }
  bool ready() const { return ready_; }
  bool cleanupPending() const { return audio_.hasStream() && !running(); }
  QString statusText() const;
  QString errorText() const { return error_; }
  QString noticeText() const { return notice_; }
  QString diagnosticText() const;
  Q_INVOKABLE QVariantMap diagnostics() const;
  Q_INVOKABLE bool saveDiagnostics(const QUrl &);
  int outputNode() const { return document_.value("outputNode").toInt(); }
  double peak() const { return peak_; }
  double cpuLoad() const { return audio_.cpuLoad() * 100.; }
  int underruns() const { return static_cast<int>(audio_.underruns()); }
  bool dirty() const { return dirty_; }
  QString patchName() const { return name_; }
  QVariantList devices() const { return audio_.devices(); }
  int selectedDevice() const { return device_; }
  int sampleRate() const { return 48000; }
  int blockSize() const { return 256; }
  Q_INVOKABLE bool setParameter(int node, int parameter, double value);
  Q_INVOKABLE void moveModule(int node, double x, double y);
  Q_INVOKABLE void addModule(const QString &id);
  Q_INVOKABLE void removeModule(int node);
  Q_INVOKABLE void connectPorts(int from, const QString &port, int to, const QString &input);
  Q_INVOKABLE void disconnectCable(int index);
  Q_INVOKABLE void startAudio();
  Q_INVOKABLE void stopAudio();
  Q_INVOKABLE void setGate(bool on);
  Q_INVOKABLE void note(int pitch, bool on);
  Q_INVOKABLE void panic();
  Q_INVOKABLE void setDevice(int id);
  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void selectOutput(int node);
  Q_INVOKABLE bool savePatch(const QUrl &);
  Q_INVOKABLE bool loadPatch(const QUrl &);
  Q_INVOKABLE void renderDemo(const QUrl &);
  Q_INVOKABLE void resetPatch();
  bool smokeCheck(QJsonObject &result);
  bool keyboardChordCheck(QJsonObject &result);
signals:
  void documentChanged();
  void parameterChanged(int node, int parameter, double value, QString display);
  void statusChanged();
  void metersChanged();

private:
    using Engine = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
    Engine makeEngine(const QJsonObject&, QString& error) const;
    bool compileDocument();
    bool validateDocument(const QJsonObject&, QString& error) const;
    QString incompleteReason(const QJsonObject&) const;
    bool commitTopology(QJsonObject candidate);
    const ws_module_descriptor* descriptor(const QString& id) const;
    const ws_module_descriptor* nodeDescriptor(int node) const;
    bool send(int node, uint32_t type, float value, uint32_t parameter = 0, uint32_t note = 1);
    bool applyParameters(AppEngine*, const QJsonObject&) const;
    void setError(QString);
    static int portIndex(const ws_port_descriptor*, uint32_t count, const QString& id);
    QString directory_, error_, notice_, name_ = "Essential voice";
    PatchWriter patchWriter_;
    QJsonObject document_;
    wirescape::document::PatchDocument documentSource_;
    Engine catalog_{nullptr, app_engine_destroy}, engine_{nullptr, app_engine_destroy};
    wirescape::midi::Host midi_;
    std::unique_ptr<QSettings> midiPreferences_;
    AudioDevice audio_;
    QTimer timer_;
    bool ready_ = false, dirty_ = false;
    int device_ = -1;
    uint64_t lastFrame_ = 0;
    double peak_ = 0;
    ws_meter lastSessionMeter_{};
};

