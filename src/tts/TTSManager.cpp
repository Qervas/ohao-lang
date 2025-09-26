#include "TTSManager.h"
#include "../core/LanguageManager.h"
#include <QDebug>

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
    if (m_ttsEngine && isInputTTSEnabled() && !text.isEmpty()) {
        QLocale locale = languageCode.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(languageCode);
        qDebug() << "Speaking input text:" << text.left(50) << "with locale:" << locale.name();
        m_ttsEngine->speak(text, true, locale);  // true = isInputText
    }
}

void TTSManager::speakOutputText(const QString& text)
{
    speakOutputText(text, QString());
}

void TTSManager::speakOutputText(const QString& text, const QString& languageCode)
{
    if (m_ttsEngine && isOutputTTSEnabled() && !text.isEmpty()) {
        QLocale locale = languageCode.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(languageCode);
        qDebug() << "Speaking output text:" << text.left(50) << "with locale:" << locale.name();
        m_ttsEngine->speak(text, false, locale); // false = isInputText (i.e., output text)
    }
}

bool TTSManager::isInputTTSEnabled() const
{
    return m_ttsEngine ? m_ttsEngine->isTTSInputEnabled() : false;
}

bool TTSManager::isOutputTTSEnabled() const
{
    return m_ttsEngine ? m_ttsEngine->isTTSOutputEnabled() : false;
}