#include "OCREngine.h"
#include "TranslationEngine.h"
#include <QDebug>
#include <QBuffer>
#include <QImageWriter>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonArray>
#include <QUrlQuery>

OCREngine::OCREngine(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
{
    m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/ohao-lang-ocr";
    QDir().mkpath(m_tempDir);
}

OCREngine::~OCREngine()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    if (m_tempImageFile) {
        delete m_tempImageFile;
    }
}

void OCREngine::setEngine(Engine engine)
{
    m_engine = engine;
}

void OCREngine::setLanguage(const QString &language)
{
    m_language = language;
}

void OCREngine::setQualityLevel(int level)
{
    m_qualityLevel = qBound(1, level, 5);
}

void OCREngine::setPreprocessing(bool enabled)
{
    m_preprocessing = enabled;
}

void OCREngine::setAutoDetectOrientation(bool enabled)
{
    m_autoDetectOrientation = enabled;
}

void OCREngine::setAutoTranslate(bool enabled)
{
    m_autoTranslate = enabled;
}

void OCREngine::setTranslationEngine(const QString &engine)
{
    m_translationEngine = engine;
}

void OCREngine::setTranslationSourceLanguage(const QString &language)
{
    m_translationSourceLanguage = language;
}

void OCREngine::setTranslationTargetLanguage(const QString &language)
{
    m_translationTargetLanguage = language;
}

void OCREngine::performOCR(const QPixmap &image)
{
    if (image.isNull()) {
        emit ocrError("Invalid image provided for OCR");
        return;
    }

    emit ocrProgress("Starting OCR processing...");

    switch (m_engine) {
    case Tesseract:
        performTesseractOCR(image);
        break;
    case EasyOCR:
        performEasyOCR(image);
        break;
    case PaddleOCR:
        performPaddleOCR(image);
        break;
    case WindowsOCR:
        performWindowsOCR(image);
        break;
    case OnlineOCR:
        performOnlineOCR(image);
        break;
    }
}

void OCREngine::performTesseractOCR(const QPixmap &image)
{
    if (!isTesseractAvailable()) {
        emit ocrError("Tesseract OCR is not installed or not found in PATH");
        return;
    }

    // Save image to temporary file
    if (m_tempImageFile) {
        delete m_tempImageFile;
    }

    m_tempImageFile = new QTemporaryFile(m_tempDir + "/ocr_image_XXXXXX.png");
    if (!m_tempImageFile->open()) {
        emit ocrError("Failed to create temporary image file");
        return;
    }

    QString imagePath = m_tempImageFile->fileName();
    if (!image.save(imagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }
    m_tempImageFile->close();

    // Preprocess image if enabled
    QString processedImagePath = imagePath;
    if (m_preprocessing) {
        emit ocrProgress("Preprocessing image...");
        processedImagePath = preprocessImage(imagePath);
    }

    // Prepare Tesseract command
    QStringList arguments;
    arguments << processedImagePath;
    arguments << "stdout"; // Output to stdout instead of file

    // Language parameter
    QString langCode = getTesseractLanguageCode(m_language);
    if (!langCode.isEmpty()) {
        arguments << "-l" << langCode;
    }

    // PSM (Page Segmentation Mode) based on quality level
    int psm = 6; // Default: uniform block of text
    switch (m_qualityLevel) {
    case 1: psm = 8; break; // Single word
    case 2: psm = 7; break; // Single text line
    case 3: psm = 6; break; // Uniform block of text
    case 4: psm = 3; break; // Fully automatic page segmentation
    case 5: psm = 1; break; // Automatic with OSD
    }
    arguments << "--psm" << QString::number(psm);

    // Auto-detect orientation
    if (m_autoDetectOrientation && m_qualityLevel >= 4) {
        arguments << "--oem" << "1"; // LSTM engine
    }

    // Additional configurations for better accuracy
    arguments << "-c" << "tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?;:()[]{}\"'-+= \n\t";

    emit ocrProgress("Running Tesseract OCR...");

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OCREngine::onTesseractFinished);

    m_process->start("tesseract", arguments);
    if (!m_process->waitForStarted(5000)) {
        emit ocrError("Failed to start Tesseract process");
        return;
    }
}

