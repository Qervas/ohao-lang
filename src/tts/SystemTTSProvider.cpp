#include "SystemTTSProvider.h"
#include <QDebug>

SystemTTSProvider::SystemTTSProvider(QObject* parent)
    : TTSProvider(parent)
    , m_initialized(false)
{
    qDebug() << "SystemTTSProvider: Initializing";
    initializeEngine();
}

SystemTTSProvider::~SystemTTSProvider()
{
    if (m_engine) {
        m_engine->stop();
    }
    qDebug() << "SystemTTSProvider: Destroyed";
}

void SystemTTSProvider::initializeEngine()
{
    try {
        m_engine = std::make_unique<QTextToSpeech>(this);
        
        if (m_engine) {
            connect(m_engine.get(), &QTextToSpeech::stateChanged,
                    this, &SystemTTSProvider::onStateChanged);
            
            m_initialized = true;
            
            // List available engines and voices
            qDebug() << "SystemTTSProvider: Available engines:" << m_engine->availableEngines();
            qDebug() << "SystemTTSProvider: Available locales:" << m_engine->availableLocales();
            qDebug() << "SystemTTSProvider: Total voices:" << m_engine->availableVoices().size();
            
            // Show voices for major languages
            for (const QLocale& locale : m_engine->availableLocales()) {
                QList<QVoice> voices = m_engine->availableVoices();
                if (!voices.isEmpty()) {
                    qDebug() << "SystemTTSProvider: Locale" << locale.name() 
                             << "has" << voices.size() << "voices";
                }
            }
        } else {
            qWarning() << "SystemTTSProvider: Failed to create QTextToSpeech engine";
        }
    } catch (const std::exception& e) {
        qWarning() << "SystemTTSProvider: Exception during initialization:" << e.what();
        m_initialized = false;
    }
}

void SystemTTSProvider::speak(const QString& text, const QLocale& locale, double rate, double pitch, double volume)
{
    if (!m_engine || !m_initialized) {
        qWarning() << "SystemTTSProvider: Engine not initialized";
        emit errorOccurred("System TTS engine not available");
        return;
    }

    if (text.isEmpty()) {
        qWarning() << "SystemTTSProvider: Empty text provided";
        return;
    }

    qDebug() << "SystemTTSProvider: Speaking text:" << text.left(50);
    qDebug() << "SystemTTSProvider: Config voice:" << m_config.voice;
    
    // Apply volume, rate, and pitch
    m_engine->setVolume(volume);
    m_engine->setRate(rate);
    m_engine->setPitch(pitch);
    
    // Re-apply the voice from config to ensure it's set correctly
    // This is necessary because the voice state might have been changed elsewhere
    if (!m_config.voice.isEmpty()) {
        qDebug() << "SystemTTSProvider: Re-applying voice from config:" << m_config.voice;
        QVoice voice = findVoiceByName(m_config.voice);
        if (!voice.name().isEmpty()) {
            m_engine->setVoice(voice);
            qDebug() << "SystemTTSProvider: Voice set to:" << voice.name();
        } else {
            qWarning() << "SystemTTSProvider: Could not find voice:" << m_config.voice;
        }
    }
    
    qDebug() << "SystemTTSProvider: Final voice:" << m_engine->voice().name();
    qDebug() << "SystemTTSProvider: Volume:" << volume << "Rate:" << rate << "Pitch:" << pitch;
    
    emit started();
    m_engine->say(text);
}

void SystemTTSProvider::stop()
{
    if (m_engine) {
        qDebug() << "SystemTTSProvider: Stopping speech";
        m_engine->stop();
    }
}

void SystemTTSProvider::pause()
{
    if (m_engine) {
        qDebug() << "SystemTTSProvider: Pausing speech";
        m_engine->pause();
    }
}

void SystemTTSProvider::resume()
{
    if (m_engine) {
        qDebug() << "SystemTTSProvider: Resuming speech";
        m_engine->resume();
    }
}

bool SystemTTSProvider::isAvailable() const
{
    return m_initialized && m_engine != nullptr;
}

bool SystemTTSProvider::isSpeaking() const
{
    if (!m_engine) {
        return false;
    }
    return m_engine->state() == QTextToSpeech::Speaking;
}

