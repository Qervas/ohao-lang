#include "TTSManager.h"
#include "../core/LanguageManager.h"
#include <QDebug>
#include <QSettings>
#include <QCoreApplication>

TTSManager* TTSManager::s_instance = nullptr;

TTSManager::TTSManager(QObject* parent)
    : QObject(parent)
    , m_ttsEngine(nullptr)
{
    m_ttsEngine = new TTSEngine(this);
    qDebug() << "TTSManager initialized";
}

TTSManager::~TTSManager()
{
    qDebug() << "TTSManager destroyed";
}

TTSManager& TTSManager::instance()
{
    if (!s_instance) {
        s_instance = new TTSManager();
    }
    return *s_instance;
}

TTSEngine* TTSManager::ttsEngine()
{
    return m_ttsEngine;
}

void TTSManager::speakInputText(const QString& text)
{
    speakInputText(text, QString());
}

void TTSManager::speakInputText(const QString& text, const QString& languageCode)
{
    qDebug() << "TTSManager::speakInputText called with text:" << text.left(50) << "languageCode:" << languageCode;
    qDebug() << "Engine exists:" << (m_ttsEngine != nullptr) << "Input TTS enabled:" << isInputTTSEnabled() << "Text empty:" << text.isEmpty();

    if (m_ttsEngine && isInputTTSEnabled() && !text.isEmpty()) {
        qDebug() << "✓ All checks passed, configuring TTS...";
        configureFromSettings();  // Ensure TTS is properly configured
        QLocale locale = languageCode.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(languageCode);
        qDebug() << "✓ Speaking input text:" << text.left(50) << "with locale:" << locale.name();
        m_ttsEngine->speak(text, true, locale);  // true = isInputText
        qDebug() << "✓ TTS engine speak() called";
    } else {
        qDebug() << "✗ TTS Input blocked - Engine:" << (m_ttsEngine != nullptr)
                 << "Enabled:" << isInputTTSEnabled() << "Text empty:" << text.isEmpty();
    }
}

void TTSManager::speakOutputText(const QString& text)
{
    speakOutputText(text, QString());
}

void TTSManager::speakOutputText(const QString& text, const QString& languageCode)
{
    if (m_ttsEngine && isOutputTTSEnabled() && !text.isEmpty()) {
        configureFromSettings();  // Ensure TTS is properly configured
        QLocale locale = languageCode.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(languageCode);
        qDebug() << "Speaking output text:" << text.left(50) << "with locale:" << locale.name();
        m_ttsEngine->speak(text, false, locale); // false = isInputText (i.e., output text)
    }
}

bool TTSManager::isInputTTSEnabled() const
{
    bool enabled = m_ttsEngine ? m_ttsEngine->isTTSInputEnabled() : false;
    qDebug() << "TTSManager::isInputTTSEnabled() returning:" << enabled << "Engine exists:" << (m_ttsEngine != nullptr);
    return enabled;
}

bool TTSManager::isOutputTTSEnabled() const
{
    return m_ttsEngine ? m_ttsEngine->isTTSOutputEnabled() : false;
}

void TTSManager::configureFromSettings()
{
    if (!m_ttsEngine) {
        qDebug() << "✗ TTSManager: No TTS engine available for configuration";
        return;
    }

    // Use the unified configuration method from TTSEngine
    qDebug() << "✓ TTSManager: Delegating to TTSEngine::configureFromCurrentSettings()";
    m_ttsEngine->configureFromCurrentSettings();
    qDebug() << "✓ TTSManager: Configuration delegation complete";
}