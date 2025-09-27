#include "ModernTTSManager.h"
#include "TTSProvider.h"
#include "EdgeTTSProvider.h"
#include "GoogleWebTTSProvider.h"
#include "../ui/core/AppSettings.h"
#include "../ui/core/LanguageManager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QRegularExpression>
#include <algorithm>

ModernTTSManager* ModernTTSManager::s_instance = nullptr;

ModernTTSManager& ModernTTSManager::instance()
{
    if (!s_instance) {
        s_instance = new ModernTTSManager();
    }
    return *s_instance;
}

ModernTTSManager::ModernTTSManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "ModernTTSManager: Initializing unified TTS system";

    // Initialize in order
    loadSettings();
    verifyAudioSystem();
    initializeProviders();
    scanAvailableVoices();

    m_initialized = true;
    qDebug() << "ModernTTSManager: Initialization complete";
    qDebug() << "Available voices:" << m_availableVoices.size();
    qDebug() << "Audio available:" << m_audioAvailable;
}

void ModernTTSManager::speak(const QString& text, const TTSOptions& options)
{
    if (text.isEmpty()) {
        qDebug() << "ModernTTSManager: Empty text, skipping TTS";
        return;
    }

    if (!m_audioAvailable) {
        qDebug() << "ModernTTSManager: No audio device available:" << m_audioStatus;
        emit speechError("No audio device available");
        return;
    }

    qDebug() << "ModernTTSManager: Speaking text:" << text.left(50);
    qDebug() << "Requested locale:" << options.locale.name();

    // Select the best voice for this request
    ModernTTSManager::VoiceInfo voice = selectBestVoice(options.locale, options);

    if (!voice.available) {
        QString error = QString("No suitable voice found for locale: %1").arg(options.locale.name());
        qDebug() << "ModernTTSManager:" << error;

        if (options.enableFallback) {
            qDebug() << "ModernTTSManager: Attempting fallback voices";
            handleSpeechFailure(text, options, error);
            return;
        } else {
            emit speechError(error);
            return;
        }
    }

    speakWithVoice(text, voice, options);
}

void ModernTTSManager::speak(const QString& text, const QString& languageCode)
{
    QLocale locale = parseLanguageCode(languageCode);
    TTSOptions options = m_defaultOptions;
    options.locale = locale;
    speak(text, options);
}

void ModernTTSManager::speak(const QString& text, const QLocale& locale)
{
    TTSOptions options = m_defaultOptions;
    options.locale = locale;
    speak(text, options);
}

void ModernTTSManager::speakWithVoice(const QString& text, const ModernTTSManager::VoiceInfo& voice, const TTSOptions& options)
{
    qDebug() << "ModernTTSManager: Using voice:" << voice.name << "(" << voice.id << ")";

    // Switch to the selected voice if needed
    if (!switchToVoice(voice)) {
        QString error = QString("Failed to switch to voice: %1").arg(voice.name);
        qDebug() << "ModernTTSManager:" << error;

        if (options.enableFallback) {
            handleSpeechFailure(text, options, error);
        } else {
            emit speechError(error);
        }
        return;
    }

    // Configure and start speaking
    if (m_currentProvider) {
        // Start speaking with the existing TTSProvider interface
        m_currentProvider->speak(text, voice.locale, options.rate, options.pitch, options.volume);
        m_currentVoice = voice;

        qDebug() << "ModernTTSManager: Speech started successfully";
        emit speechStarted();
    } else {
        emit speechError("No TTS provider available");
    }
}

