#include "TranslationEngine.h"
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

    switch (m_engine) {
    case GoogleTranslate:
        translateWithGoogle(text);
        break;
    case LibreTranslate:
        translateWithLibreTranslate(text);
        break;
    case OllamaLLM:
        translateWithOllama(text);
        break;
    case MicrosoftTranslator:
        translateWithMicrosoft(text);
        break;
    case DeepL:
        translateWithDeepL(text);
        break;
    case OfflineDictionary:
        translateOffline(text);
        break;
    }

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

void TranslationEngine::translateWithLibreTranslate(const QString &text)
{
    emit translationProgress("Connecting to LibreTranslate...");

    QString apiUrl = m_apiUrl.isEmpty() ? "https://libretranslate.de/translate" : m_apiUrl;

    QNetworkRequest request{QUrl(apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    QJsonObject json;
    json["q"] = text;
    json["source"] = getLanguageCode(m_sourceLanguage, LibreTranslate);
    json["target"] = getLanguageCode(m_targetLanguage, LibreTranslate);
    json["format"] = "text";

    QJsonDocument doc(json);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
}

void TranslationEngine::translateWithOllama(const QString &text)
{
    emit translationProgress("Connecting to Ollama LLM...");

    QString apiUrl = m_apiUrl.isEmpty() ? "http://localhost:11434/api/generate" : m_apiUrl;

    QNetworkRequest request{QUrl(apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString prompt = QString("Translate the following text from %1 to %2. Only provide the translation, no explanations:\n\n%3")
                        .arg(m_sourceLanguage == "Auto-Detect" ? "any language" : m_sourceLanguage)
                        .arg(m_targetLanguage)
                        .arg(text);

    QJsonObject json;
    json["model"] = "llama2"; // Default model, can be configured
    json["prompt"] = prompt;
    json["stream"] = false;

    QJsonDocument doc(json);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
}

void TranslationEngine::translateWithMicrosoft(const QString &text)
{
    emit translationProgress("Microsoft Translator not yet implemented");

    TranslationResult result;
    result.success = false;
    result.errorMessage = "Microsoft Translator integration coming soon";
    emit translationFinished(result);
}

void TranslationEngine::translateWithDeepL(const QString &text)
{
    emit translationProgress("Connecting to DeepL...");

    QString apiUrl = m_apiUrl.isEmpty() ? "https://api-free.deepl.com/v2/translate" : m_apiUrl;

    if (m_apiKey.isEmpty()) {
        TranslationResult result;
        result.success = false;
        result.errorMessage = "DeepL requires an API key";
        emit translationFinished(result);
        return;
    }

    QNetworkRequest request{QUrl(apiUrl)};
    request.setRawHeader("Authorization", QString("DeepL-Auth-Key %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("text", text);
    params.addQueryItem("target_lang", getLanguageCode(m_targetLanguage, DeepL));

    if (m_sourceLanguage != "Auto-Detect") {
        params.addQueryItem("source_lang", getLanguageCode(m_sourceLanguage, DeepL));
    }

    m_currentReply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
}

void TranslationEngine::translateOffline(const QString &text)
{
    emit translationProgress("Offline dictionary not yet implemented");

    TranslationResult result;
    result.success = false;
    result.errorMessage = "Offline dictionary coming soon";
    emit translationFinished(result);
}

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

    switch (m_engine) {
    case GoogleTranslate:
        parseGoogleResponse(response);
        break;
    case LibreTranslate:
        parseLibreTranslateResponse(response);
        break;
    case OllamaLLM:
        parseOllamaResponse(response);
        break;
    case DeepL:
        parseDeepLResponse(response);
        break;
    default:
        break;
    }
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

void TranslationEngine::parseLibreTranslateResponse(const QByteArray &response)
{
    TranslationResult result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);

    if (error.error != QJsonParseError::NoError) {
        result.success = false;
        result.errorMessage = "Failed to parse LibreTranslate response";
        emit translationFinished(result);
        return;
    }

    QJsonObject json = doc.object();

    if (json.contains("translatedText")) {
        result.translatedText = json["translatedText"].toString();
        result.sourceLanguage = m_sourceLanguage;
        result.targetLanguage = m_targetLanguage;
        result.success = true;
        emit translationProgress("Translation completed");
    } else if (json.contains("error")) {
        result.success = false;
        result.errorMessage = json["error"].toString();
    } else {
        result.success = false;
        result.errorMessage = "Invalid LibreTranslate response";
    }

    emit translationFinished(result);
}

void TranslationEngine::parseOllamaResponse(const QByteArray &response)
{
    TranslationResult result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);

    if (error.error != QJsonParseError::NoError) {
        result.success = false;
        result.errorMessage = "Failed to parse Ollama response";
        emit translationFinished(result);
        return;
    }

    QJsonObject json = doc.object();

    if (json.contains("response")) {
        result.translatedText = json["response"].toString().trimmed();
        result.sourceLanguage = m_sourceLanguage;
        result.targetLanguage = m_targetLanguage;
        result.success = true;
        emit translationProgress("Translation completed");
    } else if (json.contains("error")) {
        result.success = false;
        result.errorMessage = json["error"].toString();
    } else {
        result.success = false;
        result.errorMessage = "Invalid Ollama response";
    }

    emit translationFinished(result);
}

void TranslationEngine::parseDeepLResponse(const QByteArray &response)
{
    TranslationResult result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);

    if (error.error != QJsonParseError::NoError) {
        result.success = false;
        result.errorMessage = "Failed to parse DeepL response";
        emit translationFinished(result);
        return;
    }

    QJsonObject json = doc.object();

    if (json.contains("translations")) {
        QJsonArray translations = json["translations"].toArray();
        if (!translations.isEmpty()) {
            QJsonObject translation = translations[0].toObject();
            result.translatedText = translation["text"].toString();
            result.sourceLanguage = translation["detected_source_language"].toString();
            result.targetLanguage = m_targetLanguage;
            result.success = true;
            emit translationProgress("Translation completed");
        }
    } else if (json.contains("message")) {
        result.success = false;
        result.errorMessage = json["message"].toString();
    } else {
        result.success = false;
        result.errorMessage = "Invalid DeepL response";
    }

    emit translationFinished(result);
}

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
    // Language code mapping for different services
    QMap<QString, QString> googleCodes = {
        {"Auto-Detect", "auto"},
        {"English", "en"},
        {"Chinese (Simplified)", "zh-CN"},
        {"Chinese (Traditional)", "zh-TW"},
        {"Japanese", "ja"},
        {"Korean", "ko"},
        {"Spanish", "es"},
        {"French", "fr"},
        {"German", "de"},
        {"Russian", "ru"},
        {"Portuguese", "pt"},
        {"Italian", "it"},
        {"Dutch", "nl"},
        {"Polish", "pl"},
        {"Arabic", "ar"},
        {"Hindi", "hi"},
        {"Thai", "th"},
        {"Vietnamese", "vi"},
        {"Swedish", "sv"}
    };

    QMap<QString, QString> deeplCodes = {
        {"English", "EN"},
        {"Chinese (Simplified)", "ZH"},
        {"Japanese", "JA"},
        {"Spanish", "ES"},
        {"French", "FR"},
        {"German", "DE"},
        {"Russian", "RU"},
        {"Portuguese", "PT"},
        {"Italian", "IT"},
        {"Dutch", "NL"},
        {"Polish", "PL"},
        {"Swedish", "SV"}
    };

    if (engine == GoogleTranslate || engine == LibreTranslate || engine == OllamaLLM) {
        return googleCodes.value(language, "en");
    } else if (engine == DeepL) {
        return deeplCodes.value(language, "EN");
    }

    return googleCodes.value(language, "en");
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