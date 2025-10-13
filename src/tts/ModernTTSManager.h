#pragma once

#include <QObject>
#include <QLocale>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <memory>

#ifndef QT_TEXTTOSPEECH_AVAILABLE
class QTextToSpeech {
public:
    enum State { Ready, Speaking, Paused, Error };
};
#else
#include <QTextToSpeech>
#endif

class TTSProvider;

/**
 * Modern Unified TTS Manager (2025)
 *
 * Features:
 * - Smart language detection and voice selection
 * - Automatic fallback system
 * - Audio device verification
 * - Unified API - no more input/output confusion
 * - Real-time voice testing
 * - Comprehensive error handling
 */
class ModernTTSManager : public QObject
{
    Q_OBJECT

public:
    enum class VoiceQuality {
        Neural,    // Best quality (Azure Neural, Google WaveNet)
        Standard,  // Good quality (Azure Standard, Google Standard)
        System     // Fallback (OS built-in)
    };

    enum class TTSProvider {
        SystemTTS,      // OS built-in TTS (default, uses downloaded voices)
        GoogleWeb,      // Google Web TTS (free, web-based)
        EdgeTTS,        // Microsoft Edge TTS (requires installation)
        AzureCognitive  // Azure Cognitive Services (paid, premium)
    };

    struct VoiceInfo {
        QString id;
        QString name;
        QLocale locale;
        VoiceQuality quality;
        TTSProvider provider;
        bool available;
    };

    struct TTSOptions {
        QLocale locale;
        VoiceQuality preferredQuality;
        TTSProvider preferredProvider;
        double volume;
        double rate;
        double pitch;
        bool enableFallback;
        
        TTSOptions()
            : locale(QLocale::system())
            , preferredQuality(VoiceQuality::Neural)
            , preferredProvider(TTSProvider::SystemTTS)
            , volume(1.0)
            , rate(1.0)
            , pitch(0.0)
            , enableFallback(true)
        {}
    };

public:
    static ModernTTSManager& instance();

    // Main TTS API - simple and unified
    void speak(const QString& text, const TTSOptions& options);
    void speak(const QString& text);
    void speak(const QString& text, const QString& languageCode);
    void speak(const QString& text, const QLocale& locale);

    // Control
    void stop();
    void pause();
    void resume();

    // Status
    bool isSpeaking() const;
    bool isAvailable() const;
    QTextToSpeech::State state() const;

    // Voice management
    QList<VoiceInfo> availableVoices() const;
    QList<VoiceInfo> voicesForLanguage(const QLocale& locale) const;
    VoiceInfo bestVoiceFor(const QLocale& locale, VoiceQuality preferredQuality = VoiceQuality::Neural) const;

    // System verification
    bool isAudioDeviceAvailable() const;
    QString getAudioStatus() const;

    // Testing
    void testVoice(const VoiceInfo& voice);
    void testCurrentConfiguration();

    // Configuration
    void loadSettings();
    void saveSettings();
    TTSOptions getDefaultOptions() const;
    void setDefaultOptions(const TTSOptions& options);

signals:
    void stateChanged(QTextToSpeech::State state);
    void speechStarted();
    void speechFinished();
    void speechError(const QString& error);
    void voiceChanged(const VoiceInfo& voice);

private slots:
    void onProviderError(const QString& error);

private:
    explicit ModernTTSManager(QObject* parent = nullptr);
    ~ModernTTSManager() = default;

    // Prevent copying
    ModernTTSManager(const ModernTTSManager&) = delete;
    ModernTTSManager& operator=(const ModernTTSManager&) = delete;

    // Core functionality
    void initializeProviders();
    void scanAvailableVoices();
    VoiceInfo selectBestVoice(const QLocale& locale, const TTSOptions& options) const;
    bool switchToVoice(const VoiceInfo& voice);
    void speakWithVoice(const QString& text, const VoiceInfo& voice, const TTSOptions& options);

    // Dynamic voice discovery helpers
    QString generateVoiceName(const QString& voiceId, const QLocale& locale) const;
    VoiceQuality determineVoiceQuality(const QString& voiceId) const;
    void addEmergencyFallbackVoices();

    // Provider management
    std::unique_ptr<::TTSProvider> createProvider(TTSProvider type) const;
    bool isProviderAvailable(TTSProvider type) const;
    TTSProvider getCurrentProviderType() const;

    // Fallback system
    void handleSpeechFailure(const QString& text, const TTSOptions& options, const QString& error);
    QList<VoiceInfo> getFallbackVoices(const QLocale& locale) const;

    // Audio verification
    void verifyAudioSystem();

    // Language detection
    QLocale detectLanguage(const QString& text) const;
    QLocale parseLanguageCode(const QString& languageCode) const;

private:
    static ModernTTSManager* s_instance;

    // Current state
    std::unique_ptr<::TTSProvider> m_currentProvider;
    VoiceInfo m_currentVoice;
    QTextToSpeech::State m_state = QTextToSpeech::Ready;
    TTSOptions m_defaultOptions;
    TTSProvider m_currentProviderType = TTSProvider::SystemTTS;

    // Voice database
    QList<VoiceInfo> m_availableVoices;
    QMap<TTSProvider, bool> m_providerAvailability;

    // Audio system
    bool m_audioAvailable = false;
    QString m_audioStatus;

    // Error handling
    int m_retryCount = 0;
    static constexpr int MAX_RETRIES = 2;

    // Initialization
    bool m_initialized = false;
};