void OCREngine::performEasyOCR(const QPixmap &image)
{
    // Save image to temporary file
    if (m_tempImageFile) {
        delete m_tempImageFile;
    }

    m_tempImageFile = new QTemporaryFile(m_tempDir + "/ocr_image_XXXXXX.png");
    if (!m_tempImageFile->open()) {
        emit ocrError("Failed to create temporary image file");
        return;
    }

    QString imagePath = m_tempImageFile->fileName();
    if (!image.save(imagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }
    m_tempImageFile->close();

    QString script = getPythonOCRScript(EasyOCR);

    // Create Python script file
    QTemporaryFile scriptFile(m_tempDir + "/easyocr_script_XXXXXX.py");
    if (!scriptFile.open()) {
        emit ocrError("Failed to create OCR script file");
        return;
    }

    scriptFile.write(script.toUtf8());
    scriptFile.close();

    emit ocrProgress("Running EasyOCR...");

    QStringList arguments;
    arguments << scriptFile.fileName() << imagePath << m_language.toLower();

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OCREngine::onPythonOCRFinished);

    m_process->start("python3", arguments);
    if (!m_process->waitForStarted(5000)) {
        // Try python instead of python3
        m_process->start("python", arguments);
        if (!m_process->waitForStarted(5000)) {
            emit ocrError("Failed to start Python process for EasyOCR");
            return;
        }
    }
}

void OCREngine::performPaddleOCR(const QPixmap &image)
{
    // Similar implementation to EasyOCR but with PaddleOCR
    if (m_tempImageFile) {
        delete m_tempImageFile;
    }

    m_tempImageFile = new QTemporaryFile(m_tempDir + "/ocr_image_XXXXXX.png");
    if (!m_tempImageFile->open()) {
        emit ocrError("Failed to create temporary image file");
        return;
    }

    QString imagePath = m_tempImageFile->fileName();
    if (!image.save(imagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }
    m_tempImageFile->close();

    QString script = getPythonOCRScript(PaddleOCR);

    QTemporaryFile scriptFile(m_tempDir + "/paddleocr_script_XXXXXX.py");
    if (!scriptFile.open()) {
        emit ocrError("Failed to create OCR script file");
        return;
    }

    scriptFile.write(script.toUtf8());
    scriptFile.close();

    emit ocrProgress("Running PaddleOCR...");

    QStringList arguments;
    arguments << scriptFile.fileName() << imagePath << m_language.toLower();

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OCREngine::onPythonOCRFinished);

    m_process->start("python3", arguments);
    if (!m_process->waitForStarted(5000)) {
        m_process->start("python", arguments);
        if (!m_process->waitForStarted(5000)) {
            emit ocrError("Failed to start Python process for PaddleOCR");
            return;
        }
    }
}

void OCREngine::performWindowsOCR(const QPixmap &image)
{
    // On non-Windows systems, fall back to online OCR or show error
    #ifdef Q_OS_WIN
    // TODO: Implement Windows.Media.Ocr API
    emit ocrError("Windows OCR implementation not yet available");
    #else
    emit ocrError("Windows OCR is only available on Windows systems");
    #endif
}

void OCREngine::performOnlineOCR(const QPixmap &image)
{
    // Use a free online OCR service as fallback
    emit ocrProgress("Preparing image for online OCR...");

    // Convert image to base64
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    QByteArray imageData = buffer.data().toBase64();

    // Create multipart form data
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"image\"; filename=\"ocr.png\""));
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
    imagePart.setBody(buffer.data());
    multiPart->append(imagePart);

    QHttpPart langPart;
    langPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"language\""));
    langPart.setBody(m_language.toLower().toUtf8());
    multiPart->append(langPart);

    emit ocrProgress("Sending image to OCR service...");

    QNetworkRequest request(QUrl("https://api.ocr.space/parse/image"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "OhaoLang/1.0");

    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, &OCREngine::onNetworkReplyFinished);

    // Set timeout
    QTimer::singleShot(30000, reply, &QNetworkReply::abort);
}

QString OCREngine::preprocessImage(const QString &imagePath)
{
    // Simple preprocessing using ImageMagick (if available) or fallback
    QString outputPath = m_tempDir + "/preprocessed_" + QFileInfo(imagePath).fileName();

    QProcess process;
    QStringList arguments;

    // Try ImageMagick convert
    arguments << imagePath;
    arguments << "-density" << "300";
    arguments << "-quality" << "100";
    arguments << "-sharpen" << "0x1.0";
    arguments << outputPath;

    process.start("convert", arguments);
    if (process.waitForFinished(10000) && process.exitCode() == 0) {
        return outputPath;
    }

    // Fallback to original image
    return imagePath;
}

QString OCREngine::getTesseractLanguageCode(const QString &language)
{
    static QMap<QString, QString> languageMap = {
        {"English", "eng"},
        {"Chinese (Simplified)", "chi_sim"},
        {"Chinese (Traditional)", "chi_tra"},
        {"Japanese", "jpn"},
        {"Korean", "kor"},
        {"Spanish", "spa"},
        {"French", "fra"},
        {"German", "deu"},
        {"Russian", "rus"},
        {"Portuguese", "por"},
        {"Auto-Detect", ""}
    };

    return languageMap.value(language, "eng");
}

