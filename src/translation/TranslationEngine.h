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
        GoogleTranslate,
        LibreTranslate,
        OllamaLLM,
        MicrosoftTranslator,
        DeepL,
        OfflineDictionary
    };

    explicit TranslationEngine(QObject *parent = nullptr);
    ~TranslationEngine();

    void setEngine(Engine engine);
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
    void translateWithLibreTranslate(const QString &text);
    void translateWithOllama(const QString &text);
    void translateWithMicrosoft(const QString &text);
    void translateWithDeepL(const QString &text);
    void translateOffline(const QString &text);

    void parseGoogleResponse(const QByteArray &response);
    void parseLibreTranslateResponse(const QByteArray &response);
    void parseOllamaResponse(const QByteArray &response);
    void parseMicrosoftResponse(const QByteArray &response);
    void parseDeepLResponse(const QByteArray &response);

    QString detectSourceLanguage(const QString &text);
    QString buildGoogleTranslateUrl(const QString &text);
    QString buildLibreTranslateRequest(const QString &text);

    Engine m_engine = GoogleTranslate;
    QString m_sourceLanguage = "auto";
    QString m_targetLanguage = "en";
    QString m_apiKey;
    QString m_apiUrl;
    QString m_currentText;

    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QSettings *m_settings = nullptr;

    static const int TIMEOUT_MS = 30000; // 30 seconds timeout
};