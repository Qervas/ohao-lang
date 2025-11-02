#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

/**
 * @brief AI Engine - Communication with AI service providers
 *
 * Supports OpenAI-compatible API endpoints like GitHub Copilot API.
 * Handles HTTP requests, response parsing, error handling, and token tracking.
 *
 * Features:
 * - Asynchronous HTTP communication
 * - Conversation history management
 * - Token usage tracking
 * - Connection health checks
 * - Automatic retry on transient failures
 * - Streaming response support (future)
 */
class AIEngine : public QObject
{
    Q_OBJECT

signals:
    /**
     * @brief Emitted when AI response is received successfully
     * @param response The AI's response text
     * @param tokensUsed Number of tokens consumed by this request
     */
    void responseReceived(const QString &response, int tokensUsed);

    /**
     * @brief Emitted when an error occurs
     * @param error Error message describing what went wrong
     */
    void errorOccurred(const QString &error);

    /**
     * @brief Emitted when connection status changes
     * @param connected True if API is reachable, false otherwise
     */
    void connectionStatusChanged(bool connected);

    /**
     * @brief Emitted during processing to show progress
     * @param status Status message (e.g., "Sending request...", "Waiting for response...")
     */
    void progressUpdate(const QString &status);

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine();

    // Configuration
    void setProvider(const QString &provider);
    void setApiUrl(const QString &url);
    void setApiKey(const QString &key);
    void setModel(const QString &model);
    void setTemperature(float temp);
    void setMaxTokens(int maxTokens);
    void setSystemPrompt(const QString &prompt);

    // Operations
    void sendMessage(const QString &userMessage,
                     const QStringList &conversationHistory = QStringList());
    void cancelRequest();
    bool isAvailable() const;
    void checkConnection();

    // Getters
    QString provider() const { return m_provider; }
    QString apiUrl() const { return m_apiUrl; }
    QString model() const { return m_model; }
    bool isProcessing() const { return m_processing; }

private slots:
    void onNetworkReplyFinished();
    void onConnectionCheckFinished();

private:
    void sendRequest(const QJsonObject &requestData);
    QJsonObject buildChatRequest(const QString &message,
                                   const QStringList &history);
    void parseResponse(const QByteArray &data);
    void handleNetworkError(QNetworkReply::NetworkError error);

    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;

    // Configuration
    QString m_provider;
    QString m_apiUrl;
    QString m_apiKey;
    QString m_model;
    float m_temperature;
    int m_maxTokens;
    QString m_systemPrompt;

    // State
    bool m_processing;
    bool m_available;
    QTimer *m_connectionCheckTimer;
};
