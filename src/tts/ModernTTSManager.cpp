#include "ModernTTSManager.h"
#include "TTSProvider.h"
#ifdef QT_TEXTTOSPEECH_AVAILABLE
#include "SystemTTSProvider.h"
#endif
#include "EdgeTTSProvider.h"
#include "GoogleWebTTSProvider.h"
#include "../ui/core/AppSettings.h"
#include "../ui/core/LanguageManager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
    qDebug() << "=== ModernTTSManager::speak() ENTRY ===";
    qDebug() << "Text:" << text.left(50);
    qDebug() << "Locale:" << options.locale.name();
    qDebug() << "Audio available:" << m_audioAvailable;
    qDebug() << "Initialized:" << m_initialized;

    if (text.isEmpty()) {
        qDebug() << "ModernTTSManager: Empty text, skipping TTS";
        return;
    }

    if (!m_audioAvailable) {
        qDebug() << "ModernTTSManager: No audio device available:" << m_audioStatus;
        qDebug() << "ModernTTSManager: Attempting TTS anyway - audio might work";
        // Don't return early - try TTS anyway as audio detection might be wrong
    }

    qDebug() << "ModernTTSManager: Speaking text:" << text.left(50);
    qDebug() << "Requested locale:" << options.locale.name();

    // Select the best voice for this request
    ModernTTSManager::VoiceInfo voice = selectBestVoice(options.locale, options);

    qDebug() << "Selected voice ID:" << voice.id;
    qDebug() << "Selected voice name:" << voice.name;
    qDebug() << "Selected voice available:" << voice.available;

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

    qDebug() << "ModernTTSManager: About to call speakWithVoice";
    speakWithVoice(text, voice, options);
    qDebug() << "=== ModernTTSManager::speak() EXIT ===";
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
    // Reload settings to get latest provider and voice selection
    loadSettings();

    TTSOptions options = m_defaultOptions;
    options.locale = locale;

    speak(text, options);
}

void ModernTTSManager::speak(const QString& text)
{
    speak(text, m_defaultOptions);
}

void ModernTTSManager::speakWithVoice(const QString& text, const ModernTTSManager::VoiceInfo& voice, const TTSOptions& options)
{
    qDebug() << "=== speakWithVoice() ENTRY ===";
    qDebug() << "Voice name:" << voice.name;
    qDebug() << "Voice ID:" << voice.id;
    qDebug() << "Voice provider:" << static_cast<int>(voice.provider);
    qDebug() << "Current provider exists:" << (m_currentProvider != nullptr);

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

    qDebug() << "Voice switch successful, provider exists:" << (m_currentProvider != nullptr);

    // Configure and start speaking
    if (m_currentProvider) {
        qDebug() << "About to call provider->speak() with:";
        qDebug() << "  Text:" << text.left(30);
        qDebug() << "  Locale:" << voice.locale.name();
        qDebug() << "  Rate:" << options.rate;
        qDebug() << "  Pitch:" << options.pitch;
        qDebug() << "  Volume:" << options.volume;

        // Start speaking with the existing TTSProvider interface
        m_currentProvider->speak(text, voice.locale, options.rate, options.pitch, options.volume);
        m_currentVoice = voice;

        qDebug() << "ModernTTSManager: Speech started successfully";
        emit speechStarted();
    } else {
        qDebug() << "ERROR: No TTS provider available!";
        emit speechError("No TTS provider available");
    }
    qDebug() << "=== speakWithVoice() EXIT ===";
}

