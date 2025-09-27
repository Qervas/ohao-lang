#include "OCREngine.h"
#include "TranslationEngine.h"
#include <QDebug>
#include <QBuffer>
#include <QImageWriter>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QtMath>

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
    stopRunningProcess();
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
    }
}

void OCREngine::cancel()
{
    stopRunningProcess();
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }
    OCRResult cancelled;
    cancelled.success = false;
    cancelled.errorMessage = "OCR cancelled";
    emit ocrFinished(cancelled);
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

    // If already running, cancel current run to start a new one safely
    stopRunningProcess();

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

    // Ensure TESSDATA_PREFIX is set so languages can load (Windows users installing via Scoop often miss this)
    if (qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        QStringList candidateDirs;
#ifdef Q_OS_WIN
        // Common Windows install locations
        candidateDirs << QDir::fromNativeSeparators("C:/Program Files/Tesseract-OCR/tessdata");
        candidateDirs << QDir::fromNativeSeparators("C:/Program Files (x86)/Tesseract-OCR/tessdata");
        // Scoop (user-specific) pattern
        QString home = QDir::homePath();
        candidateDirs << home + "/scoop/apps/tesseract/current/tessdata";
        candidateDirs << home + "/scoop/persist/tesseract/tessdata";
#endif
        // Allow portable relative placement next to the executable
        candidateDirs << QCoreApplication::applicationDirPath() + "/tessdata";

        for (const QString &dirPath : candidateDirs) {
            if (QDir(dirPath).exists(dirPath + "/eng.traineddata")) {
                qputenv("TESSDATA_PREFIX", QFileInfo(dirPath).absolutePath().toUtf8());
                emit ocrProgress(QString("Set TESSDATA_PREFIX to %1").arg(QFileInfo(dirPath).absolutePath()));
                break;
            }
        }
    }

    // Save image to persistent temp file
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }

    m_currentImagePath = m_tempDir + "/ocr_image_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    if (!image.save(m_currentImagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }
    emit ocrProgress(QString("Saved temp image: %1 (%2x%3)")
                     .arg(QFileInfo(m_currentImagePath).fileName())
                     .arg(image.width()).arg(image.height()));

    // Preprocess image if enabled
    QString processedImagePath = m_currentImagePath;
    if (m_preprocessing) {
        emit ocrProgress("Preprocessing image...");
        processedImagePath = preprocessImage(m_currentImagePath);
    }
    m_lastProcessedImagePath = processedImagePath;

    // Prepare Tesseract command (plain text output first for reliability)
    QStringList arguments;
    arguments << processedImagePath;
    arguments << "stdout"; // tesseract will ignore for tsv but required positional

    // Robust tessdata detection: prefer explicit env, otherwise scan common directories
    QString tessdataDir;
    if (!qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        // TESSDATA_PREFIX should point to parent dir containing tessdata or be the tessdata dir itself
        QString prefix = QString::fromUtf8(qgetenv("TESSDATA_PREFIX"));
        if (QDir(prefix + "/tessdata").exists(prefix + "/tessdata/eng.traineddata")) {
            tessdataDir = prefix + "/tessdata";
        } else if (QDir(prefix).exists(prefix + "/eng.traineddata")) {
            tessdataDir = prefix; // already at tessdata layer
        }
    }
    if (tessdataDir.isEmpty()) {
        QStringList probe;
#ifdef Q_OS_WIN
        QString home = QDir::homePath();
        probe << "C:/Program Files/Tesseract-OCR/tessdata";
        probe << "C:/Program Files (x86)/Tesseract-OCR/tessdata";
        probe << home + "/scoop/apps/tesseract/current/tessdata";
        probe << home + "/scoop/persist/tesseract/tessdata";
#endif
        probe << QCoreApplication::applicationDirPath() + "/tessdata";
        for (const QString &p : probe) {
            if (QDir(p).exists(p + "/eng.traineddata")) { tessdataDir = p; break; }
        }
    }
    if (!tessdataDir.isEmpty()) {
        arguments << "--tessdata-dir" << tessdataDir;
        emit ocrProgress(QString("Using tessdata dir: %1").arg(tessdataDir));
    } else {
        emit ocrProgress("Warning: tessdata dir not found; language load may fail");
    }

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

    // Configuration tuning
    arguments << "-c" << "tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?;:()[]{}\"'-+= \n\t";

    emit ocrProgress("Running Tesseract OCR (text phase)...");

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OCREngine::onTesseractFinished);
    // Propagate TESSDATA_PREFIX if set after our detection
    if (!qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("TESSDATA_PREFIX", qgetenv("TESSDATA_PREFIX"));
        m_process->setProcessEnvironment(env);
    }

    m_process->start("tesseract", arguments);
    if (!m_process->waitForStarted(5000)) {
        emit ocrError("Failed to start Tesseract process");
        return;
    }
}

