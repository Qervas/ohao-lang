#pragma once

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QProcess>
#include <QSettings>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHttpMultiPart>
#include <QTimer>
#include <QFileInfo>
#include <QMap>
#include <QRect>
#include <QVector>
#include <memory>

class TranslationEngine;

struct OCRResult {
    QString text;
    QString translatedText;
    QString confidence;
    QString language;
    QString sourceLanguage;
    QString targetLanguage;
    bool success = false;
    bool hasTranslation = false;
    QString errorMessage;
    struct OCRToken {
        QString text;
        QRect   box;        // in selection image coordinates
        float    confidence = -1.f;
        int      lineId = -1;
    };
    QVector<OCRToken> tokens; // populated when engine returns positional data
};

class OCREngine : public QObject
{
    Q_OBJECT

public:
    enum Engine {
        AppleVision,  // macOS native Vision framework (default on macOS)
        Tesseract     // Cross-platform Tesseract OCR
    };

    explicit OCREngine(QObject *parent = nullptr);
    ~OCREngine();

    void setEngine(Engine engine);
    void setLanguage(const QString &language);
    void setQualityLevel(int level); // 1-5 scale
    void setPreprocessing(bool enabled);
    void setAutoDetectOrientation(bool enabled);

    Engine currentEngine() const { return m_engine; }
    QString currentLanguage() const { return m_language; }

    // Main OCR function
    void performOCR(const QPixmap &image);

    // Translation settings
    void setAutoTranslate(bool enabled);
    void setTranslationEngine(const QString &engine);
    void setTranslationSourceLanguage(const QString &language);
    void setTranslationTargetLanguage(const QString &language);

    // Engine availability checks
    static bool isAppleVisionAvailable();
    static bool isTesseractAvailable();

    // Concurrency helpers
    bool isBusy() const { return m_process && m_process->state() != QProcess::NotRunning; }
public slots:
    void cancel();

signals:
    void ocrFinished(const OCRResult &result);
    void ocrProgress(const QString &status);
    void ocrError(const QString &error);

private slots:
    void onTesseractFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTranslationFinished(const struct TranslationResult &result);
    void onTranslationError(const QString &error);

private:
    void performAppleVisionOCR(const QPixmap &image);
    void performTesseractOCR(const QPixmap &image);

    QString preprocessImage(const QString &imagePath);
    // REMOVED: getTesseractLanguageCode - use LanguageManager::instance().getTesseractCode() instead
    void startTranslation(const QString &text);
    QString mergeParagraphLines(const QStringList &lines, const QVector<OCRResult::OCRToken> &tokens);

    // Helper to ensure tokens are always provided
    void ensureTokensExist(OCRResult &result, const QSize &imageSize = QSize());

    // Helper to stop any running process safely
    void stopRunningProcess();

    // Helper to find Tesseract executable recursively
    static QString findTesseractExecutable();

    // Language-specific character correction
    QString correctLanguageSpecificCharacters(const QString &text, const QString &language);


    Engine m_engine = Tesseract;
    QString m_language; // No hardcoded default - loaded from settings
    int m_qualityLevel = 3;
    bool m_preprocessing = true;
    bool m_autoDetectOrientation = true;

    // Translation settings - no hardcoded defaults, loaded from user settings
    bool m_autoTranslate = false;
    QString m_translationEngine;
    QString m_translationSourceLanguage;
    QString m_translationTargetLanguage;

    OCRResult m_currentOCRResult;
    TranslationEngine *m_translationEngineInstance = nullptr;

    QProcess *m_process = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    // Replace QTemporaryFile* with persistent image path we manage manually
    QString m_currentImagePath;
    QString m_tempDir;
    QSettings *m_settings = nullptr;
    QString m_lastProcessedImagePath; // for secondary TSV extraction

};