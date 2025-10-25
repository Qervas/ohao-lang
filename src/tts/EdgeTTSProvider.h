#pragma once

#include <QProcess>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QTemporaryFile>

#include "TTSProvider.h"

// Microsoft Edge (edge-tts) provider using the open-source CLI wrapper. No API
// key required; the user supplies the executable path (python -m edge_tts or
// prebuilt binary).
class EdgeTTSProvider : public TTSProvider
{
    Q_OBJECT
public:
    explicit EdgeTTSProvider(QObject* parent = nullptr);
    ~EdgeTTSProvider() override;

    QString id() const override;
    QString displayName() const override;
    QStringList suggestedVoicesFor(const QLocale& locale) const override;

    void applyConfig(const Config& config) override;
    void speak(const QString& text,
               const QLocale& locale,
               double rate,
               double pitch,
               double volume) override;
    void stop() override;

    // Availability check
    bool isEdgeTTSAvailable() const { return m_edgeTtsAvailable; }

    // Dynamic voice discovery with caching
    static QStringList getAllAvailableVoices(bool forceRefresh = false);
    static QStringList getVoicesForLanguage(const QString& languageCode, bool forceRefresh = false);
    static void clearVoiceCache();
    static bool isVoiceCacheValid();

private:
    void launchEdgeTTS(const QString& text,
                       double rate,
                       double pitch,
                       double volume);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);

    QString m_executable;
    QString m_voice;
    QString m_tmpMediaPath;
    bool m_edgeTtsAvailable { false };

    QProcess* m_process { nullptr };
    QMediaPlayer* m_player { nullptr };
    QAudioOutput* m_audio { nullptr };
};

