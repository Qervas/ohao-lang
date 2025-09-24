#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QBuffer>
#include <QSettings>
#include <QLocale>

// Simple cloud TTS provider with Azure, Google Cloud, ElevenLabs, and Amazon Polly support.
// It fetches audio (MP3/WAV) over HTTP and plays via QMediaPlayer.
class CloudTTSProvider : public QObject {
    Q_OBJECT
public:
    explicit CloudTTSProvider(QObject* parent = nullptr);

    // Backend selection
    enum class Backend { None, Azure, Google, GoogleFree, ElevenLabs, Polly, Piper, Edge }; Q_ENUM(Backend)

    void setBackend(Backend b);
    Backend backend() const;

    // Azure config
    void setAzureConfig(const QString& region, const QString& apiKey, const QString& voiceName, const QString& style = QString());
    void setGoogleConfig(const QString& apiKey, const QString& voiceName, const QString& languageCode);
    void setElevenLabsConfig(const QString& apiKey, const QString& voiceId);
    void setPollyConfig(const QString& region, const QString& accessKey, const QString& secretKey, const QString& voiceName);
    void setPiperConfig(const QString& exePath, const QString& modelPath);
    void setEdgeConfig(const QString& exePath, const QString& voiceName);
    void setGoogleFreeConfig(const QString& voiceName);

    // Speak text using configured backend and voice
    void speak(const QString& text, const QLocale& locale, double rate, double pitch, double volume);
    void stop();

    // Discovery helpers
    static QStringList azureSuggestedVoicesFor(const QLocale& locale);
    static QStringList googleSuggestedVoicesFor(const QLocale& locale);
    static QStringList googleFreeSuggestedVoicesFor(const QLocale& locale);
    static QStringList edgeSuggestedVoicesFor(const QLocale& locale);
    static QStringList pollySuggestedVoicesFor(const QLocale& locale);

    // Dynamic voice fetching from APIs
    void fetchEdgeVoices();
    void fetchGoogleFreeVoices();

signals:
    void started();
    void finished();
    void errorOccurred(const QString& message);
    void voiceListReady(const QStringList& voices);

private slots:
    void onNetworkFinished(QNetworkReply* reply);

private:
    QByteArray buildAzureSSML(const QString& text, const QString& voiceName, const QString& style, double rate, double pitch, double volume) const;
    void postAzure(const QString& text, double rate, double pitch, double volume);
    void postGoogle(const QString& text, const QLocale& locale, double rate, double pitch, double volume);
    void postGoogleFree(const QString& text, const QLocale& locale);
    void postElevenLabs(const QString& text, double rate, double pitch, double volume);
    void postPolly(const QString& text, const QLocale& locale, double rate, double pitch, double volume);
    void postPiper(const QString& text);
    void postEdge(const QString& text, const QLocale& locale);

    // AWS SigV4 helpers
    static QByteArray hmacSha256(const QByteArray& key, const QByteArray& data);
    static QByteArray sha256(const QByteArray& data);
    static QString sha256Hex(const QByteArray& data);

    QNetworkAccessManager m_nam;
    QMediaPlayer* m_player { nullptr };
    QAudioOutput* m_audio { nullptr };

    Backend m_backend { Backend::None };
    Backend m_pendingBackend { Backend::None };

    // Azure
    QString m_azureRegion;
    QString m_azureKey;
    QString m_azureVoice;
    QString m_azureStyle;

    // Google
    QString m_googleApiKey;
    QString m_googleVoice;
    QString m_googleLanguageCode;

    // ElevenLabs
    QString m_elevenApiKey;
    QString m_elevenVoiceId;

    // Polly
    QString m_pollyRegion;
    QString m_pollyAccessKey;
    QString m_pollySecretKey;
    QString m_pollyVoice;

    // Piper
    QString m_piperExePath;
    QString m_piperModelPath;
    QString m_piperTempOutPath;

    // Edge (free)
    QString m_edgeExePath;
    QString m_edgeVoice;

    // GoogleFree
    QString m_googleFreeVoice;

    QByteArray m_lastAudio;
};