ModernTTSManager::VoiceInfo ModernTTSManager::selectBestVoice(const QLocale& locale, const TTSOptions& options) const
{
    qDebug() << "=== selectBestVoice() ENTRY ===";
    qDebug() << "Requested locale:" << locale.name();
    qDebug() << "Preferred voice ID:" << options.preferredVoiceId;
    qDebug() << "Preferred provider:" << static_cast<int>(options.preferredProvider);
    qDebug() << "Total available voices:" << m_availableVoices.size();

    // First priority: if user has explicitly selected a voice, try to use it
    if (!options.preferredVoiceId.isEmpty()) {
        qDebug() << "Searching for user's preferred voice:" << options.preferredVoiceId;

        int matchCount = 0;
        for (const ModernTTSManager::VoiceInfo& voice : m_availableVoices) {
            if (voice.id == options.preferredVoiceId) {
                matchCount++;
                qDebug() << "  Found matching voice ID:" << voice.id << "Name:" << voice.name
                         << "Available:" << voice.available << "Provider:" << static_cast<int>(voice.provider);
                if (voice.available) {
                    qDebug() << "✅ ModernTTSManager: Using user's preferred voice:" << voice.name;
                    return voice;
                } else {
                    qDebug() << "⚠️ Voice found but NOT available!";
                }
            }
        }

        if (matchCount == 0) {
            qDebug() << "❌ User's preferred voice" << options.preferredVoiceId << "NOT FOUND in available voices list!";
            qDebug() << "   Available voice IDs:";
            for (int i = 0; i < qMin(10, m_availableVoices.size()); i++) {
                qDebug() << "   -" << m_availableVoices[i].id << "(" << m_availableVoices[i].name << ")";
            }
        } else {
            qDebug() << "⚠️ Preferred voice found but not available, falling back to auto-selection";
        }
    } else {
        qDebug() << "No preferred voice ID set, using auto-selection";
    }

    // Get voices for the requested language
    QList<ModernTTSManager::VoiceInfo> candidates = voicesForLanguage(locale);

    if (candidates.isEmpty()) {
        qDebug() << "ModernTTSManager: No voices found for" << locale.name();
        return ModernTTSManager::VoiceInfo{}; // Invalid voice
    }

    // Score voices based on preferences using a safe scoring system
    std::sort(candidates.begin(), candidates.end(), [&](const ModernTTSManager::VoiceInfo& a, const ModernTTSManager::VoiceInfo& b) {
        // Calculate scores for each voice (higher score = better voice)
        auto calculateScore = [&](const ModernTTSManager::VoiceInfo& voice) -> int {
            int score = 0;

            // Quality score (Neural = 3, Standard = 2, System = 1)
            score += static_cast<int>(voice.quality) + 1;

            // Preferred provider bonus
            if (voice.provider == options.preferredProvider) {
                score += 10;
            }

            // Exact locale match bonus
            if (voice.locale == locale) {
                score += 20;
            }
            // Language-only match bonus
            else if (voice.locale.language() == locale.language()) {
                score += 5;
            }

            return score;
        };

        int scoreA = calculateScore(a);
        int scoreB = calculateScore(b);

        // Higher score first (descending order)
        return scoreA > scoreB;
    });

    ModernTTSManager::VoiceInfo selected = candidates.first();
    qDebug() << "ModernTTSManager: Auto-selected voice:" << selected.name
             << "ID:" << selected.id
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
    m_providerAvailability[TTSProvider::SystemTTS] = isProviderAvailable(TTSProvider::SystemTTS);
    // m_providerAvailability[TTSProvider::AzureCognitive] = isProviderAvailable(TTSProvider::AzureCognitive);

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
    qDebug() << "=== ModernTTSManager: Voice Scanning Start ===";
    qDebug() << "ModernTTSManager: Dynamically scanning available voices from providers";
    m_availableVoices.clear();

    // Scan each available provider for their actual voice lists
    for (auto it = m_providerAvailability.begin(); it != m_providerAvailability.end(); ++it) {
        TTSProvider providerType = it.key();
        bool available = it.value();

        if (!available) {
            qDebug() << "Skipping unavailable provider:" << static_cast<int>(providerType);
            continue;
        }

        qDebug() << "Scanning voices for provider:" << static_cast<int>(providerType);

        // Create a temporary provider to get its voice list
        std::unique_ptr<::TTSProvider> provider = createProvider(providerType);
        if (!provider) {
            qDebug() << "Failed to create provider for voice scanning:" << static_cast<int>(providerType);
            continue;
        }

        // For now, get suggested voices for major languages
        // TODO: Implement proper dynamic voice discovery per provider
        QList<QLocale> majorLocales = {
            QLocale("en-US"), QLocale("en-GB"), QLocale("sv-SE"),
            QLocale("zh-CN"), QLocale("ja-JP"), QLocale("ko-KR"),
            QLocale("fr-FR"), QLocale("de-DE"), QLocale("es-ES"),
            QLocale("it-IT"), QLocale("pt-BR"), QLocale("ru-RU"),
            QLocale("ar-SA"), QLocale("hi-IN"), QLocale("th-TH"),
            QLocale("vi-VN"), QLocale("nl-NL"), QLocale("da-DK"),
            QLocale("no-NO"), QLocale("fi-FI"), QLocale("pl-PL")
        };

        for (const QLocale& locale : majorLocales) {
            QStringList voices = provider->suggestedVoicesFor(locale);
            if (!voices.isEmpty()) {
                qDebug() << "  Found" << voices.size() << "voices for" << locale.name()
                         << "(" << locale.language() << ") from provider" << static_cast<int>(providerType);
            }
            for (const QString& voiceId : voices) {
                ModernTTSManager::VoiceInfo voice;
                voice.id = voiceId;
                voice.name = generateVoiceName(voiceId, locale);
                voice.locale = locale;
                voice.quality = determineVoiceQuality(voiceId);
                voice.provider = providerType;
                voice.available = true;
                m_availableVoices.append(voice);
                qDebug() << "    Added voice:" << voice.name << "for locale" << locale.name();
            }
        }
    }

    qDebug() << "ModernTTSManager: Dynamically discovered" << m_availableVoices.size() << "voices";

    // Always add emergency fallbacks to ensure TTS works
    qDebug() << "Adding emergency fallbacks to ensure TTS works";
    addEmergencyFallbackVoices();

    // If we still got no voices, something is seriously wrong
    if (m_availableVoices.isEmpty()) {
        qDebug() << "CRITICAL: No voices available even after emergency fallbacks!";
    }

    qDebug() << "=== Final voice count:" << m_availableVoices.size() << "===";
    for (const VoiceInfo& voice : m_availableVoices) {
        qDebug() << "Voice:" << voice.id << voice.name << voice.locale.name() << "Available:" << voice.available;
    }
    qDebug() << "=== ModernTTSManager: Voice Scanning End ===";
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
        case TTSProvider::SystemTTS:
#ifdef QT_TEXTTOSPEECH_AVAILABLE
            qDebug() << "ModernTTSManager: Creating SystemTTS provider (native system voices)";
            return std::make_unique<SystemTTSProvider>();
#else
            qWarning() << "ModernTTSManager: SystemTTS provider not available (Qt TextToSpeech not found)";
            return nullptr;
#endif
        case TTSProvider::GoogleWeb:
            qDebug() << "ModernTTSManager: Creating GoogleWeb provider";
            return std::make_unique<GoogleWebTTSProvider>();
        case TTSProvider::EdgeTTS:
            qDebug() << "ModernTTSManager: Creating EdgeTTS provider (requires installation)";
            return std::make_unique<EdgeTTSProvider>();
        case TTSProvider::AzureCognitive:
            // Not implemented yet
            qDebug() << "ModernTTSManager: AzureCognitive not implemented";
            return nullptr;
        default:
            qDebug() << "ModernTTSManager: Unknown provider type:" << static_cast<int>(type);
            return nullptr;
    }
}

