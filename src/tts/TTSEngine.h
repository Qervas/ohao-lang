#pragma once

#include <QObject>
#include <QLocale>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <memory>

#include "TTSProvider.h"

#ifndef QT_TEXTTOSPEECH_AVAILABLE
class QTextToSpeech {
public:
    enum State { Ready, Speaking, Paused, Error };
};
#else
#include <QTextToSpeech>
#endif

class TTSEngine : public QObject
{
    Q_OBJECT

public:
    explicit TTSEngine(QObject *parent = nullptr);
    ~TTSEngine();

    void configureGoogle(const QString& apiKey, const QString& voiceName, const QString& languageCode);

    QString googleVoice() const { return m_googleVoice; }
    QString googleLanguageCode() const { return m_googleLanguageCode; }

    void setInputVoice(const QString &voiceName);
    void setOutputVoice(const QString &voiceName);
    QString inputVoice() const { return m_inputVoice; }
    QString outputVoice() const { return m_outputVoice; }

    void setPrimaryVoice(const QString& voiceName);
    QString primaryVoice() const;

    void setProviderId(const QString& id);
    QString providerId() const { return m_providerId; }
    QStringList availableProviders() const;
    QString providerDisplayName(const QString& id) const;

    void setEdgeVoice(const QString& voiceName);
    QString edgeVoice() const { return m_edgeVoice; }
    void setEdgeExecutable(const QString& exePath);
    QString edgeExecutable() const { return m_edgeExecutable; }

    QStringList suggestedVoicesFor(const QLocale& locale) const;
    QString providerName() const;

    void setVolume(double volume);
    void setPitch(double pitch);
    void setRate(double rate);
    double volume() const { return m_volume; }
    double pitch() const { return m_pitch; }
    double rate() const { return m_rate; }

    bool isTTSInputEnabled() const { return m_ttsInputEnabled; }
    bool isTTSOutputEnabled() const { return m_ttsOutputEnabled; }
    void setTTSInputEnabled(bool enabled);
    void setTTSOutputEnabled(bool enabled);

    // Unified configuration method
    void configureFromCurrentSettings();

    void speak(const QString &text);
    void speak(const QString &text, bool isInputText);
    void speak(const QString &text, bool isInputText, const QLocale& locale);
    void stop();

    bool isAvailable() const;
    bool isSpeaking() const;
    QTextToSpeech::State state() const { return m_state; }

    void loadSettings();
    void saveSettings();

signals:
    void stateChanged(QTextToSpeech::State state);
    void errorOccurred(const QString &errorString);

private:
    QString effectiveVoice(bool isInputText) const;
    void applyProviderConfig(const QString& voiceOverride = QString());
    void ensureProvider();
    TTSProvider* provider() const { return m_provider.get(); }

    std::unique_ptr<TTSProvider> m_provider;
    QSettings *m_settings;
    QTextToSpeech::State m_state { QTextToSpeech::Ready };

    QString m_providerId { QStringLiteral("google-web") };
    QString m_googleVoice;
    QString m_googleLanguageCode;

    QString m_edgeVoice;
    QString m_edgeExecutable;

    QString m_inputVoice;
    QString m_outputVoice;

    double m_volume {1.0};
    double m_pitch {0.0};
    double m_rate {0.0};

    bool m_ttsInputEnabled { false };
    bool m_ttsOutputEnabled { true };
    bool m_isSpeaking { false };
};
