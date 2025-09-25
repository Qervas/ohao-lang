#include "TTSEngine.h"
#include <QCoreApplication>
#include <QDebug>

TTSEngine::TTSEngine(QObject *parent)
    : QObject(parent)
    , m_tts(nullptr)
    , m_settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
    , m_isAvailable(false)
{
    m_backend = Backend::System; // Default backend
    // Initialize TTS engine with a concrete backend when possible
    const QStringList engines = QTextToSpeech::availableEngines();
    qDebug() << "Available TTS engines:" << engines;

#if defined(Q_OS_WIN)
    const QString preferredEngine = QStringLiteral("sapi");
#elif defined(Q_OS_MACOS)
    const QString preferredEngine = QStringLiteral("macos");
#else
    const QString preferredEngine = QStringLiteral("speechd");
#endif

    if (engines.contains(preferredEngine, Qt::CaseInsensitive)) {
        m_tts = new QTextToSpeech(preferredEngine, this);
        m_engineName = preferredEngine;
        qDebug() << "Initialized TTS with engine:" << preferredEngine;
    } else if (!engines.isEmpty()) {
        // Fall back to first available engine (may be 'mock' which only beeps)
        m_tts = new QTextToSpeech(engines.first(), this);
        m_engineName = engines.first();
        qWarning() << "Preferred TTS engine not found. Using fallback engine:" << engines.first();
    } else {
        // As a last resort, try default constructor
        m_tts = new QTextToSpeech(this);
        m_engineName.clear();
        qWarning() << "No TTS engines reported by Qt. Using default constructor.";
    }

    m_isAvailable = (m_tts && m_tts->state() != QTextToSpeech::Error);

    if (m_isAvailable) {
        const bool mockOnly = (engines.size() == 1 && engines.first().contains("mock", Qt::CaseInsensitive));
        if (mockOnly) {
            qWarning() << "Only the 'mock' TTS engine is available. You will hear tones instead of speech."
                       << "On Windows, ensure qtexttospeech_sapi plugin is deployed (windeployqt)";
        }
        // Connect signals
        connect(m_tts, &QTextToSpeech::stateChanged, this, &TTSEngine::onStateChanged);
        connect(m_tts, &QTextToSpeech::localeChanged, this, &TTSEngine::onLocaleChanged);
        connect(m_tts, &QTextToSpeech::voiceChanged, this, &TTSEngine::onVoiceChanged);

        // Cache available locales and voices
        cacheAvailableLocalesAndVoices();

    // Load settings or set defaults
        initializeDefaults();
        loadSettings();

        qDebug() << "TTS Engine initialized successfully";
        qDebug() << "Available locales:" << m_tts->availableLocales().size();
        qDebug() << "Current locale:" << m_tts->locale();
        qDebug() << "Available voices for current locale:" << m_tts->availableVoices().size();
    } else {
        qWarning() << "TTS Engine failed to initialize";
        emit errorOccurred("Text-to-Speech engine is not available on this system");
    }
}

TTSEngine::~TTSEngine()
{
    if (m_tts && m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->stop();
    }
}

