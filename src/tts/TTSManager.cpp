#include "TTSManager.h"
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
    if (m_ttsEngine && isInputTTSEnabled() && !text.isEmpty()) {
        qDebug() << "Speaking input text:" << text.left(50);
        m_ttsEngine->speak(text, true);  // true = isInputText
    }
}

void TTSManager::speakOutputText(const QString& text)
{
    if (m_ttsEngine && isOutputTTSEnabled() && !text.isEmpty()) {
        qDebug() << "Speaking output text:" << text.left(50);
        m_ttsEngine->speak(text, false); // false = isInputText (i.e., output text)
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