QString OCREngine::getPythonOCRScript(Engine engine)
{
    if (engine == EasyOCR) {
        return R"(
import sys
import json
import easyocr

try:
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Not enough arguments"}))
        sys.exit(1)

    image_path = sys.argv[1]
    language = sys.argv[2] if len(sys.argv) > 2 else 'en'

    # Map language names to EasyOCR codes
    lang_map = {
        'english': 'en',
        'chinese': 'ch_sim',
        'japanese': 'ja',
        'korean': 'ko',
        'spanish': 'es',
        'french': 'fr',
        'german': 'de',
        'russian': 'ru',
        'portuguese': 'pt'
    }

    lang_code = lang_map.get(language.lower(), 'en')

    reader = easyocr.Reader([lang_code])
    results = reader.readtext(image_path)

    text_parts = []
    confidence_sum = 0
    count = 0

    for (bbox, text, confidence) in results:
        text_parts.append(text)
        confidence_sum += confidence
        count += 1

    final_text = ' '.join(text_parts)
    avg_confidence = confidence_sum / count if count > 0 else 0

    result = {
        "text": final_text,
        "confidence": f"{avg_confidence:.2f}",
        "success": True
    }

    print(json.dumps(result))

except Exception as e:
    error_result = {
        "error": str(e),
        "success": False
    }
    print(json.dumps(error_result))
)";
    } else if (engine == PaddleOCR) {
        return R"(
import sys
import json
from paddleocr import PaddleOCR

try:
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Not enough arguments"}))
        sys.exit(1)

    image_path = sys.argv[1]
    language = sys.argv[2] if len(sys.argv) > 2 else 'en'

    # Map language names to PaddleOCR codes
    lang_map = {
        'english': 'en',
        'chinese': 'ch',
        'japanese': 'japan',
        'korean': 'korean',
        'spanish': 'es',
        'french': 'fr',
        'german': 'german',
        'russian': 'ru',
        'portuguese': 'pt'
    }

    lang_code = lang_map.get(language.lower(), 'en')

    ocr = PaddleOCR(use_angle_cls=True, lang=lang_code, show_log=False)
    results = ocr.ocr(image_path, cls=True)

    text_parts = []
    confidence_sum = 0
    count = 0

    if results and results[0]:
        for line in results[0]:
            if line and len(line) > 1:
                text = line[1][0] if len(line[1]) > 0 else ""
                confidence = line[1][1] if len(line[1]) > 1 else 0
                text_parts.append(text)
                confidence_sum += confidence
                count += 1

    final_text = ' '.join(text_parts)
    avg_confidence = confidence_sum / count if count > 0 else 0

    result = {
        "text": final_text,
        "confidence": f"{avg_confidence:.2f}",
        "success": True
    }

    print(json.dumps(result))

except Exception as e:
    error_result = {
        "error": str(e),
        "success": False
    }
    print(json.dumps(error_result))
)";
    }

    return "";
}

void OCREngine::onTesseractFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    OCRResult result;

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        result.success = false;
        result.errorMessage = m_process->readAllStandardError();
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = "Tesseract process failed with exit code " + QString::number(exitCode);
        }
    } else {
        QString output = m_process->readAllStandardOutput();
        result.text = output.trimmed();
        result.success = !result.text.isEmpty();
        result.confidence = "N/A"; // Tesseract doesn't provide confidence by default
        result.language = m_language;

        if (!result.success) {
            result.errorMessage = "No text detected in image";
        }
    }

    m_process->deleteLater();
    m_process = nullptr;

    // Store the OCR result and start translation if auto-translate is enabled
    m_currentOCRResult = result;
    if (result.success && m_autoTranslate && !result.text.isEmpty()) {
        startTranslation(result.text);
    } else {
        emit ocrFinished(result);
    }
}

void OCREngine::onPythonOCRFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    OCRResult result;

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        result.success = false;
        result.errorMessage = m_process->readAllStandardError();
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = "Python OCR process failed with exit code " + QString::number(exitCode);
        }
    } else {
        QString output = m_process->readAllStandardOutput();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            result.success = false;
            result.errorMessage = "Failed to parse OCR output: " + parseError.errorString();
        } else {
            QJsonObject obj = doc.object();
            result.success = obj["success"].toBool();
            result.text = obj["text"].toString();
            result.confidence = obj["confidence"].toString();
            result.language = m_language;
            result.errorMessage = obj["error"].toString();
        }
    }

    m_process->deleteLater();
    m_process = nullptr;

    // Store the OCR result and start translation if auto-translate is enabled
    m_currentOCRResult = result;
    if (result.success && m_autoTranslate && !result.text.isEmpty()) {
        startTranslation(result.text);
    } else {
        emit ocrFinished(result);
    }
}