void TTSEngine::speak(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (m_backend == Backend::System) {
        if (!m_isAvailable) return;
        qDebug() << "Speaking (system) text:" << text.left(50) + (text.length() > 50 ? "..." : "");
        m_tts->say(text);
        return;
    }

    if (m_backend == Backend::Azure) {
        // Use Azure cloud TTS
        if (m_azureRegion.isEmpty() || m_azureKey.isEmpty() || m_azureVoice.isEmpty()) {
            emit errorOccurred("Azure TTS not configured. Set region, key, and voice in Settings.");
            return;
        }
        m_cloud.setBackend(CloudTTSProvider::Backend::Azure);
        m_cloud.setAzureConfig(m_azureRegion, m_azureKey, m_azureVoice, m_azureStyle);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (azure) with voice" << m_azureVoice;
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::Google) {
        if (m_googleApiKey.isEmpty() || m_googleVoice.isEmpty()) { emit errorOccurred("Google TTS not configured"); return; }
        m_cloud.setBackend(CloudTTSProvider::Backend::Google);
        m_cloud.setGoogleConfig(m_googleApiKey, m_googleVoice, m_googleLanguageCode);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (google) with voice" << m_googleVoice;
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::GoogleFree) {
        m_cloud.setBackend(CloudTTSProvider::Backend::GoogleFree);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (google free)";
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::ElevenLabs) {
        if (m_elevenApiKey.isEmpty() || m_elevenVoiceId.isEmpty()) { emit errorOccurred("ElevenLabs TTS not configured"); return; }
        m_cloud.setBackend(CloudTTSProvider::Backend::ElevenLabs);
        m_cloud.setElevenLabsConfig(m_elevenApiKey, m_elevenVoiceId);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (elevenlabs) with voiceId" << m_elevenVoiceId;
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::Edge) {
        if (m_edgeExePath.isEmpty()) { emit errorOccurred("Edge TTS not configured"); return; }
        m_cloud.setBackend(CloudTTSProvider::Backend::Edge);
        m_cloud.setEdgeConfig(m_edgeExePath, m_edgeVoice);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (edge free)";
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::Polly) {
        if (m_pollyRegion.isEmpty() || m_pollyAccessKey.isEmpty() || m_pollySecretKey.isEmpty() || m_pollyVoice.isEmpty()) { emit errorOccurred("Polly TTS not configured"); return; }
        m_cloud.setBackend(CloudTTSProvider::Backend::Polly);
        m_cloud.setPollyConfig(m_pollyRegion, m_pollyAccessKey, m_pollySecretKey, m_pollyVoice);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (polly) with voice" << m_pollyVoice;
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
    if (m_backend == Backend::Piper) {
        if (m_piperExePath.isEmpty() || m_piperModelPath.isEmpty()) { emit errorOccurred("Piper not configured"); return; }
        m_cloud.setBackend(CloudTTSProvider::Backend::Piper);
        m_cloud.setPiperConfig(m_piperExePath, m_piperModelPath);
        connect(&m_cloud, &CloudTTSProvider::errorOccurred, this, &TTSEngine::errorOccurred, Qt::UniqueConnection);
        qDebug() << "Speaking (piper)";
        m_cloud.speak(text, currentLocale(), rate(), pitch(), volume());
        return;
    }
}

void TTSEngine::speak(const QString &text, bool isInputText)
{
    if (text.isEmpty()) {
        return;
    }

    // Choose appropriate voice based on input/output type
    QString targetVoice;
    if (isInputText && !m_inputVoice.isEmpty()) {
        targetVoice = m_inputVoice;
        qDebug() << "Using input voice:" << targetVoice << "for text:" << text.left(30);
    } else if (!isInputText && !m_outputVoice.isEmpty()) {
        targetVoice = m_outputVoice;
        qDebug() << "Using output voice:" << targetVoice << "for text:" << text.left(30);
    }

    // If we have a specific voice set, temporarily configure it
    QString originalVoice;
    if (!targetVoice.isEmpty()) {
        // Store current voice configuration
        switch (m_backend) {
            case Backend::Azure:
                originalVoice = m_azureVoice;
                m_azureVoice = targetVoice;
                break;
            case Backend::Google:
                originalVoice = m_googleVoice;
                m_googleVoice = targetVoice;
                break;
            case Backend::GoogleFree:
                originalVoice = m_googleFreeVoice;
                m_googleFreeVoice = targetVoice;
                break;
            case Backend::Edge:
                originalVoice = m_edgeVoice;
                m_edgeVoice = targetVoice;
                break;
            case Backend::ElevenLabs:
                originalVoice = m_elevenVoiceId;
                m_elevenVoiceId = targetVoice;
                break;
            case Backend::Polly:
                originalVoice = m_pollyVoice;
                m_pollyVoice = targetVoice;
                break;
            case Backend::System:
                // For system TTS, we'd need to find and set the voice by name
                // This is more complex, so for now just use the regular speak method
                speak(text);
                return;
            case Backend::Piper:
                // Piper uses model files, not voice names
                speak(text);
                return;
        }
    }

    // Call the regular speak method with the voice temporarily set
    speak(text);

    // Restore original voice configuration
    if (!targetVoice.isEmpty()) {
        switch (m_backend) {
            case Backend::Azure:
                m_azureVoice = originalVoice;
                break;
            case Backend::Google:
                m_googleVoice = originalVoice;
                break;
            case Backend::GoogleFree:
                m_googleFreeVoice = originalVoice;
                break;
            case Backend::Edge:
                m_edgeVoice = originalVoice;
                break;
            case Backend::ElevenLabs:
                m_elevenVoiceId = originalVoice;
                break;
            case Backend::Polly:
                m_pollyVoice = originalVoice;
                break;
            default:
                break;
        }
    }
}

void TTSEngine::stop()
{
    if (m_backend == Backend::System) {
        if (m_isAvailable) m_tts->stop();
    } else {
        m_cloud.stop();
    }
}

void TTSEngine::pause()
{
    if (m_isAvailable && m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->pause();
    }
}

void TTSEngine::resume()
{
    if (m_isAvailable && m_tts->state() == QTextToSpeech::Paused) {
        m_tts->resume();
    }
}

QStringList TTSEngine::getAvailableLanguages() const
{
    QStringList languages;
    if (!m_isAvailable) {
        return languages;
    }

    const auto locales = m_tts->availableLocales();
    for (const auto &locale : locales) {
        languages.append(getLanguageDisplayName(locale));
    }

    languages.sort();
    return languages;
}

QList<QVoice> TTSEngine::getVoicesForLanguage(const QLocale &locale) const
{
    if (!m_isAvailable) {
        return QList<QVoice>();
    }

    return m_voiceCache.value(locale.name(), QList<QVoice>());
}

QStringList TTSEngine::getVoiceNamesForLanguage(const QLocale &locale) const
{
    QStringList voiceNames;
    const auto voices = getVoicesForLanguage(locale);

    for (const auto &voice : voices) {
        QString displayName = voice.name();
        if (voice.gender() != QVoice::Unknown) {
            displayName += QString(" (%1)").arg(
                voice.gender() == QVoice::Male ? "Male" : "Female"
            );
        }
        if (voice.age() != QVoice::Other) {
            QString ageStr;
            switch (voice.age()) {
                case QVoice::Child: ageStr = "Child"; break;
                case QVoice::Teenager: ageStr = "Teenager"; break;
                case QVoice::Adult: ageStr = "Adult"; break;
                case QVoice::Senior: ageStr = "Senior"; break;
                default: break;
            }
            if (!ageStr.isEmpty()) {
                displayName += QString(" [%1]").arg(ageStr);
            }
        }
        voiceNames.append(displayName);
    }

    return voiceNames;
}

QString TTSEngine::getLanguageDisplayName(const QLocale &locale) const
{
    QString name = locale.nativeLanguageName();
    if (name.isEmpty()) {
        name = QLocale::languageToString(locale.language());
    }

    QString country = locale.nativeTerritoryName();
    if (country.isEmpty()) {
        country = QLocale::territoryToString(locale.territory());
    }

    if (!country.isEmpty() && country != name) {
        return QString("%1 (%2)").arg(name, country);
    }

    return name;
}

QLocale TTSEngine::getLocaleForLanguageName(const QString &languageName) const
{
    return m_languageToLocaleMap.value(languageName, QLocale());
}

void TTSEngine::setLocale(const QLocale &locale)
{
    if (m_isAvailable && m_tts->locale() != locale) {
        m_tts->setLocale(locale);
    }
}

void TTSEngine::setVoice(const QVoice &voice)
{
    if (m_isAvailable) {
        m_tts->setVoice(voice);
    }
}

void TTSEngine::setInputVoice(const QString &voiceName)
{
    m_inputVoice = voiceName;
    m_settings->setValue("TTS/inputVoice", voiceName);
}

void TTSEngine::setOutputVoice(const QString &voiceName)
{
    m_outputVoice = voiceName;
    m_settings->setValue("TTS/outputVoice", voiceName);
}

void TTSEngine::setVolume(double volume)
{
    if (m_isAvailable) {
        m_tts->setVolume(qBound(0.0, volume, 1.0));
    }
}

void TTSEngine::setPitch(double pitch)
{
    if (m_isAvailable) {
        m_tts->setPitch(qBound(-1.0, pitch, 1.0));
    }
}

void TTSEngine::setRate(double rate)
{
    if (m_isAvailable) {
        // Qt's normalized range is typically [-1.0, 1.0]
        m_tts->setRate(qBound(-1.0, rate, 1.0));
    }
}

QLocale TTSEngine::currentLocale() const
{
    return m_isAvailable ? m_tts->locale() : QLocale();
}

QVoice TTSEngine::currentVoice() const
{
    return m_isAvailable ? m_tts->voice() : QVoice();
}

QString TTSEngine::inputVoice() const
{
    return m_inputVoice;
}

QString TTSEngine::outputVoice() const
{
    return m_outputVoice;
}

double TTSEngine::volume() const
{
    return m_isAvailable ? m_tts->volume() : 0.0;
}

double TTSEngine::pitch() const
{
    return m_isAvailable ? m_tts->pitch() : 0.0;
}

double TTSEngine::rate() const
{
    return m_isAvailable ? m_tts->rate() : 1.0;
}

QStringList TTSEngine::availableEngines() const
{
    return QTextToSpeech::availableEngines();
}

QString TTSEngine::currentEngine() const
{
    // engine() returns empty for some backends; prefer remembered name
    if (!m_engineName.isEmpty()) return m_engineName;
    return m_tts ? m_tts->engine() : QString();
}

bool TTSEngine::isAvailable() const
{
    return m_backend == Backend::System ? m_isAvailable : true;
}

bool TTSEngine::isSpeaking() const
{
    if (m_backend == Backend::System) return m_isAvailable && m_tts->state() == QTextToSpeech::Speaking;
    return false; // Not tracked for cloud player presently
}

QTextToSpeech::State TTSEngine::state() const
{
    return m_backend == Backend::System ? (m_isAvailable ? m_tts->state() : QTextToSpeech::Error)
                                        : QTextToSpeech::Ready;
}

void TTSEngine::loadSettings()
{
    m_settings->beginGroup("TTS");

    // Backend
    const QString backendStr = m_settings->value("backend", "system").toString();
    if (backendStr.compare("azure", Qt::CaseInsensitive) == 0) m_backend = Backend::Azure;
    else if (backendStr.compare("google", Qt::CaseInsensitive) == 0) m_backend = Backend::Google;
    else if (backendStr.compare("googlefree", Qt::CaseInsensitive) == 0) m_backend = Backend::GoogleFree;
    else if (backendStr.compare("elevenlabs", Qt::CaseInsensitive) == 0) m_backend = Backend::ElevenLabs;
    else if (backendStr.compare("edge", Qt::CaseInsensitive) == 0) m_backend = Backend::Edge;
    else if (backendStr.compare("piper", Qt::CaseInsensitive) == 0) m_backend = Backend::Piper;
    else if (backendStr.compare("polly", Qt::CaseInsensitive) == 0) m_backend = Backend::Polly;
    else m_backend = Backend::System;

    // Load locale
    QString localeString = m_settings->value("locale", "").toString();
    if (!localeString.isEmpty()) {
        QLocale savedLocale(localeString);
        if (m_tts->availableLocales().contains(savedLocale)) {
            setLocale(savedLocale);
        }
    }

    // Load voice
    QString voiceString = m_settings->value("voice", "").toString();
    if (!voiceString.isEmpty()) {
        QVoice savedVoice = voiceFromString(voiceString, m_tts->locale());
        if (!savedVoice.name().isEmpty()) {
            setVoice(savedVoice);
        }
    }

    // Load audio settings
    setVolume(m_settings->value("volume", 0.8).toDouble());
    setPitch(m_settings->value("pitch", 0.0).toDouble());
    setRate(m_settings->value("rate", 1.0).toDouble());
    // Cloud settings
    m_settings->beginGroup("Azure");
    m_azureRegion = m_settings->value("region").toString();
    m_azureKey = m_settings->value("key").toString();
    m_azureVoice = m_settings->value("voice").toString();
    m_azureStyle = m_settings->value("style").toString();
    m_settings->endGroup();
    m_settings->beginGroup("Google");
    m_googleApiKey = m_settings->value("apiKey").toString();
    m_googleVoice = m_settings->value("voice").toString();
    m_googleLanguageCode = m_settings->value("languageCode").toString();
    m_settings->endGroup();
    m_settings->beginGroup("ElevenLabs");
    m_elevenApiKey = m_settings->value("apiKey").toString();
    m_elevenVoiceId = m_settings->value("voiceId").toString();
    m_settings->endGroup();
    m_settings->beginGroup("Polly");
    m_pollyRegion = m_settings->value("region").toString();
    m_pollyAccessKey = m_settings->value("accessKey").toString();
    m_pollySecretKey = m_settings->value("secretKey").toString();
    m_pollyVoice = m_settings->value("voice").toString();
    m_settings->endGroup();

    // Load GoogleFree settings
    m_settings->beginGroup("GoogleFree");
    m_googleFreeVoice = m_settings->value("voice").toString();
    m_settings->endGroup();

    // Load Edge settings
    m_settings->beginGroup("Edge");
    m_edgeExePath = m_settings->value("exe").toString();
    m_edgeVoice = m_settings->value("voice").toString();
    m_settings->endGroup();

    // Load Piper settings
    m_settings->beginGroup("Piper");
    m_piperExePath = m_settings->value("exe").toString();
    m_piperModelPath = m_settings->value("model").toString();
    m_settings->endGroup();

    m_settings->endGroup();

    // Load TTS options
    m_ttsInputEnabled = m_settings->value("general/ttsInput", false).toBool();
    m_ttsOutputEnabled = m_settings->value("general/ttsOutput", true).toBool();
    
    // Load input/output specific voices
    m_inputVoice = m_settings->value("TTS/inputVoice", "").toString();
    m_outputVoice = m_settings->value("TTS/outputVoice", "").toString();

    // Configure cloud provider with loaded settings
    m_cloud.setAzureConfig(m_azureRegion, m_azureKey, m_azureVoice, m_azureStyle);
    m_cloud.setGoogleConfig(m_googleApiKey, m_googleVoice, m_googleLanguageCode);
    m_cloud.setElevenLabsConfig(m_elevenApiKey, m_elevenVoiceId);
    m_cloud.setPollyConfig(m_pollyRegion, m_pollyAccessKey, m_pollySecretKey, m_pollyVoice);
    m_cloud.setGoogleFreeConfig(m_googleFreeVoice);
    m_cloud.setEdgeConfig(m_edgeExePath, m_edgeVoice);
    m_cloud.setPiperConfig(m_piperExePath, m_piperModelPath);

    qDebug() << "TTS settings loaded - Backend:" << (m_backend == Backend::System ? "system" : "cloud");
    if (m_backend == Backend::System) {
        qDebug() << "Locale:" << m_tts->locale() << "Voice:" << m_tts->voice().name() << "Volume:" << m_tts->volume();
    } else {
        qDebug() << "Cloud backend configured";
    }
}

void TTSEngine::saveSettings()
{
    m_settings->beginGroup("TTS");
    switch (m_backend) {
    case Backend::System: m_settings->setValue("backend", "system"); break;
    case Backend::Azure: m_settings->setValue("backend", "azure"); break;
    case Backend::Google: m_settings->setValue("backend", "google"); break;
    case Backend::GoogleFree: m_settings->setValue("backend", "googlefree"); break;
    case Backend::Edge: m_settings->setValue("backend", "edge"); break;
    case Backend::Piper: m_settings->setValue("backend", "piper"); break;
    case Backend::ElevenLabs: m_settings->setValue("backend", "elevenlabs"); break;
    case Backend::Polly: m_settings->setValue("backend", "polly"); break;
    }
    if (m_backend == Backend::System && m_isAvailable) {
        m_settings->setValue("locale", m_tts->locale().name());
        m_settings->setValue("voice", voiceToString(m_tts->voice()));
        m_settings->setValue("volume", m_tts->volume());
        m_settings->setValue("pitch", m_tts->pitch());
        m_settings->setValue("rate", m_tts->rate());
    } else {
        // Persist audio controls too (shared semantics)
        m_settings->setValue("volume", volume());
        m_settings->setValue("pitch", pitch());
        m_settings->setValue("rate", rate());
    }
    
    // Save input/output specific voices
    m_settings->setValue("inputVoice", m_inputVoice);
    m_settings->setValue("outputVoice", m_outputVoice);
    
    m_settings->endGroup();
    m_settings->sync();

    qDebug() << "TTS settings saved";
}

void TTSEngine::onStateChanged(QTextToSpeech::State state)
{
    emit stateChanged(state);

    if (state == QTextToSpeech::Error) {
        emit errorOccurred("TTS engine encountered an error");
    }
}

void TTSEngine::onLocaleChanged(const QLocale &locale)
{
    emit localeChanged(locale);
    qDebug() << "TTS locale changed to:" << locale;
}

void TTSEngine::onVoiceChanged(const QVoice &voice)
{
    emit voiceChanged(voice);
    qDebug() << "TTS voice changed to:" << voice.name();
}

void TTSEngine::initializeDefaults()
{
    // Set reasonable defaults
    setVolume(0.8);
    setPitch(0.0);
    setRate(1.0);

    if (!m_isAvailable) return;

    // Try to set a good default locale with priority for major languages
    QLocale systemLocale = QLocale::system();
    const auto availableLocales = m_tts->availableLocales();

    QList<QLocale> priorityLocales = {
        systemLocale,
        QLocale(QLocale::English, QLocale::UnitedStates),
        QLocale(QLocale::English, QLocale::UnitedKingdom),
        QLocale(QLocale::German, QLocale::Germany),
        QLocale(QLocale::French, QLocale::France),
        QLocale(QLocale::Spanish, QLocale::Spain),
        QLocale(QLocale::Chinese, QLocale::China),
        QLocale(QLocale::Swedish, QLocale::Sweden)
    };

    for (const QLocale &locale : priorityLocales) {
        if (availableLocales.contains(locale)) {
            setLocale(locale);
            qDebug() << "Set default locale to:" << locale;
            return;
        }
    }

    if (!availableLocales.isEmpty()) {
        setLocale(availableLocales.first());
        qDebug() << "Set fallback locale to:" << availableLocales.first();
    }
}

QString TTSEngine::voiceToString(const QVoice &voice) const
{
    return voice.name();
}

QVoice TTSEngine::voiceFromString(const QString &voiceString, const QLocale &locale) const
{
    if (!m_isAvailable) {
        return QVoice();
    }

    // Temporarily set locale to get voices
    QLocale currentLocale = m_tts->locale();
    m_tts->setLocale(locale);

    const auto voices = m_tts->availableVoices();
    for (const auto &voice : voices) {
        if (voice.name() == voiceString) {
            // Restore locale and return found voice
            m_tts->setLocale(currentLocale);
            return voice;
        }
    }

    // Restore locale
    m_tts->setLocale(currentLocale);
    return QVoice();
}

void TTSEngine::cacheAvailableLocalesAndVoices()
{
    if (!m_isAvailable) {
        return;
    }

    m_voiceCache.clear();
    m_languageToLocaleMap.clear();

    // Store current locale to restore later
    QLocale originalLocale = m_tts->locale();

    // Get all available locales
    const auto locales = m_tts->availableLocales();

    qDebug() << "Caching voices for" << locales.size() << "locales";

    for (const auto &locale : locales) {
        // Set locale to get its voices
        m_tts->setLocale(locale);

        // Get voices for this locale
        const auto voices = m_tts->availableVoices();
        m_voiceCache[locale.name()] = voices;

        // Map language display name to locale
        QString displayName = getLanguageDisplayName(locale);
        m_languageToLocaleMap[displayName] = locale;

        qDebug() << "Cached" << voices.size() << "voices for" << displayName;

        // Debug: List voice names for major languages
        if (locale.language() == QLocale::English ||
            locale.language() == QLocale::German ||
            locale.language() == QLocale::French ||
            locale.language() == QLocale::Spanish ||
            locale.language() == QLocale::Chinese ||
            locale.language() == QLocale::Swedish) {

            qDebug() << "  Voices for" << displayName << ":";
            for (const auto &voice : voices) {
                qDebug() << "    -" << voice.name() <<
                    (voice.gender() == QVoice::Male ? "(Male)" :
                     voice.gender() == QVoice::Female ? "(Female)" : "(Unknown)");
            }
        }
    }

    // Restore original locale
    m_tts->setLocale(originalLocale);

    qDebug() << "Voice caching complete. Total languages:" << m_languageToLocaleMap.size();
}

// Backend API
void TTSEngine::setBackend(Backend b) { m_backend = b; }
TTSEngine::Backend TTSEngine::backend() const { return m_backend; }

void TTSEngine::configureAzure(const QString& region, const QString& apiKey, const QString& voiceName, const QString& style)
{
    m_azureRegion = region;
    m_azureKey = apiKey;
    m_azureVoice = voiceName;
    m_azureStyle = style;
    m_cloud.setAzureConfig(m_azureRegion, m_azureKey, m_azureVoice, m_azureStyle);

    // Persist
    m_settings->beginGroup("TTS");
    m_settings->beginGroup("Azure");
    m_settings->setValue("region", m_azureRegion);
    m_settings->setValue("key", m_azureKey);
    m_settings->setValue("voice", m_azureVoice);
    m_settings->setValue("style", m_azureStyle);
    m_settings->endGroup();
    m_settings->endGroup();
    m_settings->sync();
}

void TTSEngine::configureGoogle(const QString& apiKey, const QString& voiceName, const QString& languageCode)
{
    m_googleApiKey = apiKey; m_googleVoice = voiceName; m_googleLanguageCode = languageCode;
    m_cloud.setGoogleConfig(m_googleApiKey, m_googleVoice, m_googleLanguageCode);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("Google");
    m_settings->setValue("apiKey", m_googleApiKey);
    m_settings->setValue("voice", m_googleVoice);
    m_settings->setValue("languageCode", m_googleLanguageCode);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

void TTSEngine::configureElevenLabs(const QString& apiKey, const QString& voiceId)
{
    m_elevenApiKey = apiKey; m_elevenVoiceId = voiceId; m_cloud.setElevenLabsConfig(m_elevenApiKey, m_elevenVoiceId);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("ElevenLabs");
    m_settings->setValue("apiKey", m_elevenApiKey);
    m_settings->setValue("voiceId", m_elevenVoiceId);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

void TTSEngine::configurePolly(const QString& region, const QString& accessKey, const QString& secretKey, const QString& voiceName)
{
    m_pollyRegion = region; m_pollyAccessKey = accessKey; m_pollySecretKey = secretKey; m_pollyVoice = voiceName;
    m_cloud.setPollyConfig(m_pollyRegion, m_pollyAccessKey, m_pollySecretKey, m_pollyVoice);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("Polly");
    m_settings->setValue("region", m_pollyRegion);
    m_settings->setValue("accessKey", m_pollyAccessKey);
    m_settings->setValue("secretKey", m_pollySecretKey);
    m_settings->setValue("voice", m_pollyVoice);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

QString TTSEngine::azureRegion() const { return m_azureRegion; }
QString TTSEngine::azureApiKey() const { return m_azureKey; }
QString TTSEngine::azureVoice() const { return m_azureVoice; }
QString TTSEngine::azureStyle() const { return m_azureStyle; }
QString TTSEngine::googleApiKey() const { return m_googleApiKey; }
QString TTSEngine::googleVoice() const { return m_googleVoice; }
QString TTSEngine::googleLanguageCode() const { return m_googleLanguageCode; }
QString TTSEngine::elevenApiKey() const { return m_elevenApiKey; }
QString TTSEngine::elevenVoiceId() const { return m_elevenVoiceId; }
QString TTSEngine::pollyRegion() const { return m_pollyRegion; }
QString TTSEngine::pollyAccessKey() const { return m_pollyAccessKey; }
QString TTSEngine::pollySecretKey() const { return m_pollySecretKey; }
QString TTSEngine::pollyVoice() const { return m_pollyVoice; }
QString TTSEngine::edgeVoice() const { return m_edgeVoice; }
QString TTSEngine::piperExePath() const { return m_piperExePath; }
QString TTSEngine::piperModelPath() const { return m_piperModelPath; }

void TTSEngine::configureEdge(const QString& exePath, const QString& voiceName)
{
    m_edgeExePath = exePath; m_edgeVoice = voiceName;
    m_cloud.setEdgeConfig(m_edgeExePath, m_edgeVoice);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("Edge");
    m_settings->setValue("exe", m_edgeExePath);
    m_settings->setValue("voice", m_edgeVoice);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

void TTSEngine::configurePiper(const QString& exePath, const QString& modelPath)
{
    m_piperExePath = exePath; m_piperModelPath = modelPath;
    m_cloud.setPiperConfig(m_piperExePath, m_piperModelPath);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("Piper");
    m_settings->setValue("exe", m_piperExePath);
    m_settings->setValue("model", m_piperModelPath);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

void TTSEngine::configureGoogleFree(const QString& voiceName)
{
    m_googleFreeVoice = voiceName;
    m_cloud.setGoogleFreeConfig(m_googleFreeVoice);
    m_settings->beginGroup("TTS"); m_settings->beginGroup("GoogleFree");
    m_settings->setValue("voice", m_googleFreeVoice);
    m_settings->endGroup(); m_settings->endGroup(); m_settings->sync();
}

bool TTSEngine::isTTSInputEnabled() const { return m_ttsInputEnabled; }
bool TTSEngine::isTTSOutputEnabled() const { return m_ttsOutputEnabled; }

void TTSEngine::setTTSInputEnabled(bool enabled) 
{ 
    m_ttsInputEnabled = enabled; 
    m_settings->setValue("general/ttsInput", enabled);
}

void TTSEngine::setTTSOutputEnabled(bool enabled) 
{ 
    m_ttsOutputEnabled = enabled; 
    m_settings->setValue("general/ttsOutput", enabled);
}