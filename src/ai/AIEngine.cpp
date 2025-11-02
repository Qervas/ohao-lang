#include "AIEngine.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

AIEngine::AIEngine(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_provider("GitHub Copilot")
    , m_apiUrl("http://localhost:4141")
    , m_apiKey("")
    , m_model("gpt-4")
    , m_temperature(0.7f)
    , m_maxTokens(2000)
    , m_systemPrompt("You are a helpful translation and language learning assistant.")
    , m_processing(false)
    , m_available(false)
    , m_connectionCheckTimer(new QTimer(this))
{
    qDebug() << "AIEngine: Initializing...";

    // Setup connection check timer (check every 30 seconds)
    m_connectionCheckTimer->setInterval(30000);
    connect(m_connectionCheckTimer, &QTimer::timeout, this, &AIEngine::checkConnection);

    // Do initial connection check
    checkConnection();
}

AIEngine::~AIEngine()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void AIEngine::setProvider(const QString &provider)
{
    m_provider = provider;
    qDebug() << "AIEngine: Provider set to" << provider;
}

void AIEngine::setApiUrl(const QString &url)
{
    m_apiUrl = url;
    qDebug() << "AIEngine: API URL set to" << url;
    checkConnection();  // Re-check connection when URL changes
}

void AIEngine::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void AIEngine::setModel(const QString &model)
{
    m_model = model;
    qDebug() << "AIEngine: Model set to" << model;
}

void AIEngine::setTemperature(float temp)
{
    m_temperature = qBound(0.0f, temp, 2.0f);
}

void AIEngine::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

void AIEngine::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

void AIEngine::sendMessage(const QString &userMessage, const QStringList &conversationHistory)
{
    if (m_processing) {
        qWarning() << "AIEngine: Already processing a request";
        emit errorOccurred("Already processing a request. Please wait.");
        return;
    }

    if (userMessage.trimmed().isEmpty()) {
        emit errorOccurred("Message cannot be empty");
        return;
    }

    qDebug() << "AIEngine: Sending message:" << userMessage;
    emit progressUpdate("Sending request...");

    m_processing = true;

    QJsonObject requestData = buildChatRequest(userMessage, conversationHistory);
    sendRequest(requestData);
}

void AIEngine::cancelRequest()
{
    if (m_currentReply) {
        qDebug() << "AIEngine: Cancelling current request";
        m_currentReply->abort();
    }
    m_processing = false;
}

bool AIEngine::isAvailable() const
{
    return m_available;
}

void AIEngine::checkConnection()
{
    qDebug() << "AIEngine: Checking connection to" << m_apiUrl;

    // Simple health check: send minimal request to /v1/models endpoint or test the main endpoint
    QUrl url(m_apiUrl + "/v1/models");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AIEngine::onConnectionCheckFinished);
}

void AIEngine::sendRequest(const QJsonObject &requestData)
{
    QUrl url(m_apiUrl + "/v1/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    QJsonDocument doc(requestData);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    qDebug() << "AIEngine: Sending POST to" << url.toString();
    qDebug() << "AIEngine: Request data:" << QString::fromUtf8(data);

    m_currentReply = m_networkManager->post(request, data);
    connect(m_currentReply, &QNetworkReply::finished, this, &AIEngine::onNetworkReplyFinished);

    emit progressUpdate("Waiting for response...");
}

QJsonObject AIEngine::buildChatRequest(const QString &message, const QStringList &history)
{
    QJsonObject request;
    request["model"] = m_model;
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;

    // Build messages array
    QJsonArray messages;

    // Add system prompt
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject systemMessage;
        systemMessage["role"] = "system";
        systemMessage["content"] = m_systemPrompt;
        messages.append(systemMessage);
    }

    // Add conversation history
    for (const QString &entry : history) {
        if (entry.startsWith("User: ")) {
            QJsonObject userMsg;
            userMsg["role"] = "user";
            userMsg["content"] = entry.mid(6);  // Remove "User: " prefix
            messages.append(userMsg);
        } else if (entry.startsWith("Assistant: ")) {
            QJsonObject assistantMsg;
            assistantMsg["role"] = "assistant";
            assistantMsg["content"] = entry.mid(11);  // Remove "Assistant: " prefix
            messages.append(assistantMsg);
        }
    }

    // Add current user message
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = message;
    messages.append(userMessage);

    request["messages"] = messages;

    return request;
}