void OCREngine::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    OCRResult result;

    if (reply->error() != QNetworkReply::NoError) {
        result.success = false;
        result.errorMessage = "Network error: " + reply->errorString();
    } else {
        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            result.success = false;
            result.errorMessage = "Failed to parse online OCR response";
        } else {
            QJsonObject obj = doc.object();
            QJsonArray parsedResults = obj["ParsedResults"].toArray();

            if (!parsedResults.isEmpty()) {
                QJsonObject firstResult = parsedResults[0].toObject();
                result.text = firstResult["ParsedText"].toString();
                result.success = !result.text.isEmpty();
                result.confidence = "N/A";
                result.language = m_language;
            } else {
                result.success = false;
                result.errorMessage = "No text found in image";
            }
        }
    }

    reply->deleteLater();
    emit ocrFinished(result);
}

// Static availability checks
bool OCREngine::isTesseractAvailable()
{
    QProcess process;
    process.start("tesseract", QStringList() << "--version");
    return process.waitForFinished(3000) && process.exitCode() == 0;
}

bool OCREngine::isEasyOCRAvailable()
{
    QProcess process;
    process.start("python3", QStringList() << "-c" << "import easyocr; print('OK')");
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        return true;
    }

    // Try python instead of python3
    process.start("python", QStringList() << "-c" << "import easyocr; print('OK')");
    return process.waitForFinished(5000) && process.exitCode() == 0;
}

bool OCREngine::isPaddleOCRAvailable()
{
    QProcess process;
    process.start("python3", QStringList() << "-c" << "from paddleocr import PaddleOCR; print('OK')");
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        return true;
    }

    // Try python instead of python3
    process.start("python", QStringList() << "-c" << "from paddleocr import PaddleOCR; print('OK')");
    return process.waitForFinished(5000) && process.exitCode() == 0;
}

bool OCREngine::isWindowsOCRAvailable()
{
    #ifdef Q_OS_WIN
    return true; // TODO: Check if Windows.Media.Ocr is available
    #else
    return false;
    #endif
}

void OCREngine::startTranslation(const QString &text)
{
    if (!m_translationEngineInstance) {
        m_translationEngineInstance = new TranslationEngine(this);
        connect(m_translationEngineInstance, &TranslationEngine::translationFinished,
                this, &OCREngine::onTranslationFinished);
        connect(m_translationEngineInstance, &TranslationEngine::translationError,
                this, &OCREngine::onTranslationError);
        connect(m_translationEngineInstance, &TranslationEngine::translationProgress,
                this, &OCREngine::ocrProgress);

        // Configure the translation engine based on settings
        if (m_translationEngine == "Google Translate (Free)") {
            m_translationEngineInstance->setEngine(TranslationEngine::GoogleTranslate);
        } else if (m_translationEngine == "LibreTranslate") {
            m_translationEngineInstance->setEngine(TranslationEngine::LibreTranslate);
        } else if (m_translationEngine.contains("Ollama")) {
            m_translationEngineInstance->setEngine(TranslationEngine::OllamaLLM);
        } else if (m_translationEngine.contains("Microsoft")) {
            m_translationEngineInstance->setEngine(TranslationEngine::MicrosoftTranslator);
        } else if (m_translationEngine.contains("DeepL")) {
            m_translationEngineInstance->setEngine(TranslationEngine::DeepL);
        }

        m_translationEngineInstance->setSourceLanguage(m_translationSourceLanguage);
        m_translationEngineInstance->setTargetLanguage(m_translationTargetLanguage);

        // Load API settings from QSettings
        if (m_settings) {
            m_translationEngineInstance->setApiKey(m_settings->value("translation/apiKey").toString());
            m_translationEngineInstance->setApiUrl(m_settings->value("translation/apiUrl").toString());
        }
    }

    emit ocrProgress("Starting translation...");
    m_translationEngineInstance->translate(text);
}

void OCREngine::onTranslationFinished(const TranslationResult &translationResult)
{
    m_currentOCRResult.hasTranslation = translationResult.success;

    if (translationResult.success) {
        m_currentOCRResult.translatedText = translationResult.translatedText;
        m_currentOCRResult.sourceLanguage = translationResult.sourceLanguage;
        m_currentOCRResult.targetLanguage = translationResult.targetLanguage;
        emit ocrProgress("Translation completed successfully!");
    } else {
        m_currentOCRResult.translatedText = "Translation failed: " + translationResult.errorMessage;
        emit ocrProgress("Translation failed: " + translationResult.errorMessage);
    }

    emit ocrFinished(m_currentOCRResult);
}

void OCREngine::onTranslationError(const QString &error)
{
    m_currentOCRResult.hasTranslation = false;
    m_currentOCRResult.translatedText = "Translation error: " + error;

    emit ocrProgress("Translation error: " + error);
    emit ocrFinished(m_currentOCRResult);
}