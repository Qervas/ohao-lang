#pragma once

#include "TTSProvider.h"
#include <QTextToSpeech>
#include <QVoice>
#include <memory>

/**
 * System TTS Provider - Uses Qt's QTextToSpeech
 * 
 * This provider uses the native system TTS engine:
 * - macOS: Uses AVSpeechSynthesizer with downloaded system voices
 * - Windows: Uses SAPI
 * - Linux: Uses speech-dispatcher
 * 
 * Benefits:
 * - No external dependencies
 * - Uses voices downloaded in System Settings
 * - Works out of the box
 */
class SystemTTSProvider : public TTSProvider
{
    Q_OBJECT

public:
    explicit SystemTTSProvider(QObject* parent = nullptr);
    ~SystemTTSProvider() override;

    // TTSProvider interface
    QString id() const override { return QStringLiteral("system"); }
    QString displayName() const override { return QStringLiteral("System TTS"); }
    QStringList suggestedVoicesFor(const QLocale& locale) const override;
    void applyConfig(const Config& config) override;
    void speak(const QString& text, const QLocale& locale, double rate, double pitch, double volume) override;
    void stop() override;
    
    // Additional methods
    void pause();
    void resume();
    bool isAvailable() const;
    bool isSpeaking() const;

private slots:
    void onStateChanged(QTextToSpeech::State state);

private:
    void initializeEngine();
    QVoice findBestVoice(const QLocale& locale, const QString& voiceId = QString()) const;

    std::unique_ptr<QTextToSpeech> m_engine;
    Config m_config;
    bool m_initialized;
};