void AIEngine::parseResponse(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred("Invalid JSON response from AI service");
        return;
    }

    QJsonObject response = doc.object();

    // Check for error in response
    if (response.contains("error")) {
        QJsonObject error = response["error"].toObject();
        QString errorMsg = error["message"].toString("Unknown error");
        emit errorOccurred(QString("AI service error: %1").arg(errorMsg));
        return;
    }

    // Extract the response text
    if (!response.contains("choices")) {
        emit errorOccurred("Response missing 'choices' field");
        return;
    }

    QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty()) {
        emit errorOccurred("Response has empty choices array");
        return;
    }

    QJsonObject firstChoice = choices[0].toObject();
    QJsonObject messageObj = firstChoice["message"].toObject();
    QString content = messageObj["content"].toString();

    if (content.isEmpty()) {
        emit errorOccurred("Response content is empty");
        return;
    }

    // Extract token usage
    int tokensUsed = 0;
    if (response.contains("usage")) {
        QJsonObject usage = response["usage"].toObject();
        tokensUsed = usage["total_tokens"].toInt(0);
    }

    qDebug() << "AIEngine: Received response:" << content;
    qDebug() << "AIEngine: Tokens used:" << tokensUsed;

    emit responseReceived(content, tokensUsed);
}

void AIEngine::handleNetworkError(QNetworkReply::NetworkError error)
{
    QString errorMsg;
    switch (error) {
        case QNetworkReply::ConnectionRefusedError:
            errorMsg = "Connection refused. Is the AI service running?";
            break;
        case QNetworkReply::HostNotFoundError:
            errorMsg = "Host not found. Check the API URL.";
            break;
        case QNetworkReply::TimeoutError:
            errorMsg = "Request timeout. The AI service is not responding.";
            break;
        case QNetworkReply::ContentNotFoundError:
            errorMsg = "Endpoint not found (404). Check API URL and version.";
            break;
        case QNetworkReply::AuthenticationRequiredError:
            errorMsg = "Authentication required. Check API key.";
            break;
        default:
            errorMsg = QString("Network error: %1").arg(m_currentReply->errorString());
            break;
    }

    qWarning() << "AIEngine: Network error:" << errorMsg;
    emit errorOccurred(errorMsg);
}

void AIEngine::onNetworkReplyFinished()
{
    m_processing = false;

    if (!m_currentReply) {
        return;
    }

    QNetworkReply::NetworkError error = m_currentReply->error();

    if (error != QNetworkReply::NoError) {
        handleNetworkError(error);
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Check HTTP status code
    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "AIEngine: HTTP status code:" << statusCode;

    if (statusCode != 200) {
        QByteArray errorData = m_currentReply->readAll();
        qWarning() << "AIEngine: HTTP error" << statusCode << ":" << QString::fromUtf8(errorData);
        emit errorOccurred(QString("HTTP %1: %2").arg(statusCode).arg(QString::fromUtf8(errorData)));
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // Parse successful response
    QByteArray data = m_currentReply->readAll();
    qDebug() << "AIEngine: Response data:" << QString::fromUtf8(data);

    parseResponse(data);

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void AIEngine::onConnectionCheckFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    bool wasAvailable = m_available;
    QNetworkReply::NetworkError error = reply->error();

    if (error == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_available = (statusCode == 200 || statusCode == 404);  // 404 is OK, endpoint might not exist but server is running
        qDebug() << "AIEngine: Connection check - HTTP" << statusCode << "- Available:" << m_available;
    } else {
        m_available = false;
        qDebug() << "AIEngine: Connection check failed -" << reply->errorString();
    }

    if (wasAvailable != m_available) {
        qDebug() << "AIEngine: Connection status changed to" << (m_available ? "available" : "unavailable");
        emit connectionStatusChanged(m_available);
    }

    reply->deleteLater();

    // Start periodic checks if not already running
    if (!m_connectionCheckTimer->isActive()) {
        m_connectionCheckTimer->start();
    }
}