void OCREngine::performEasyOCR(const QPixmap &image)
{
    // Save image to persistent temp file
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }

    m_currentImagePath = m_tempDir + "/ocr_image_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    if (!image.save(m_currentImagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }

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
    arguments << scriptFile.fileName() << m_currentImagePath << m_language.toLower();

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
    // Save image to persistent temp file
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }

    m_currentImagePath = m_tempDir + "/ocr_image_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    if (!image.save(m_currentImagePath, "PNG")) {
        emit ocrError("Failed to save image to temporary file");
        return;
    }

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
    arguments << scriptFile.fileName() << m_currentImagePath << m_language.toLower();

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
    tokens = []

    for (bbox, text, confidence) in results:
        if not text:
            continue
        text_parts.append(text)
        confidence_sum += confidence
        count += 1
        # bbox is list of 4 points [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]
        xs = [p[0] for p in bbox]
        ys = [p[1] for p in bbox]
        left, top = min(xs), min(ys)
        width, height = max(xs) - left, max(ys) - top
        tokens.append({
            'text': text,
            'box': [int(left), int(top), int(width), int(height)],
            'confidence': float(confidence),
            'lineId': -1
        })

    final_text = ' '.join(text_parts)
    avg_confidence = confidence_sum / count if count > 0 else 0

    result = {
        'text': final_text,
        'confidence': f"{avg_confidence:.2f}",
        'success': True,
        'tokens': tokens
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
    tokens = []

    if results and results[0]:
        for line in results[0]:
            if line and len(line) > 1:
                box_points = line[0]  # list of 4 points
                txt_conf = line[1]
                if isinstance(txt_conf, (list, tuple)) and len(txt_conf) >= 2:
                    text, confidence = txt_conf[0], txt_conf[1]
                else:
                    text, confidence = '', 0
                if not text:
                    continue
                text_parts.append(text)
                confidence_sum += confidence
                count += 1
                xs = [p[0] for p in box_points]
                ys = [p[1] for p in box_points]
                left, top = min(xs), min(ys)
                width, height = max(xs) - left, max(ys) - top
                tokens.append({
                    'text': text,
                    'box': [int(left), int(top), int(width), int(height)],
                    'confidence': float(confidence),
                    'lineId': -1
                })

    final_text = ' '.join(text_parts)
    avg_confidence = confidence_sum / count if count > 0 else 0

    result = {
        'text': final_text,
        'confidence': f"{avg_confidence:.2f}",
        'success': True,
        'tokens': tokens
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
        // Parse TSV directly from output
        QTextStream ts(&output, QIODevice::ReadOnly);
        QString header = ts.readLine();
        Q_UNUSED(header)
        QMap<int, QString> lineAccum; // lineId -> text
        int tokenCount = 0;
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            if (line.isEmpty()) continue;
            QStringList cols = line.split('\t');
            if (cols.size() < 12) continue;
            int level = cols[0].toInt();
            if (level != 5) continue; // word level
            int lineNum = cols[5].toInt();
            int left = cols[6].toInt();
            int top = cols[7].toInt();
            int width = cols[8].toInt();
            int height = cols[9].toInt();
            float conf = cols[10].toFloat();
            QString tokenText = cols[11];
            if (tokenText.trimmed().isEmpty()) continue;
            OCRResult::OCRToken token;
            token.text = tokenText;
            token.box = QRect(left, top, width, height);
            token.confidence = conf;
            token.lineId = lineNum;
            result.tokens.push_back(token);
            if (!lineAccum.contains(lineNum)) lineAccum[lineNum] = tokenText;
            else lineAccum[lineNum] += " " + tokenText;
            ++tokenCount;
        }
        // Reconstruct text preserving line order
        QStringList lines;
        QList<int> keys = lineAccum.keys();
        std::sort(keys.begin(), keys.end());
        for (int k : keys) lines << lineAccum.value(k);
        
        // Apply intelligent paragraph merging
        result.text = mergeParagraphLines(lines, result.tokens);
        result.success = !result.text.isEmpty();
        result.confidence = "N/A";
        result.language = m_language;

        if (!result.success) {
            emit ocrProgress("TSV parse empty, retrying plain text mode...");
            // Fallback: run tesseract again in plain text mode quick
            QProcess plain;
            QStringList args;
            args << m_currentImagePath << "stdout";
            QString langCode = getTesseractLanguageCode(m_language);
            if (!langCode.isEmpty()) { args << "-l" << langCode; }
            plain.start("tesseract", args);
            if (plain.waitForFinished(5000) && plain.exitCode()==0) {
                QString txt = plain.readAllStandardOutput().trimmed();
                if (!txt.isEmpty()) {
                    // Apply paragraph merging to plain text as well
                    QStringList lines = txt.split('\n');
                    if (lines.size() > 1) {
                        // For plain text mode, use simplified merging without token info
                        result.text = mergeParagraphLines(lines, QVector<OCRResult::OCRToken>());
                    } else {
                        result.text = txt;
                    }
                    result.success = true;
                }
            }
        }
        emit ocrProgress(QString("Tesseract tokens: %1 lines: %2 success=%3")
                         .arg(result.tokens.size())
                         .arg(result.text.count('\n')+1)
                         .arg(result.success));

        if (!result.success) {
            result.errorMessage = "No text detected in image";
        }
    }

    m_process->deleteLater();
    m_process = nullptr;

    // Clean up temp image
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }

    // Store the OCR result and start translation if auto-translate is enabled
    m_currentOCRResult = result;

    // Ensure tokens exist before emitting or starting translation
    if (result.success) {
        ensureTokensExist(m_currentOCRResult);
    }

    if (result.success && m_autoTranslate && !result.text.isEmpty()) {
        startTranslation(result.text);
    } else {
        emit ocrFinished(m_currentOCRResult);
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
            if (obj.contains("tokens") && obj["tokens"].isArray()) {
                QJsonArray arr = obj["tokens"].toArray();
                for (const QJsonValue &val : arr) {
                    if (!val.isObject()) continue;
                    QJsonObject to = val.toObject();
                    OCRResult::OCRToken tok;
                    tok.text = to.value("text").toString();
                    tok.confidence = static_cast<float>(to.value("confidence").toDouble(-1.0));
                    tok.lineId = to.value("lineId").toInt(-1);
                    if (to.contains("box") && to["box"].isArray()) {
                        QJsonArray b = to["box"].toArray();
                        if (b.size() >= 4) {
                            tok.box = QRect(b[0].toInt(), b[1].toInt(), b[2].toInt(), b[3].toInt());
                        }
                    }
                    if (!tok.text.trimmed().isEmpty()) {
                        result.tokens.push_back(tok);
                    }
                }
            }
        }
    }

    m_process->deleteLater();
    m_process = nullptr;

    // Clean up temp image
    if (!m_currentImagePath.isEmpty()) {
        QFile::remove(m_currentImagePath);
        m_currentImagePath.clear();
    }

    // Store the OCR result and start translation if auto-translate is enabled
    m_currentOCRResult = result;

    // Ensure tokens exist before emitting or starting translation
    if (result.success) {
        ensureTokensExist(m_currentOCRResult);
    }

    if (result.success && m_autoTranslate && !result.text.isEmpty()) {
        startTranslation(result.text);
    } else {
        emit ocrFinished(m_currentOCRResult);
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

    // Ensure tokens exist for network OCR results
    if (result.success) {
        ensureTokensExist(result);
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

    // Ensure tokens exist before final emission
    if (m_currentOCRResult.success) {
        ensureTokensExist(m_currentOCRResult);
    }

    emit ocrFinished(m_currentOCRResult);
}

void OCREngine::onTranslationError(const QString &error)
{
    m_currentOCRResult.hasTranslation = false;
    m_currentOCRResult.translatedText = "Translation error: " + error;

    // Ensure tokens exist even for translation errors
    if (m_currentOCRResult.success) {
        ensureTokensExist(m_currentOCRResult);
    }

    emit ocrProgress("Translation error: " + error);
    emit ocrFinished(m_currentOCRResult);
}

void OCREngine::ensureTokensExist(OCRResult &result, const QSize &imageSize)
{
    // If tokens already exist, nothing to do
    if (!result.tokens.isEmpty()) {
        return;
    }

    // If no text, nothing to tokenize
    if (result.text.isEmpty()) {
        return;
    }

    qDebug() << "OCREngine: Creating fallback tokens for result without token data";

    // Use provided image size or default
    QSize actualSize = imageSize.isValid() ? imageSize : QSize(800, 600);

    // Split text into words and create a token for each
    QStringList words = result.text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        // Create single token for entire text
        OCRResult::OCRToken token;
        token.text = result.text;
        token.box = QRect(0, 0, actualSize.width(), actualSize.height());
        token.confidence = 1.0f;
        token.lineId = 0;
        result.tokens.append(token);
        qDebug() << "OCREngine: Created single fallback token for entire text";
        return;
    }

    // Estimate token positioning (simple left-to-right flow)
    int wordsPerLine = qMax(1, static_cast<int>(qSqrt(words.size()))); // Square-ish layout
    int lineHeight = actualSize.height() / qMax(1, (words.size() + wordsPerLine - 1) / wordsPerLine);
    int wordWidth = actualSize.width() / wordsPerLine;

    for (int i = 0; i < words.size(); ++i) {
        OCRResult::OCRToken token;
        token.text = words[i];
        token.confidence = 1.0f; // Fallback tokens have full confidence
        token.lineId = i / wordsPerLine;

        // Calculate position
        int col = i % wordsPerLine;
        int row = i / wordsPerLine;
        token.box = QRect(
            col * wordWidth,
            row * lineHeight,
            wordWidth,
            lineHeight
        );

        result.tokens.append(token);
    }

    qDebug() << "OCREngine: Created" << result.tokens.size() << "fallback tokens";
}