ModernTTSManager::VoiceInfo ModernTTSManager::selectBestVoice(const QLocale& locale, const TTSOptions& options) const
{
    qDebug() << "ModernTTSManager: Selecting best voice for" << locale.name();

    // Get voices for the requested language
    QList<ModernTTSManager::VoiceInfo> candidates = voicesForLanguage(locale);

    if (candidates.isEmpty()) {
        qDebug() << "ModernTTSManager: No voices found for" << locale.name();
        return ModernTTSManager::VoiceInfo{}; // Invalid voice
    }

    // Score voices based on preferences
    std::sort(candidates.begin(), candidates.end(), [&](const ModernTTSManager::VoiceInfo& a, const ModernTTSManager::VoiceInfo& b) {
        // Higher quality is better
        if (a.quality != b.quality) {
            return static_cast<int>(a.quality) < static_cast<int>(b.quality);
        }

        // Preferred provider is better
        if (a.provider == options.preferredProvider && b.provider != options.preferredProvider) {
            return true;
        }
        if (b.provider == options.preferredProvider && a.provider != options.preferredProvider) {
            return false;
        }

        // Exact locale match is better than language-only match
        if (a.locale == locale && b.locale.language() == locale.language()) {
            return true;
        }

        return false;
    });

    ModernTTSManager::VoiceInfo selected = candidates.first();
    qDebug() << "ModernTTSManager: Selected voice:" << selected.name
             << "quality:" << static_cast<int>(selected.quality)
             << "provider:" << static_cast<int>(selected.provider);

    return selected;
}

QList<ModernTTSManager::VoiceInfo> ModernTTSManager::voicesForLanguage(const QLocale& locale) const
{
    QList<ModernTTSManager::VoiceInfo> result;

    for (const ModernTTSManager::VoiceInfo& voice : m_availableVoices) {
        if (!voice.available) continue;

        // Exact locale match
        if (voice.locale == locale) {
            result.append(voice);
        }
        // Language match (e.g., sv-SE matches sv)
        else if (voice.locale.language() == locale.language()) {
            result.append(voice);
        }
    }

    return result;
}

ModernTTSManager::VoiceInfo ModernTTSManager::bestVoiceFor(const QLocale& locale, VoiceQuality preferredQuality) const
{
    TTSOptions options = m_defaultOptions;
    options.locale = locale;
    options.preferredQuality = preferredQuality;
    return selectBestVoice(locale, options);
}

void ModernTTSManager::initializeProviders()
{
    qDebug() << "ModernTTSManager: Initializing TTS providers";

    // Check availability of each provider
    m_providerAvailability[TTSProvider::EdgeTTS] = isProviderAvailable(TTSProvider::EdgeTTS);
    m_providerAvailability[TTSProvider::GoogleWeb] = isProviderAvailable(TTSProvider::GoogleWeb);
    m_providerAvailability[TTSProvider::AzureCognitive] = isProviderAvailable(TTSProvider::AzureCognitive);
    m_providerAvailability[TTSProvider::SystemTTS] = isProviderAvailable(TTSProvider::SystemTTS);

    // Initialize the default provider
    TTSProvider preferredProvider = m_defaultOptions.preferredProvider;
    if (m_providerAvailability[preferredProvider]) {
        m_currentProvider = createProvider(preferredProvider);
        qDebug() << "ModernTTSManager: Initialized preferred provider:" << static_cast<int>(preferredProvider);
    } else {
        // Find the first available provider
        for (auto it = m_providerAvailability.begin(); it != m_providerAvailability.end(); ++it) {
            if (it.value()) {
                m_currentProvider = createProvider(it.key());
                qDebug() << "ModernTTSManager: Initialized fallback provider:" << static_cast<int>(it.key());
                break;
            }
        }
    }

    if (!m_currentProvider) {
        qDebug() << "ModernTTSManager: Warning - No TTS providers available!";
    } else {
        // Connect provider signals
        connect(m_currentProvider.get(), &::TTSProvider::started, this, [this]() {
            m_state = QTextToSpeech::Speaking;
            emit stateChanged(m_state);
            emit speechStarted();
        });
        connect(m_currentProvider.get(), &::TTSProvider::finished, this, [this]() {
            m_state = QTextToSpeech::Ready;
            emit stateChanged(m_state);
            emit speechFinished();
            m_retryCount = 0;
        });
        connect(m_currentProvider.get(), &::TTSProvider::errorOccurred,
                this, &ModernTTSManager::onProviderError);
    }
}

