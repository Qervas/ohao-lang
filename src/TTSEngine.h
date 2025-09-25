#pragma once

#include <QObject>
#ifdef QT_TEXTTOSPEECH_AVAILABLE
#include <QTextToSpeech>
#include <QVoice>
#endif
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QMap>
#include "CloudTTSProvider.h"

#ifndef QT_TEXTTOSPEECH_AVAILABLE
class QVoice {
public:
    enum Gender { Male, Female, Unknown };
    enum Age { Child, Teenager, Adult, Senior, Other };
    QVoice() {}
    QVoice(Gender gender, Age age, const QString &name = QString()) {}
    Gender gender() const { return Unknown; }
    Age age() const { return Other; }
    QString name() const { return QString(); }
};
#endif

#ifndef QT_TEXTTOSPEECH_AVAILABLE
class QTextToSpeech : public QObject {
    Q_OBJECT
public:
    enum State { Ready, Speaking, Paused, Error };
    QTextToSpeech(QObject *parent = nullptr) : QObject(parent) {}
    QTextToSpeech(const QString &engine, QObject *parent = nullptr) : QObject(parent) {}
    ~QTextToSpeech() {}
    void say(const QString &text) {}
    void stop() {}
    void pause() {}
    void resume() {}
    State state() const { return Ready; }
    void setLocale(const QLocale &locale) {}
    QLocale locale() const { return QLocale(); }
    void setVoice(const QVoice &voice) {}
    QVoice voice() const { return QVoice(); }
    void setVolume(double volume) {}
    double volume() const { return 1.0; }
    void setPitch(double pitch) {}
    double pitch() const { return 0.0; }
    void setRate(double rate) {}
    double rate() const { return 1.0; }
    QList<QLocale> availableLocales() const { return QList<QLocale>(); }
    QList<QVoice> availableVoices() const { return QList<QVoice>(); }
    QString engine() const { return QString(); }
    static QStringList availableEngines() { return QStringList(); }
signals:
    void stateChanged(State state);
    void localeChanged(const QLocale &locale);
    void voiceChanged(const QVoice &voice);
};
#endif

class TTSEngine : public QObject
{
    Q_OBJECT

public:
    // Backend control
    enum class Backend { System, Azure, Google, Edge, ElevenLabs, Polly, Piper, GoogleFree };
    void setBackend(Backend b);
    Backend backend() const;

    // Cloud TTS configuration (Azure)
    void configureAzure(const QString& region, const QString& apiKey, const QString& voiceName, const QString& style = QString());
    void configureGoogle(const QString& apiKey, const QString& voiceName, const QString& languageCode);
    void configureElevenLabs(const QString& apiKey, const QString& voiceId);
    void configurePolly(const QString& region, const QString& accessKey, const QString& secretKey, const QString& voiceName);
    void configureEdge(const QString& exePath, const QString& voiceName);
    void configurePiper(const QString& exePath, const QString& modelPath);
    void configureGoogleFree(const QString& voiceName);
    QString azureRegion() const; QString azureApiKey() const; QString azureVoice() const; QString azureStyle() const;
    QString googleApiKey() const; QString googleVoice() const; QString googleLanguageCode() const;
    QString elevenApiKey() const; QString elevenVoiceId() const;
    QString pollyRegion() const; QString pollyAccessKey() const; QString pollySecretKey() const; QString pollyVoice() const;
    QString edgeVoice() const;
    QString edgeExePath() const { return m_edgeExePath; }
    QString piperExePath() const; QString piperModelPath() const;
    
    // Access to cloud provider for dynamic voice fetching
    CloudTTSProvider& getCloudProvider() { return m_cloud; }
    
    // TTS option getters
    bool isTTSInputEnabled() const;
    bool isTTSOutputEnabled() const;
    void setTTSInputEnabled(bool enabled);
    void setTTSOutputEnabled(bool enabled);
    
    explicit TTSEngine(QObject *parent = nullptr);
    ~TTSEngine();

    // Core TTS functionality
    void speak(const QString &text);
    void speak(const QString &text, bool isInputText);  // For bilingual voice selection
    void stop();
    void pause();
    void resume();

    // Voice and language management
    QStringList getAvailableLanguages() const;
    QList<QVoice> getVoicesForLanguage(const QLocale &locale) const;
    QStringList getVoiceNamesForLanguage(const QLocale &locale) const;
    QString getLanguageDisplayName(const QLocale &locale) const;
    QLocale getLocaleForLanguageName(const QString &languageName) const;

    // Settings
    void setLocale(const QLocale &locale);
    void setVoice(const QVoice &voice);
    void setInputVoice(const QString &voiceName);   // For input text (OCR language)
    void setOutputVoice(const QString &voiceName);  // For output text (translation language)
    void setVolume(double volume); // 0.0 to 1.0
    void setPitch(double pitch);   // -1.0 to 1.0
    void setRate(double rate);     // 0.1 to 3.0

    // Getters
    QLocale currentLocale() const;
    QVoice currentVoice() const;
    QString inputVoice() const;     // Current input voice name
    QString outputVoice() const;    // Current output voice name
    double volume() const;
    double pitch() const;
    double rate() const;
    QStringList availableEngines() const;
    QString currentEngine() const;

    // State
    bool isAvailable() const;
    bool isSpeaking() const;
    QTextToSpeech::State state() const;

    // Settings persistence
    void loadSettings();
    void saveSettings();

signals:
    void stateChanged(QTextToSpeech::State state);
    void localeChanged(const QLocale &locale);
    void voiceChanged(const QVoice &voice);
    void errorOccurred(const QString &errorString);

private slots:
    void onStateChanged(QTextToSpeech::State state);
    void onLocaleChanged(const QLocale &locale);
    void onVoiceChanged(const QVoice &voice);

private:
    void initializeDefaults();
    QString voiceToString(const QVoice &voice) const;
    QVoice voiceFromString(const QString &voiceString, const QLocale &locale) const;
    void cacheAvailableLocalesAndVoices();

    QTextToSpeech *m_tts;
    QSettings *m_settings;
    QMap<QString, QList<QVoice>> m_voiceCache; // Use locale name as key instead
    QMap<QString, QLocale> m_languageToLocaleMap;
    bool m_isAvailable;
    QString m_engineName;

    // Backend
    Backend m_backend { Backend::System };
    CloudTTSProvider m_cloud;
    // Azure config cache (persisted via QSettings under TTS/*)
    QString m_azureRegion;
    QString m_azureKey;
    QString m_azureVoice;
    QString m_azureStyle;
    // Google
    QString m_googleApiKey; QString m_googleVoice; QString m_googleLanguageCode;
    // ElevenLabs
    QString m_elevenApiKey; QString m_elevenVoiceId;
    // Polly
    QString m_pollyRegion; QString m_pollyAccessKey; QString m_pollySecretKey; QString m_pollyVoice;
    // Edge (free)
    QString m_edgeExePath; QString m_edgeVoice;
    // Piper (free, local)
    QString m_piperExePath; QString m_piperModelPath;
    // GoogleFree
    QString m_googleFreeVoice;
    
    // TTS options
    bool m_ttsInputEnabled = false;
    bool m_ttsOutputEnabled = true;
    
    // Input/Output specific voices
    QString m_inputVoice;   // Voice for input text (OCR language)
    QString m_outputVoice;  // Voice for output text (translation language)
};