void OCREngine::stopRunningProcess()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
}

QString OCREngine::mergeParagraphLines(const QStringList &lines, const QVector<OCRResult::OCRToken> &tokens)
{
    if (lines.isEmpty()) return QString();
    if (lines.size() == 1) return lines.first();
    
    // Group tokens by line ID to analyze spatial relationships
    QMap<int, QList<OCRResult::OCRToken>> tokensByLine;
    for (const OCRResult::OCRToken &token : tokens) {
        if (token.lineId >= 0) {
            tokensByLine[token.lineId].append(token);
        }
    }
    
    // Calculate line metrics for paragraph detection
    struct LineInfo {
        QString text;
        int leftMargin = 0;
        int rightEnd = 0;
        int height = 0;
        int centerY = 0;
        bool endsWithPunctuation = false;
        bool startsCapitalized = false;
        bool hasIndentation = false;
    };
    
    QList<LineInfo> lineInfos;
    bool hasTokenData = !tokens.isEmpty();
    
    if (hasTokenData) {
        // Use token-based analysis when available
        QList<int> lineIds = tokensByLine.keys();
        std::sort(lineIds.begin(), lineIds.end());
        
        for (int i = 0; i < lines.size() && i < lineIds.size(); ++i) {
            LineInfo info;
            info.text = lines[i];
            
            // Calculate spatial metrics from tokens
            if (tokensByLine.contains(lineIds[i])) {
                const QList<OCRResult::OCRToken> &lineTokens = tokensByLine[lineIds[i]];
                if (!lineTokens.isEmpty()) {
                    // Find leftmost and rightmost positions
                    info.leftMargin = lineTokens.first().box.left();
                    info.rightEnd = lineTokens.first().box.right();
                    info.centerY = lineTokens.first().box.center().y();
                    info.height = lineTokens.first().box.height();
                    
                    for (const OCRResult::OCRToken &token : lineTokens) {
                        info.leftMargin = qMin(info.leftMargin, token.box.left());
                        info.rightEnd = qMax(info.rightEnd, token.box.right());
                    }
                }
            }
            
            // Text analysis
            QString trimmed = info.text.trimmed();
            if (!trimmed.isEmpty()) {
                // Check if line ends with punctuation (sentence/paragraph enders)
                QChar lastChar = trimmed.at(trimmed.length() - 1);
                info.endsWithPunctuation = (lastChar == '.' || lastChar == '!' || lastChar == '?' || 
                                           lastChar == ':' || lastChar == ';');
                
                // Check if line starts with capital letter (potential new sentence/paragraph)
                info.startsCapitalized = trimmed.at(0).isUpper();
            }
            
            lineInfos.append(info);
        }
    } else {
        // Fallback to text-based analysis when no token data
        for (int i = 0; i < lines.size(); ++i) {
            LineInfo info;
            info.text = lines[i];
            
            // Analyze text indentation (spaces/tabs at start)
            QString originalLine = lines[i];
            int leadingSpaces = 0;
            while (leadingSpaces < originalLine.length() && originalLine.at(leadingSpaces).isSpace()) {
                leadingSpaces++;
            }
            info.hasIndentation = (leadingSpaces > 0);
            
            // Text analysis
            QString trimmed = info.text.trimmed();
            if (!trimmed.isEmpty()) {
                // Check if line ends with punctuation (sentence/paragraph enders)
                QChar lastChar = trimmed.at(trimmed.length() - 1);
                info.endsWithPunctuation = (lastChar == '.' || lastChar == '!' || lastChar == '?' || 
                                           lastChar == ':' || lastChar == ';');
                
                // Check if line starts with capital letter (potential new sentence/paragraph)
                info.startsCapitalized = trimmed.at(0).isUpper();
            }
            
            lineInfos.append(info);
        }
    }
    
    // Detect paragraph breaks and merge accordingly
    QStringList paragraphs;
    QString currentParagraph;
    
    for (int i = 0; i < lineInfos.size(); ++i) {
        const LineInfo &current = lineInfos[i];
        bool shouldStartNewParagraph = false;
        
        if (i == 0) {
            // First line always starts a new paragraph
            shouldStartNewParagraph = true;
        } else {
            const LineInfo &previous = lineInfos[i - 1];
            
            // Heuristics for paragraph detection:
            bool hasSignificantIndent = false;
            bool hasVerticalGap = false;
            
            if (hasTokenData) {
                // Token-based spatial analysis
                // 1. Large indentation change (common in paragraphs)
                int indentDiff = current.leftMargin - previous.leftMargin;
                hasSignificantIndent = indentDiff > 20; // pixels
                
                // 4. Large vertical gap (line spacing indicates paragraph break)
                if (i > 0) {
                    int verticalGap = current.centerY - previous.centerY;
                    int expectedLineSpacing = qMax(previous.height, current.height) * 1.2; // 120% of line height
                    hasVerticalGap = verticalGap > expectedLineSpacing;
                }
            } else {
                // Text-based analysis fallback
                // 1. Text indentation change
                hasSignificantIndent = current.hasIndentation && !previous.hasIndentation;
            }
            
            // 2. Previous line ends with paragraph-ending punctuation
            bool previousEndsParagraph = previous.endsWithPunctuation;
            
            // 3. Current line starts with capital letter (potential new sentence)
            bool currentStartsCapitalized = current.startsCapitalized;
            
            // 5. Very short previous line (often indicates paragraph end)
            bool previousLineShort = previous.text.trimmed().length() < 40;
            
            // Decision logic: Start new paragraph if...
            shouldStartNewParagraph = (hasSignificantIndent && currentStartsCapitalized) ||  // Indented + capitalized
                                     (previousEndsParagraph && hasVerticalGap) ||            // Punctuation + gap
                                     (previousEndsParagraph && hasSignificantIndent) ||      // Punctuation + indent
                                     (previousLineShort && previousEndsParagraph && currentStartsCapitalized); // Short line ending + new sentence
        }
        
        if (shouldStartNewParagraph) {
            if (!currentParagraph.isEmpty()) {
                paragraphs.append(currentParagraph.trimmed());
            }
            currentParagraph = current.text;
        } else {
            // Continue current paragraph - join with space
            if (!currentParagraph.isEmpty()) {
                currentParagraph += " ";
            }
            currentParagraph += current.text.trimmed();
        }
    }
    
    // Add the last paragraph
    if (!currentParagraph.isEmpty()) {
        paragraphs.append(currentParagraph.trimmed());
    }
    
    // Join paragraphs with double line breaks to preserve paragraph structure
    return paragraphs.join("\n\n");
}