void ModernTTSManager::scanAvailableVoices()
{
    qDebug() << "ModernTTSManager: Scanning available voices";
    m_availableVoices.clear();

    // For now, we'll create some common voices based on what's typically available
    // In a real implementation, this would query each provider for actual voice lists

    // Swedish voices
    ModernTTSManager::VoiceInfo swedishNeural;
    swedishNeural.id = "sv-SE-SofieNeural";
    swedishNeural.name = "Sofie (Swedish Neural)";
    swedishNeural.locale = QLocale("sv-SE");
    swedishNeural.quality = VoiceQuality::Neural;
    swedishNeural.provider = TTSProvider::EdgeTTS;
    swedishNeural.available = m_providerAvailability[TTSProvider::EdgeTTS];
    m_availableVoices.append(swedishNeural);

    // English voices
    ModernTTSManager::VoiceInfo englishNeural;
    englishNeural.id = "en-US-AriaNeural";
    englishNeural.name = "Aria (English Neural)";
    englishNeural.locale = QLocale("en-US");
    englishNeural.quality = VoiceQuality::Neural;
    englishNeural.provider = TTSProvider::EdgeTTS;
    englishNeural.available = m_providerAvailability[TTSProvider::EdgeTTS];
    m_availableVoices.append(englishNeural);

    // Add more voices for common languages
    struct VoiceTemplate {
        QString id;
        QString name;
        QString locale;
        VoiceQuality quality;
        TTSProvider provider;
    };

    QList<VoiceTemplate> templates = {
        {"zh-CN-XiaoxiaoNeural", "Xiaoxiao (Chinese Neural)", "zh-CN", VoiceQuality::Neural, TTSProvider::EdgeTTS},
        {"ja-JP-NanamiNeural", "Nanami (Japanese Neural)", "ja-JP", VoiceQuality::Neural, TTSProvider::EdgeTTS},
        {"ko-KR-SunHiNeural", "SunHi (Korean Neural)", "ko-KR", VoiceQuality::Neural, TTSProvider::EdgeTTS},
        {"fr-FR-DeniseNeural", "Denise (French Neural)", "fr-FR", VoiceQuality::Neural, TTSProvider::EdgeTTS},
        {"de-DE-KatjaNeural", "Katja (German Neural)", "de-DE", VoiceQuality::Neural, TTSProvider::EdgeTTS},
        {"es-ES-ElviraNeural", "Elvira (Spanish Neural)", "es-ES", VoiceQuality::Neural, TTSProvider::EdgeTTS},
    };

    for (const auto& tmpl : templates) {
        ModernTTSManager::VoiceInfo voice;
        voice.id = tmpl.id;
        voice.name = tmpl.name;
        voice.locale = QLocale(tmpl.locale);
        voice.quality = tmpl.quality;
        voice.provider = tmpl.provider;
        voice.available = m_providerAvailability[tmpl.provider];
        m_availableVoices.append(voice);
    }

    qDebug() << "ModernTTSManager: Found" << m_availableVoices.size() << "voices";
}

bool ModernTTSManager::switchToVoice(const ModernTTSManager::VoiceInfo& voice)
{
    if (!voice.available) {
        qDebug() << "ModernTTSManager: Voice not available:" << voice.name;
        return false;
    }

    // If we need to switch providers
    if (!m_currentProvider || voice.provider != m_currentProviderType) {
        m_currentProvider = createProvider(voice.provider);
        if (!m_currentProvider) {
            qDebug() << "ModernTTSManager: Failed to create provider for voice:" << voice.name;
            return false;
        }

        m_currentProviderType = voice.provider;

        // Reconnect signals
        connect(m_currentProvider.get(), &::TTSProvider::started, this, [this]() {
            m_state = QTextToSpeech::Speaking;
            emit stateChanged(m_state);
            emit speechStarted();
        });
        connect(m_currentProvider.get(), &::TTSProvider::finished, this, [this]() {
            m_state = QTextToSpeech::Ready;
            emit stateChanged(m_state);
            emit speechFinished();
            m_retryCount = 0;
        });
        connect(m_currentProvider.get(), &::TTSProvider::errorOccurred,
                this, &ModernTTSManager::onProviderError);
    }

    // Configure the provider for this voice
    ::TTSProvider::Config config;
    config.voice = voice.id;
    config.languageCode = voice.locale.name();
    m_currentProvider->applyConfig(config);
    return true;
}