bool ModernTTSManager::isProviderAvailable(TTSProvider type) const
{
    bool available = false;
    switch (type) {
        case TTSProvider::EdgeTTS: {
            // Check if edge-tts is actually available by creating a test instance
            auto testProvider = std::make_unique<EdgeTTSProvider>();
            available = testProvider->isEdgeTTSAvailable();
            qDebug() << "ModernTTSManager: EdgeTTS provider available:" << available;
            break;
        }
        case TTSProvider::GoogleWeb:
            available = true; // Always available (web-based)
            qDebug() << "ModernTTSManager: GoogleWeb provider available:" << available;
            break;
        case TTSProvider::SystemTTS:
#ifdef QT_TEXTTOSPEECH_AVAILABLE
            available = true; // Always available when Qt TextToSpeech is found
#else
            available = false; // Qt TextToSpeech not found
#endif
            qDebug() << "ModernTTSManager: SystemTTS provider available:" << available;
            break;
        default:
            available = false;
            qDebug() << "ModernTTSManager: Unknown provider type:" << static_cast<int>(type);
            break;
    }
    return available;
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

    qDebug() << "ModernTTSManager: Loading TTS settings";
    qDebug() << "Engine:" << ttsConfig.engine;
    qDebug() << "Voice:" << ttsConfig.voice;
    qDebug() << "Volume:" << ttsConfig.volume;
    qDebug() << "Speed:" << ttsConfig.speed;

    // Convert old settings to new format
    m_defaultOptions.volume = ttsConfig.volume;
    m_defaultOptions.rate = ttsConfig.speed;
    m_defaultOptions.preferredVoiceId = ttsConfig.voice;  // Store the user's selected voice ID

    // Sanity check: if volume is suspiciously low, reset to 0.8
    if (m_defaultOptions.volume < 0.1) {
        qDebug() << "ModernTTSManager: Volume too low (" << m_defaultOptions.volume << "), resetting to 0.8";
        m_defaultOptions.volume = 0.8;
    }

    // Map old provider names to new enum
    if (ttsConfig.engine.contains("Edge") || ttsConfig.engine.contains("edge")) {
        m_defaultOptions.preferredProvider = TTSProvider::EdgeTTS;
        qDebug() << "ModernTTSManager: Selected EdgeTTS provider";
    } else if (ttsConfig.engine.contains("Google")) {
        m_defaultOptions.preferredProvider = TTSProvider::GoogleWeb;
        qDebug() << "ModernTTSManager: Selected GoogleWeb provider";
    } else {
        m_defaultOptions.preferredProvider = TTSProvider::SystemTTS;
        qDebug() << "ModernTTSManager: Selected SystemTTS provider (fallback for:" << ttsConfig.engine << ")";
    }

    qDebug() << "ModernTTSManager: Loaded settings - Provider:" << static_cast<int>(m_defaultOptions.preferredProvider)
             << "Voice ID:" << m_defaultOptions.preferredVoiceId;
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
    // Load test sentence from JSON file based on voice locale
    QString testText = "Hello, this is a voice test."; // Default fallback

    // Try to load from resources/test_sentences.json
    QString jsonPath = QCoreApplication::applicationDirPath() + "/resources/test_sentences.json";
    QFile jsonFile(jsonPath);

    if (jsonFile.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = jsonFile.readAll();
        jsonFile.close();

        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isObject()) {
            QJsonObject sentences = doc.object();

            // Try exact locale match first (e.g., "de_DE")
            QString localeKey = voice.locale.name();
            if (sentences.contains(localeKey)) {
                testText = sentences[localeKey].toString();
                qDebug() << "ModernTTSManager: Using test sentence for locale" << localeKey;
            }
            // Try language-only match (e.g., "de")
            else {
                QString langKey = voice.locale.name().left(2);
                // Find first locale starting with language code
                for (auto it = sentences.constBegin(); it != sentences.constEnd(); ++it) {
                    if (it.key().startsWith(langKey + "_")) {
                        testText = it.value().toString();
                        qDebug() << "ModernTTSManager: Using test sentence for language" << langKey << "from" << it.key();
                        break;
                    }
                }
            }
        }
    } else {
        qDebug() << "ModernTTSManager: Could not load test_sentences.json from" << jsonPath;
    }

    // Override locale for test
    TTSOptions options = m_defaultOptions;
    options.locale = voice.locale;

    qDebug() << "ModernTTSManager: Testing voice:" << voice.name << "with text:" << testText;
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

