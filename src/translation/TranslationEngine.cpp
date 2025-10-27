#include "TranslationEngine.h"
#include "../ui/core/LanguageManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonParseError>
#include <QHttpMultiPart>
#include <QProcess>
#include <QRegularExpression>

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

    // Reset aggregation/chunk state
    m_aggregatedText.clear();
    m_chunks = chunkTextByLimit(text, CHUNK_CHAR_LIMIT);
    m_currentChunkIndex = -1;
    startNextChunk();
}

void TranslationEngine::translateWithGoogle(const QString &text)
{
    emit translationProgress("Connecting to Google Translate...");

    QString url = buildGoogleTranslateUrl(text);
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Referer", "https://translate.google.com/");

    // Cancel any existing reply
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
    m_timeoutTimer->start(TIMEOUT_MS);
}

// REMOVED: Non-Google translation engines (LibreTranslate, Ollama, Microsoft, DeepL, Offline)
// Only Google Translate is supported (free, no API key needed)

QString TranslationEngine::buildGoogleTranslateUrl(const QString &text)
{
    QString sourceLang = getLanguageCode(m_sourceLanguage, GoogleTranslate);
    QString targetLang = getLanguageCode(m_targetLanguage, GoogleTranslate);

    qDebug() << "TranslationEngine: Building Google Translate URL";
    qDebug() << "  Source language (display):" << m_sourceLanguage << "-> code:" << sourceLang;
    qDebug() << "  Target language (display):" << m_targetLanguage << "-> code:" << targetLang;
    qDebug() << "  Text to translate:" << text.left(50);

    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", sourceLang);
    query.addQueryItem("tl", targetLang);
    query.addQueryItem("dt", "t");
    query.addQueryItem("hl", "en");
    query.addQueryItem("ie", "UTF-8");
    query.addQueryItem("oe", "UTF-8");
    query.addQueryItem("dj", "1");          // prefer object format for robustness
    query.addQueryItem("source", "input");   // better handling for multi-script
    query.addQueryItem("q", text);
    url.setQuery(query);

    return url.toString();
}

QString TranslationEngine::buildGoogleTranslateUrl(const QString &text, bool forceAutoDetect)
{
    QString src = forceAutoDetect ? QStringLiteral("auto")
                                  : getLanguageCode(m_sourceLanguage, GoogleTranslate);
    QString tgt = getLanguageCode(m_targetLanguage, GoogleTranslate);
    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", src);
    query.addQueryItem("tl", tgt);
    query.addQueryItem("dt", "t");
    query.addQueryItem("hl", "en");
    query.addQueryItem("ie", "UTF-8");
    query.addQueryItem("oe", "UTF-8");
    query.addQueryItem("dj", "1");
    query.addQueryItem("source", "input");
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

    auto cleanupReply = [this]() {
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
    };

    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error: %1").arg(m_currentReply->errorString());
        qDebug() << errorMsg;
        cleanupReply();

        if (m_retryAttempt < MAX_RETRIES) {
            int delay = BACKOFF_BASE_MS * (1 << m_retryAttempt);
            m_retryAttempt++;
            emit translationProgress(QString("Retrying... (%1/%2)").arg(m_retryAttempt).arg(MAX_RETRIES));
            QTimer::singleShot(delay, this, [this]() {
                // If we detect right-to-left or Cyrillic content, force auto-detect on retry
                if (!m_chunks.isEmpty()) {
                    QString chunk = m_chunks.value(m_currentChunkIndex);
                    // Use QRegularExpression (Qt 6)
                    QRegularExpression arabicRe(QStringLiteral("[\u0600-\u06FF]"));
                    QRegularExpression cyrillicRe(QStringLiteral("[\u0400-\u04FF]"));
                    bool rtlOrCyr = arabicRe.match(chunk).hasMatch() || cyrillicRe.match(chunk).hasMatch();
                    if (rtlOrCyr) {
                        m_forceAutoDetect = true;
                    }
                }
                sendRequestForText(m_chunks.value(m_currentChunkIndex));
            });
            return;
        }

        emit translationError(errorMsg);
        TranslationResult result;
        result.success = false;
        result.errorMessage = errorMsg;
        emit translationFinished(result);
        return;
    }

    QByteArray response = m_currentReply->readAll();
    cleanupReply();

    // Only Google Translate is supported
    parseGoogleResponse(response);
}