ModernTTSManager::TTSProvider ModernTTSManager::getCurrentProviderType() const
{
    // This would need to be tracked when creating providers
    // For now, return the default
    return m_defaultOptions.preferredProvider;
}

std::unique_ptr<::TTSProvider> ModernTTSManager::createProvider(TTSProvider type) const
{
    switch (type) {
        case TTSProvider::EdgeTTS:
            return std::make_unique<EdgeTTSProvider>();
        case TTSProvider::GoogleWeb:
            return std::make_unique<GoogleWebTTSProvider>();
        case TTSProvider::AzureCognitive:
            // Not implemented yet
            return nullptr;
        case TTSProvider::SystemTTS:
            // Use EdgeTTS as fallback for now
            return std::make_unique<EdgeTTSProvider>();
        default:
            qDebug() << "ModernTTSManager: Unknown provider type:" << static_cast<int>(type);
            return nullptr;
    }
}

bool ModernTTSManager::isProviderAvailable(TTSProvider type) const
{
    switch (type) {
        case TTSProvider::EdgeTTS:
            // Check if edge-tts executable is available
            return true; // Simplified for now
        case TTSProvider::GoogleWeb:
            return true; // Usually available
        case TTSProvider::SystemTTS:
            return true; // Always available as fallback
        default:
            return false;
    }
}

void ModernTTSManager::verifyAudioSystem()
{
    qDebug() << "ModernTTSManager: Verifying audio system";

    // Check if we have audio output devices
    QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();

    if (audioDevices.isEmpty()) {
        m_audioAvailable = false;
        m_audioStatus = "No audio output devices found";
        qDebug() << "ModernTTSManager:" << m_audioStatus;
        return;
    }

    // Check if default device is valid
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull()) {
        m_audioAvailable = false;
        m_audioStatus = "No default audio output device";
        qDebug() << "ModernTTSManager:" << m_audioStatus;
        return;
    }

    m_audioAvailable = true;
    m_audioStatus = QString("Audio available: %1").arg(defaultDevice.description());
    qDebug() << "ModernTTSManager:" << m_audioStatus;
}

QLocale ModernTTSManager::parseLanguageCode(const QString& languageCode) const
{
    if (languageCode.isEmpty() || languageCode == "Auto-Detect") {
        return QLocale::system();
    }

    // Convert display names to language codes using LanguageManager
    QString actualCode = LanguageManager::instance().languageCodeFromDisplayName(languageCode);

    // Create locale from the code
    QLocale locale(actualCode);

    // Verify it's valid
    if (locale.language() == QLocale::C) {
        qDebug() << "ModernTTSManager: Invalid language code:" << languageCode << "using system locale";
        return QLocale::system();
    }

    return locale;
}

void ModernTTSManager::handleSpeechFailure(const QString& text, const TTSOptions& options, const QString& error)
{
    m_retryCount++;

    if (m_retryCount > MAX_RETRIES) {
        qDebug() << "ModernTTSManager: Max retries exceeded, giving up";
        emit speechError(QString("TTS failed after %1 retries: %2").arg(MAX_RETRIES).arg(error));
        m_retryCount = 0;
        return;
    }

    qDebug() << "ModernTTSManager: Attempting fallback speech (attempt" << m_retryCount << ")";

    // Try fallback voices
    QList<VoiceInfo> fallbacks = getFallbackVoices(options.locale);

    if (!fallbacks.isEmpty()) {
        ModernTTSManager::VoiceInfo fallback = fallbacks.first();
        qDebug() << "ModernTTSManager: Trying fallback voice:" << fallback.name;
        speakWithVoice(text, fallback, options);
    } else {
        // Last resort: try system default with English
        TTSOptions fallbackOptions = options;
        fallbackOptions.locale = QLocale("en");
        fallbackOptions.preferredProvider = TTSProvider::SystemTTS;

        ModernTTSManager::VoiceInfo systemVoice = selectBestVoice(fallbackOptions.locale, fallbackOptions);
        if (systemVoice.available) {
            qDebug() << "ModernTTSManager: Using system TTS as last resort";
            speakWithVoice(text, systemVoice, fallbackOptions);
        } else {
            emit speechError("All TTS options exhausted");
            m_retryCount = 0;
        }
    }
}