void SystemTTSProvider::applyConfig(const Config& config)
{
    if (!m_engine || !m_initialized) {
        qWarning() << "SystemTTSProvider: Cannot apply config, engine not initialized";
        return;
    }

    m_config = config;
    
    qDebug() << "SystemTTSProvider: Applying config:";
    qDebug() << "  Language:" << config.languageCode;
    qDebug() << "  Voice:" << config.voice;

    // If voice name is provided, try to find and set it directly
    if (!config.voice.isEmpty()) {
        qDebug() << "SystemTTSProvider: Searching for voice:" << config.voice;
        QVoice voice = findVoiceByName(config.voice);
        if (!voice.name().isEmpty()) {
            qDebug() << "SystemTTSProvider: Found voice, setting to:" << voice.name();
            m_engine->setVoice(voice);
            
            // Verify it was actually set
            QVoice currentVoice = m_engine->voice();
            qDebug() << "SystemTTSProvider: Current voice after setting:" << currentVoice.name();
            
            if (currentVoice.name() != voice.name()) {
                qWarning() << "SystemTTSProvider: WARNING! Voice was not set correctly!";
                qWarning() << "  Requested:" << voice.name() << "Got:" << currentVoice.name();
            }
            
            qDebug() << "SystemTTSProvider: Config applied successfully with voice:" << voice.name();
            return;
        } else {
            qWarning() << "SystemTTSProvider: Voice not found:" << config.voice;
        }
    }

    // Fallback: use locale-based selection
    QLocale locale(config.languageCode);
    if (!config.languageCode.isEmpty()) {
        qDebug() << "SystemTTSProvider: Setting locale to:" << locale.name();
        m_engine->setLocale(locale);
    }

    // Find and set the best voice for this locale
    QVoice voice = findBestVoice(locale, config.voice);
    qDebug() << "SystemTTSProvider: Selected voice:" << voice.name() 
             << "for locale:" << locale.name();
    m_engine->setVoice(voice);
    
    qDebug() << "SystemTTSProvider: Config applied successfully";
}

QStringList SystemTTSProvider::suggestedVoicesFor(const QLocale& locale) const
{
    if (!m_engine || !m_initialized) {
        return QStringList();
    }

    QStringList suggestions;
    
    // Save current locale
    QLocale currentLocale = m_engine->locale();
    
    // Temporarily set to requested locale to get its voices
    m_engine->setLocale(locale);
    QList<QVoice> voices = m_engine->availableVoices();
    
    // Restore original locale
    m_engine->setLocale(currentLocale);
    
    qDebug() << "SystemTTSProvider: Found" << voices.size() 
             << "voices for locale:" << locale.name();
    
    for (const QVoice& voice : voices) {
        suggestions.append(voice.name());
    }
    
    return suggestions;
}

QVoice SystemTTSProvider::findVoiceByName(const QString& voiceName) const
{
    if (!m_engine || voiceName.isEmpty()) {
        return QVoice();
    }

    // Search through all available locales and voices
    for (const QLocale& locale : m_engine->availableLocales()) {
        // Save current locale
        QLocale currentLocale = m_engine->locale();
        
        // Set to this locale
        m_engine->setLocale(locale);
        QList<QVoice> voices = m_engine->availableVoices();
        
        // Restore locale
        m_engine->setLocale(currentLocale);
        
        // Search for exact or partial match
        for (const QVoice& voice : voices) {
            if (voice.name() == voiceName) {
                qDebug() << "SystemTTSProvider: Found exact voice match:" << voice.name();
                return voice;
            }
        }
        
        // Try partial match
        for (const QVoice& voice : voices) {
            if (voice.name().contains(voiceName, Qt::CaseInsensitive)) {
                qDebug() << "SystemTTSProvider: Found partial voice match:" << voice.name();
                return voice;
            }
        }
    }
    
    qWarning() << "SystemTTSProvider: Voice not found:" << voiceName;
    return QVoice();
}

QVoice SystemTTSProvider::findBestVoice(const QLocale& locale, const QString& voiceId) const
{
    if (!m_engine) {
        return QVoice();
    }

    // Save current locale
    QLocale currentLocale = m_engine->locale();
    
    // Set to requested locale
    m_engine->setLocale(locale);
    QList<QVoice> voices = m_engine->availableVoices();
    
    // Restore locale
    m_engine->setLocale(currentLocale);
    
    if (voices.isEmpty()) {
        qWarning() << "SystemTTSProvider: No voices available for locale:" << locale.name();
        // Try to get any voice as fallback
        voices = m_engine->availableVoices();
        if (voices.isEmpty()) {
            qWarning() << "SystemTTSProvider: No voices available at all!";
            return QVoice();
        }
    }

    // If specific voice requested, try to find it
    if (!voiceId.isEmpty()) {
        for (const QVoice& voice : voices) {
            if (voice.name().contains(voiceId, Qt::CaseInsensitive)) {
                qDebug() << "SystemTTSProvider: Found requested voice:" << voice.name();
                return voice;
            }
        }
        qDebug() << "SystemTTSProvider: Requested voice not found:" << voiceId;
    }

    // Return first available voice as fallback
    qDebug() << "SystemTTSProvider: Using first available voice:" << voices.first().name();
    return voices.first();
}

void SystemTTSProvider::onStateChanged(QTextToSpeech::State state)
{
    qDebug() << "SystemTTSProvider: State changed to:" << state;
    
    switch (state) {
        case QTextToSpeech::Ready:
            if (m_engine && m_engine->errorString().isEmpty()) {
                // Only emit finished if there's no error
                emit finished();
            }
            break;
            
        case QTextToSpeech::Speaking:
            // Already emitted started() before calling say()
            break;
            
        case QTextToSpeech::Synthesizing:
            // Speech is being prepared
            break;
            
        case QTextToSpeech::Paused:
            // No specific signal for paused
            break;
            
        case QTextToSpeech::Error:
            qWarning() << "SystemTTSProvider: Error occurred:" << m_engine->errorString();
            emit errorOccurred(m_engine->errorString());
            break;
    }
}