void TranslationEngine::parseGoogleResponse(const QByteArray &response)
{
    QString chunkText;
    if (!parseGoogleResponseText(response, chunkText)) {
        if (m_retryAttempt < MAX_RETRIES) {
            int delay = BACKOFF_BASE_MS * (1 << m_retryAttempt);
            m_retryAttempt++;
            emit translationProgress(QString("Retrying parse... (%1/%2)").arg(m_retryAttempt).arg(MAX_RETRIES));
            QTimer::singleShot(delay, this, [this]() {
                sendRequestForText(m_chunks.value(m_currentChunkIndex));
            });
            return;
        }

        TranslationResult result;
        result.success = false;
        result.errorMessage = "Failed to parse Google Translate response";
        emit translationError(result.errorMessage);
        emit translationFinished(result);
        return;
    }

    m_aggregatedText += chunkText;

    // Move to next chunk or finish
    startNextChunk();
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

    if (m_retryAttempt < MAX_RETRIES) {
        int delay = BACKOFF_BASE_MS * (1 << m_retryAttempt);
        m_retryAttempt++;
        emit translationProgress(QString("Retrying after timeout... (%1/%2)").arg(m_retryAttempt).arg(MAX_RETRIES));
        QTimer::singleShot(delay, this, [this]() {
            sendRequestForText(m_chunks.value(m_currentChunkIndex));
        });
        return;
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

// --- helpers ---

QStringList TranslationEngine::chunkTextByLimit(const QString &text, int limit) const
{
    if (text.size() <= limit) {
        return {text};
    }

    // Greedy grouping of logical lines to minimize number of chunks
    QStringList lines = text.split('\n');
    QStringList chunks;
    QString current;
    for (int i = 0; i < lines.size(); ++i) {
        QString candidate = current.isEmpty() ? lines[i] : current + '\n' + lines[i];
        if (candidate.size() <= limit) {
            current = candidate;
        } else {
            if (!current.isEmpty()) {
                chunks << current;
            }
            // If a single line is over limit, hard-split that line
            if (lines[i].size() > limit) {
                int start = 0;
                while (start < lines[i].size()) {
                    int len = qMin(limit, lines[i].size() - start);
                    chunks << lines[i].mid(start, len);
                    start += len;
                }
                current.clear();
            } else {
                current = lines[i];
            }
        }
    }
    if (!current.isEmpty()) {
        chunks << current;
    }
    return chunks;
}

void TranslationEngine::startNextChunk()
{
    m_retryAttempt = 0;
    m_currentChunkIndex++;
    if (m_currentChunkIndex >= 0 && m_currentChunkIndex < m_chunks.size()) {
        QString status = QString("Translating chunk %1/%2...")
                              .arg(m_currentChunkIndex + 1)
                              .arg(m_chunks.size());
        emit translationProgress(status);
        sendRequestForText(m_chunks[m_currentChunkIndex]);
        return;
    }

    // Finished all chunks
    TranslationResult result;
    result.translatedText = m_aggregatedText;
    result.sourceLanguage = m_sourceLanguage;
    result.targetLanguage = m_targetLanguage;
    result.success = true;
    emit translationProgress("Translation completed");
    emit translationFinished(result);
}

void TranslationEngine::sendRequestForText(const QString &text)
{
    if (m_forceAutoDetect) {
        QString url = buildGoogleTranslateUrl(text, true);
        QNetworkRequest request{QUrl(url)};
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        request.setRawHeader("Accept", "application/json, text/plain, */*");
        request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
        request.setRawHeader("Cache-Control", "no-cache");
        request.setRawHeader("Pragma", "no-cache");
        request.setRawHeader("Referer", "https://translate.google.com/");
        if (m_currentReply) {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        m_currentReply = m_networkManager->get(request);
        connect(m_currentReply, &QNetworkReply::finished, this, &TranslationEngine::onNetworkReplyFinished);
        m_timeoutTimer->start(TIMEOUT_MS);
        return;
    }
    translateWithGoogle(text);
}

bool TranslationEngine::parseGoogleResponseText(const QByteArray &response, QString &outText)
{
    // Strip potential XSSI prefix ")]}\'\n"
    QByteArray body = response.trimmed();
    if (body.startsWith(")]}\'")) {
        int nl = body.indexOf('\n');
        if (nl >= 0) {
            body = body.mid(nl + 1);
        }
    }

    qDebug() << "Raw Google Translate response (first 500 chars):" << QString::fromUtf8(body.left(500));

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    // Format 1: Array-based response
    if (doc.isArray()) {
        try {
            QJsonArray mainArray = doc.array();
            if (!mainArray.isEmpty() && mainArray[0].isArray()) {
                QJsonArray translationsArray = mainArray[0].toArray();
                QString translatedText;
                for (int i = 0; i < translationsArray.size(); i++) {
                    const auto &value = translationsArray[i];
                    if (value.isArray()) {
                        QJsonArray item = value.toArray();
                        if (!item.isEmpty() && item[0].isString()) {
                            translatedText += item[0].toString();
                        }
                    }
                }
                outText = translatedText;
                if (!outText.isEmpty()) return true;
            }
        } catch (...) {
            // fall through to alternate format
        }
    }

    // Format 2: Object with sentences array (dj=1 style)
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("sentences") && obj["sentences"].isArray()) {
            QJsonArray sentences = obj["sentences"].toArray();
            QString translatedText;
            for (const auto &s : sentences) {
                if (s.isObject()) {
                    QJsonObject so = s.toObject();
                    if (so.contains("trans") && so["trans"].isString()) {
                        translatedText += so["trans"].toString();
                    }
                }
            }
            outText = translatedText;
            return !outText.isEmpty();
        }
    }

    return false;
}