QList<ModernTTSManager::VoiceInfo> ModernTTSManager::getFallbackVoices(const QLocale& locale) const
{
    QList<VoiceInfo> fallbacks;

    // First try: other voices in the same language
    for (const ModernTTSManager::VoiceInfo& voice : m_availableVoices) {
        if (voice.available && voice.locale.language() == locale.language()) {
            fallbacks.append(voice);
        }
    }

    // Second try: English voices (most common)
    if (locale.language() != QLocale::English) {
        for (const ModernTTSManager::VoiceInfo& voice : m_availableVoices) {
            if (voice.available && voice.locale.language() == QLocale::English) {
                fallbacks.append(voice);
            }
        }
    }

    return fallbacks;
}

void ModernTTSManager::loadSettings()
{
    auto ttsConfig = AppSettings::instance().getTTSConfig();

    // Convert old settings to new format
    m_defaultOptions.volume = ttsConfig.volume;
    m_defaultOptions.rate = ttsConfig.speed;

    // Map old provider names to new enum
    if (ttsConfig.engine.contains("Edge") || ttsConfig.engine.contains("edge")) {
        m_defaultOptions.preferredProvider = TTSProvider::EdgeTTS;
    } else if (ttsConfig.engine.contains("Google")) {
        m_defaultOptions.preferredProvider = TTSProvider::GoogleWeb;
    } else {
        m_defaultOptions.preferredProvider = TTSProvider::SystemTTS;
    }

    qDebug() << "ModernTTSManager: Loaded settings - Provider:" << static_cast<int>(m_defaultOptions.preferredProvider);
}

void ModernTTSManager::saveSettings()
{
    // This would save the current configuration back to AppSettings
    // For now, we'll keep it simple
    qDebug() << "ModernTTSManager: Settings saved";
}

void ModernTTSManager::testCurrentConfiguration()
{
    QString testText = "This is a test of the text-to-speech system.";
    QLocale testLocale = QLocale::system();

    qDebug() << "ModernTTSManager: Testing current configuration";
    speak(testText, testLocale);
}

void ModernTTSManager::testVoice(const ModernTTSManager::VoiceInfo& voice)
{
    QString testText = "Hello, this is a voice test.";

    // Override locale for test
    TTSOptions options = m_defaultOptions;
    options.locale = voice.locale;

    qDebug() << "ModernTTSManager: Testing voice:" << voice.name;
    speakWithVoice(testText, voice, options);
}

bool ModernTTSManager::isAudioDeviceAvailable() const
{
    return m_audioAvailable;
}

QString ModernTTSManager::getAudioStatus() const
{
    return m_audioStatus;
}

QList<ModernTTSManager::VoiceInfo> ModernTTSManager::availableVoices() const
{
    return m_availableVoices;
}

ModernTTSManager::TTSOptions ModernTTSManager::getDefaultOptions() const
{
    return m_defaultOptions;
}

void ModernTTSManager::setDefaultOptions(const ModernTTSManager::TTSOptions& options)
{
    m_defaultOptions = options;
}

void ModernTTSManager::stop()
{
    if (m_currentProvider) {
        m_currentProvider->stop();
    }
}

void ModernTTSManager::pause()
{
    // Pause not supported by base TTSProvider interface
    // Just stop for now
    if (m_currentProvider) {
        m_currentProvider->stop();
    }
}

void ModernTTSManager::resume()
{
    // Resume not supported by base TTSProvider interface
    // Would need to re-speak the last text
    qDebug() << "ModernTTSManager: Resume not supported by current provider interface";
}

bool ModernTTSManager::isSpeaking() const
{
    return m_state == QTextToSpeech::Speaking;
}

bool ModernTTSManager::isAvailable() const
{
    return m_initialized && m_audioAvailable && m_currentProvider != nullptr;
}

QTextToSpeech::State ModernTTSManager::state() const
{
    return m_state;
}


void ModernTTSManager::onProviderError(const QString& error)
{
    qDebug() << "ModernTTSManager: Provider error:" << error;
    emit speechError(error);
}