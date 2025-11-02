#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QSettings>
#include <QStringList>

struct TranslationResult {
    QString translatedText;
    QString sourceLanguage;
    QString targetLanguage;
    QString confidence;
    bool success = false;
    QString errorMessage;
};

class TranslationEngine : public QObject
{
    Q_OBJECT

public:
    enum Engine {
        GoogleTranslate  // Only Google Translate is supported (free, no API key needed)
    };

    explicit TranslationEngine(QObject *parent = nullptr);
    ~TranslationEngine();

    void setEngine(Engine engine);
    // Set source language for translation
    // NOTE: Should be the OCR-detected language or "Auto-Detect" for automatic detection
    void setSourceLanguage(const QString &language);
    void setTargetLanguage(const QString &language);
    void setApiKey(const QString &key);
    void setApiUrl(const QString &url);

    Engine currentEngine() const { return m_engine; }
    QString sourceLanguage() const { return m_sourceLanguage; }
    QString targetLanguage() const { return m_targetLanguage; }

    // Main translation function
    void translate(const QString &text);

    // Language code conversion
    static QString getLanguageCode(const QString &language, Engine engine);
    static QString getLanguageName(const QString &code);

signals:
    void translationFinished(const TranslationResult &result);
    void translationProgress(const QString &status);
    void translationError(const QString &error);

private slots:
    void onNetworkReplyFinished();
    void onRequestTimeout();

private:
    void translateWithGoogle(const QString &text);
	void parseGoogleResponse(const QByteArray &response);
	bool parseGoogleResponseText(const QByteArray &response, QString &outText);
	void sendRequestForText(const QString &text);
	QStringList chunkTextByLimit(const QString &text, int limit) const;
	void startNextChunk();
    QNetworkRequest createGoogleTranslateRequest(const QString &url);

    QString detectSourceLanguage(const QString &text);
    QString buildGoogleTranslateUrl(const QString &text);
    QString buildGoogleTranslateUrl(const QString &text, bool forceAutoDetect);

    Engine m_engine = GoogleTranslate;
    QString m_sourceLanguage; // Set from user settings
    QString m_targetLanguage; // Set from user settings
    QString m_apiKey;
    QString m_apiUrl;
    QString m_currentText;

    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QSettings *m_settings = nullptr;
	int m_retryAttempt = 0;
	QStringList m_chunks;
	int m_currentChunkIndex = -1;
	QString m_aggregatedText;
    bool m_forceAutoDetect = false;

	static const int TIMEOUT_MS = 30000; // 30 seconds timeout
	static const int MAX_RETRIES = 2;    // retry count per chunk
	static const int BACKOFF_BASE_MS = 800; // exponential backoff base
	static const int CHUNK_CHAR_LIMIT = 4800; // prefer one request; stay under common 5k limit
};