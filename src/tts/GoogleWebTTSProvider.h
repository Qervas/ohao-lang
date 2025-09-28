#pragma once

#include <QNetworkAccessManager>
#include <QAudioOutput>
#include <QMediaPlayer>

#include "TTSProvider.h"

// Google Translate web TTS provider. Uses the public endpoint leveraged by the
// translate web app, requiring no API key.
class GoogleWebTTSProvider : public TTSProvider
{
    Q_OBJECT
public:
    explicit GoogleWebTTSProvider(QObject* parent = nullptr);

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

    // Enhanced voice caching system (static methods for consistency with EdgeTTS)
    static QStringList getAllAvailableVoices(bool forceRefresh = false);
    static QStringList getVoicesForLanguage(const QString& languageCode, bool forceRefresh = false);
    static void clearVoiceCache();
    static bool isVoiceCacheValid();

private:
    QString languageCodeForVoice(const QString& voice) const;
    QString languageCodeForLocale(const QLocale& locale) const;

    QNetworkAccessManager m_nam;
    QMediaPlayer* m_player { nullptr };
    QAudioOutput* m_audio { nullptr };
    QString m_voice;
    QString m_languageCode;
};
