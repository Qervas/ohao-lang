#include "TranslationEngine.h"
#include "../ui/core/LanguageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonParseError>
#include <QHttpMultiPart>
#include <QProcess>

TranslationEngine::TranslationEngine(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &TranslationEngine::onRequestTimeout);

    m_settings = new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this);
}

TranslationEngine::~TranslationEngine()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void TranslationEngine::setEngine(Engine engine)
{
    m_engine = engine;
}

void TranslationEngine::setSourceLanguage(const QString &language)
{
    m_sourceLanguage = language;
}

void TranslationEngine::setTargetLanguage(const QString &language)
{
    m_targetLanguage = language;
}

void TranslationEngine::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void TranslationEngine::setApiUrl(const QString &url)
{
    m_apiUrl = url;
}

void TranslationEngine::translate(const QString &text)
{
    if (text.isEmpty()) {
        TranslationResult result;
        result.success = false;
        result.errorMessage = "No text to translate";
        emit translationFinished(result);
        return;
    }

    m_currentText = text;
    emit translationProgress("Starting translation...");

    // Cancel any existing request
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // Only Google Translate is supported
    translateWithGoogle(text);

    m_timeoutTimer->start(TIMEOUT_MS);
}

void TranslationEngine::translateWithGoogle(const QString &text)
{
    emit translationProgress("Connecting to Google Translate...");

    QString url = buildGoogleTranslateUrl(text);
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
}

// REMOVED: Non-Google translation engines (LibreTranslate, Ollama, Microsoft, DeepL, Offline)
// Only Google Translate is supported (free, no API key needed)

QString TranslationEngine::buildGoogleTranslateUrl(const QString &text)
{
    QString sourceLang = getLanguageCode(m_sourceLanguage, GoogleTranslate);
    QString targetLang = getLanguageCode(m_targetLanguage, GoogleTranslate);

    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", sourceLang);
    query.addQueryItem("tl", targetLang);
    query.addQueryItem("dt", "t");
    query.addQueryItem("q", text);
    url.setQuery(query);

    return url.toString();
}

void TranslationEngine::onNetworkReplyFinished()
{
    m_timeoutTimer->stop();

    if (!m_currentReply) {
        return;
    }

    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error: %1").arg(m_currentReply->errorString());
        emit translationError(errorMsg);

        TranslationResult result;
        result.success = false;
        result.errorMessage = errorMsg;
        emit translationFinished(result);

        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QByteArray response = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    // Only Google Translate is supported
    parseGoogleResponse(response);
}

void TranslationEngine::parseGoogleResponse(const QByteArray &response)
{
    TranslationResult result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);

    if (error.error != QJsonParseError::NoError) {
        result.success = false;
        result.errorMessage = "Failed to parse Google Translate response";
        emit translationFinished(result);
        return;
    }

    try {
        QJsonArray mainArray = doc.array();
        if (!mainArray.isEmpty() && mainArray[0].isArray()) {
            QJsonArray translationsArray = mainArray[0].toArray();
            QString translatedText;

            for (const auto &value : translationsArray) {
                if (value.isArray()) {
                    QJsonArray item = value.toArray();
                    if (!item.isEmpty() && item[0].isString()) {
                        translatedText += item[0].toString();
                    }
                }
            }

            result.translatedText = translatedText;
            result.sourceLanguage = m_sourceLanguage;
            result.targetLanguage = m_targetLanguage;
            result.success = true;

            emit translationProgress("Translation completed");
        } else {
            result.success = false;
            result.errorMessage = "Invalid Google Translate response format";
        }
    } catch (...) {
        result.success = false;
        result.errorMessage = "Error parsing Google Translate response";
    }

    emit translationFinished(result);
}

// REMOVED: Non-Google response parsers (LibreTranslate, Ollama, DeepL)
// Only Google Translate is supported

void TranslationEngine::onRequestTimeout()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    TranslationResult result;
    result.success = false;
    result.errorMessage = "Translation request timed out";

    emit translationError("Request timed out");
    emit translationFinished(result);
}

QString TranslationEngine::getLanguageCode(const QString &language, Engine engine)
{
    Q_UNUSED(engine); // Only Google Translate is supported

    // Special case for Auto-Detect
    if (language == "Auto-Detect") {
        return "auto";
    }

    // Get language code from LanguageManager - the single source of truth
    QString code = LanguageManager::instance().getGoogleTranslateCode(language);

    // Fallback to English if not found
    return code.isEmpty() ? "en" : code;
}

QString TranslationEngine::getLanguageName(const QString &code)
{
    QMap<QString, QString> codeToName = {
        {"en", "English"},
        {"zh-CN", "Chinese (Simplified)"},
        {"zh-TW", "Chinese (Traditional)"},
        {"ja", "Japanese"},
        {"ko", "Korean"},
        {"es", "Spanish"},
        {"fr", "French"},
        {"de", "German"},
        {"ru", "Russian"},
        {"pt", "Portuguese"},
        {"it", "Italian"},
        {"nl", "Dutch"},
        {"pl", "Polish"},
        {"ar", "Arabic"},
        {"hi", "Hindi"},
        {"th", "Thai"},
        {"vi", "Vietnamese"},
        {"sv", "Swedish"}
    };

    return codeToName.value(code, code);
}