QString ModernTTSManager::generateVoiceName(const QString& voiceId, const QLocale& locale) const
{
    // Extract a human-readable name from the voice ID
    // Example: "en-US-AriaNeural" -> "Aria (English US Neural)"
    QStringList parts = voiceId.split('-');
    if (parts.size() >= 3) {
        QString language = locale.nativeLanguageName();
        if (language.isEmpty()) {
            language = QLocale::languageToString(locale.language());
        }

        QString region = locale.nativeTerritoryName();
        if (region.isEmpty()) {
            region = QLocale::territoryToString(locale.territory());
        }

        QString voiceName = parts.last().replace("Neural", "").replace("Standard", "");
        QString quality = parts.last().contains("Neural") ? "Neural" : "Standard";

        return QString("%1 (%2 %3 %4)").arg(voiceName, language, region, quality);
    }

    // Fallback: use the ID as-is with locale info
    return QString("%1 (%2)").arg(voiceId, locale.nativeLanguageName());
}

ModernTTSManager::VoiceQuality ModernTTSManager::determineVoiceQuality(const QString& voiceId) const
{
    if (voiceId.contains("Neural", Qt::CaseInsensitive) ||
        voiceId.contains("WaveNet", Qt::CaseInsensitive)) {
        return VoiceQuality::Neural;
    } else if (voiceId.contains("Standard", Qt::CaseInsensitive)) {
        return VoiceQuality::Standard;
    } else {
        return VoiceQuality::System;
    }
}

void ModernTTSManager::addEmergencyFallbackVoices()
{
    // Add minimal fallback voices for common languages to prevent total failure
    QList<QPair<QString, QLocale>> emergencyVoices = {
        {"en-US-Default", QLocale("en-US")},
        {"sv-SE-Default", QLocale("sv-SE")},
        {"system-default", QLocale::system()}
    };

    for (const auto& pair : emergencyVoices) {
        ModernTTSManager::VoiceInfo voice;
        voice.id = pair.first;
        voice.name = QString("Emergency %1").arg(pair.second.nativeLanguageName());
        voice.locale = pair.second;
        voice.quality = VoiceQuality::System;
        voice.provider = TTSProvider::SystemTTS;
        voice.available = true;
        m_availableVoices.append(voice);
    }

    qDebug() << "Added" << emergencyVoices.size() << "emergency fallback voices";
}