#include "OCREngine.h"
#include "AppleVisionOCR.h"
#include "TranslationEngine.h"
#include "../common/Platform.h"
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
    
    // Load saved OCR engine from settings, or use platform default
    QString savedEngine = m_settings->value("ocr/engine", "").toString();
    
    if (!savedEngine.isEmpty()) {
        // Use saved preference (matches SettingsWindow internal names)
        if (savedEngine == "AppleVision") {
            m_engine = AppleVision;
        } else {
            // Default to Tesseract for all other cases (including legacy engines)
            m_engine = Tesseract;
        }
        qDebug() << "OCREngine: Loaded saved engine from settings:" << savedEngine;
    } else {
        // Set default engine based on platform AND save it to settings
#ifdef Q_OS_MACOS
        if (isAppleVisionAvailable()) {
            m_engine = AppleVision;
            m_settings->setValue("ocr/engine", "AppleVision");
            qDebug() << "OCREngine: Defaulting to Apple Vision (native macOS OCR) and saving to settings";
        } else {
            m_engine = OnlineOCR;
            m_settings->setValue("ocr/engine", "OnlineOCR");
            qDebug() << "OCREngine: Apple Vision not available, using Online OCR and saving to settings";
        }
#else
        m_engine = Tesseract;  // Default to Tesseract on other platforms
        m_settings->setValue("ocr/engine", "Tesseract");
        qDebug() << "OCREngine: Defaulting to Tesseract on" << PLATFORM_NAME << "and saving to settings";
#endif
        m_settings->sync();  // Ensure it's written to disk immediately
    }
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
    case AppleVision:
        performAppleVisionOCR(image);
        break;
    case Tesseract:
        performTesseractOCR(image);
        break;
    }
}

void OCREngine::performAppleVisionOCR(const QPixmap &image)
{
#ifdef Q_OS_MACOS
    if (!AppleVisionOCR::isAvailable()) {
        emit ocrError("Apple Vision OCR is not available on this system");
        return;
    }

    emit ocrProgress("Running Apple Vision OCR...");

    // Determine recognition level based on quality setting
    AppleVisionOCR::RecognitionLevel level = (m_qualityLevel >= 4) 
        ? AppleVisionOCR::Accurate 
        : AppleVisionOCR::Fast;

    // Map language to Apple Vision format
    QString visionLanguage = m_language;
    if (m_language == "Auto-Detect") {
        visionLanguage = QString(); // Empty string means auto-detect
    }

    // Perform OCR using Apple Vision
    OCRResult result = AppleVisionOCR::performOCR(image, visionLanguage, level);

    // Apply language-specific character corrections if OCR succeeded
    if (result.success && !result.text.isEmpty()) {
        result.text = correctLanguageSpecificCharacters(result.text, m_language);
    }

    // Store the OCR result and start translation if auto-translate is enabled
    m_currentOCRResult = result;

    // Ensure tokens exist before emitting or starting translation
    if (result.success) {
        ensureTokensExist(m_currentOCRResult, image.size());
    }

    if (result.success && m_autoTranslate && !result.text.isEmpty()) {
        startTranslation(result.text);
    } else {
        emit ocrFinished(m_currentOCRResult);
    }
#else
    emit ocrError("Apple Vision OCR is only available on macOS");
#endif
}

void OCREngine::performTesseractOCR(const QPixmap &image)
{
    if (!isTesseractAvailable()) {
#ifdef Q_OS_WIN
        QString bundledPath = QCoreApplication::applicationDirPath() + "/tesseract/tesseract.exe";
#else
        QString bundledPath = QCoreApplication::applicationDirPath() + "/tesseract";
#endif
        QString errorMsg = QString("Tesseract OCR not found!\n\nSearched locations:\n- System PATH\n- Bundled: %1\n\nPlease reinstall the application.").arg(bundledPath);
        qDebug() << "Tesseract not available. Bundled path:" << bundledPath << "Exists:" << QFile::exists(bundledPath);
        emit ocrError(errorMsg);
        return;
    }

    // Ensure TESSDATA_PREFIX is set so languages can load
    if (qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        QStringList candidateDirs;
        // Bundled tessdata (first priority - included in release package)
        candidateDirs << QCoreApplication::applicationDirPath() + "/tesseract/tessdata";
#ifdef Q_OS_WIN
        // Common Windows install locations (fallback)
        candidateDirs << QDir::fromNativeSeparators("C:/Program Files/Tesseract-OCR/tessdata");
        candidateDirs << QDir::fromNativeSeparators("C:/Program Files (x86)/Tesseract-OCR/tessdata");
        // Scoop (user-specific) pattern
        QString home = QDir::homePath();
        candidateDirs << home + "/scoop/apps/tesseract/current/tessdata";
        candidateDirs << home + "/scoop/persist/tesseract/tessdata";
#endif

        for (const QString &dirPath : candidateDirs) {
            if (QDir(dirPath).exists() && QFile::exists(dirPath + "/eng.traineddata")) {
                // Set TESSDATA_PREFIX to the parent directory containing tessdata
                QString parentDir = QFileInfo(dirPath).absolutePath();
                qputenv("TESSDATA_PREFIX", parentDir.toUtf8());
                emit ocrProgress(QString("Set TESSDATA_PREFIX to %1 (tessdata found at %2)").arg(parentDir).arg(dirPath));
                qDebug() << "Auto-detected tessdata at:" << dirPath << "Setting TESSDATA_PREFIX to:" << parentDir;
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
        // Bundled tessdata (first priority - included in release package)
        probe << QCoreApplication::applicationDirPath() + "/tesseract/tessdata";
#ifdef Q_OS_WIN
        QString home = QDir::homePath();
        probe << "C:/Program Files/Tesseract-OCR/tessdata";
        probe << "C:/Program Files (x86)/Tesseract-OCR/tessdata";
        probe << home + "/scoop/apps/tesseract/current/tessdata";
        probe << home + "/scoop/persist/tesseract/tessdata";
#endif
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

    // Determine which Tesseract to use (bundled or system)
    QString tesseractExe = "tesseract"; // Default to system PATH
#ifdef Q_OS_WIN
    QString bundledTesseract = QCoreApplication::applicationDirPath() + "/tesseract/tesseract.exe";
#else
    QString bundledTesseract = QCoreApplication::applicationDirPath() + "/tesseract";
#endif
    if (QFile::exists(bundledTesseract)) {
        tesseractExe = bundledTesseract;
        qDebug() << "Using bundled Tesseract:" << tesseractExe;
    }

    // Propagate TESSDATA_PREFIX if set after our detection
    if (!qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("TESSDATA_PREFIX", qgetenv("TESSDATA_PREFIX"));
        m_process->setProcessEnvironment(env);
    }

    m_process->start(tesseractExe, arguments);
    if (!m_process->waitForStarted(5000)) {
        emit ocrError("Failed to start Tesseract process");
        return;
    }
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
        {"Italian", "ita"},
        {"Dutch", "nld"},
        {"Polish", "pol"},
        {"Swedish", "swe"},      // THIS WAS MISSING!
        {"Arabic", "ara"},
        {"Hindi", "hin"},
        {"Thai", "tha"},
        {"Vietnamese", "vie"},
        {"Auto-Detect", ""}
    };

    return languageMap.value(language, "eng");
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

        // Apply language-specific character corrections
        result.text = correctLanguageSpecificCharacters(result.text, m_language);

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

                    // Apply language-specific character corrections
                    result.text = correctLanguageSpecificCharacters(result.text, m_language);

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

// Static availability checks
bool OCREngine::isAppleVisionAvailable()
{
#ifdef Q_OS_MACOS
    return AppleVisionOCR::isAvailable();
#else
    return false;
#endif
}

bool OCREngine::isTesseractAvailable()
{
    // First, check for bundled Tesseract in application directory
#ifdef Q_OS_WIN
    QString bundledTesseract = QCoreApplication::applicationDirPath() + "/tesseract/tesseract.exe";
#else
    QString bundledTesseract = QCoreApplication::applicationDirPath() + "/tesseract";
#endif
    if (QFile::exists(bundledTesseract)) {
        QProcess process;
        process.start(bundledTesseract, QStringList() << "--version");
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            qDebug() << "Found bundled Tesseract:" << bundledTesseract;
            return true;
        }
    }

    // Fall back to system-installed Tesseract in PATH
    QProcess process;
    process.start("tesseract", QStringList() << "--version");
    return process.waitForFinished(3000) && process.exitCode() == 0;
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
            currentParagraph = current.text.trimmed();
        } else {
            // Continue current paragraph - smart space handling for TTS
            QString trimmedCurrentText = current.text.trimmed();
            if (!currentParagraph.isEmpty() && !trimmedCurrentText.isEmpty()) {
                // Only add space if neither the current paragraph ends with space
                // nor the new text starts with space/punctuation
                QString trimmedParagraph = currentParagraph.trimmed();
                bool needsSpace = !trimmedParagraph.isEmpty() &&
                                !trimmedParagraph.endsWith(' ') &&
                                !trimmedCurrentText.startsWith(' ') &&
                                !trimmedCurrentText.at(0).isPunct();

                if (needsSpace) {
                    currentParagraph = trimmedParagraph + " " + trimmedCurrentText;
                } else {
                    currentParagraph = trimmedParagraph + trimmedCurrentText;
                }
            } else if (!trimmedCurrentText.isEmpty()) {
                currentParagraph += trimmedCurrentText;
            }
        }
    }
    
    // Add the last paragraph
    if (!currentParagraph.isEmpty()) {
        paragraphs.append(currentParagraph.trimmed());
    }
    
    // Join paragraphs optimized for TTS - use single space instead of line breaks
    // to avoid TTS pauses while maintaining natural flow
    QString result = paragraphs.join(" ");

    // Clean up any multiple spaces that might cause TTS hiccups
    QRegularExpression multipleSpaces("\\s+");
    result = result.replace(multipleSpaces, " ").trimmed();

    return result;
}

QString OCREngine::correctLanguageSpecificCharacters(const QString &text, const QString &language)
{
    QString correctedText = text;

    if (language == "Swedish") {
        qDebug() << "Applying Swedish-specific character corrections";

        // SWEDISH ONLY: Only correct characters that specifically should be Swedish
        QMap<QString, QString> swedishCorrections = {
            // Only fix obvious OCR errors that should be Swedish characters
            // ä - ONLY basic ASCII misrecognitions
            {"a\"", "ä"},    // OCR sometimes sees ä as a with quotes
            {"a'", "ä"},     // OCR sometimes sees ä as a with apostrophe

            // ö - ONLY basic ASCII misrecognitions
            {"o\"", "ö"},    // OCR sometimes sees ö as o with quotes
            {"o'", "ö"},     // OCR sometimes sees ö as o with apostrophe

            // å - ONLY basic ASCII misrecognitions
            {"a°", "å"},     // OCR sometimes sees å as a with degree symbol
            {"ao", "å"},     // OCR sometimes sees å as a followed by o

            // Capital versions
            {"A\"", "Ä"},    {"A'", "Ä"},
            {"O\"", "Ö"},    {"O'", "Ö"},
            {"A°", "Å"},     {"AO", "Å"},   {"Ao", "Å"}
        };

        // Apply character corrections
        for (auto it = swedishCorrections.begin(); it != swedishCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // Additional context-based corrections for Swedish
        // Fix common word-level misrecognitions
        QMap<QString, QString> swedishWordCorrections = {
            {"för", "för"},         // Ensure proper ö in common word "för" (for)
            {"kött", "kött"},       // Ensure proper ö in "kött" (meat)
            {"höger", "höger"},     // Ensure proper ö in "höger" (right)
            {"väl", "väl"},         // Ensure proper ä in "väl" (well)
            {"här", "här"},         // Ensure proper ä in "här" (here)
            {"år", "år"},           // Ensure proper å in "år" (year)
            {"på", "på"},           // Ensure proper å in "på" (on)
            {"då", "då"},           // Ensure proper å in "då" (then)
        };

        // Apply word-level corrections (case insensitive)
        for (auto it = swedishWordCorrections.begin(); it != swedishWordCorrections.end(); ++it) {
            QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(it.key()));
            QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
            correctedText.replace(regex, it.value());
        }

        qDebug() << "Swedish corrections applied. Original length:" << text.length()
                 << "Corrected length:" << correctedText.length();
    }

    // Language-specific character corrections - NO MIXING between languages
    else if (language == "French") {
        qDebug() << "Applying French-specific character corrections";
        QMap<QString, QString> frenchCorrections = {
            // French accents - ONLY correct OCR errors, not other language characters
            {"a`", "à"}, {"a'", "á"}, {"a^", "â"}, {"a~", "ã"},
            {"e`", "è"}, {"e'", "é"}, {"e^", "ê"}, {"e\"", "ë"},
            {"i`", "ì"}, {"i'", "í"}, {"i^", "î"}, {"i\"", "ï"},
            {"o`", "ò"}, {"o'", "ó"}, {"o^", "ô"}, {"o~", "õ"},
            {"u`", "ù"}, {"u'", "ú"}, {"u^", "û"}, {"u\"", "ü"},
            {"c,", "ç"}, {"n~", "ñ"},
            // Capital versions
            {"A`", "À"}, {"A'", "Á"}, {"A^", "Â"}, {"A~", "Ã"},
            {"E`", "È"}, {"E'", "É"}, {"E^", "Ê"}, {"E\"", "Ë"},
            {"I`", "Ì"}, {"I'", "Í"}, {"I^", "Î"}, {"I\"", "Ï"},
            {"O`", "Ò"}, {"O'", "Ó"}, {"O^", "Ô"}, {"O~", "Õ"},
            {"U`", "Ù"}, {"U'", "Ú"}, {"U^", "Û"}, {"U\"", "Ü"},
            {"C,", "Ç"}, {"N~", "Ñ"}
        };
        for (auto it = frenchCorrections.begin(); it != frenchCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "Spanish") {
        qDebug() << "Applying Spanish-specific character corrections";
        QMap<QString, QString> spanishCorrections = {
            // Spanish accents - ONLY correct OCR errors, not other language characters
            {"a'", "á"}, {"e'", "é"}, {"i'", "í"}, {"o'", "ó"}, {"u'", "ú"}, {"u\"", "ü"},
            {"n~", "ñ"},
            // Capital versions
            {"A'", "Á"}, {"E'", "É"}, {"I'", "Í"}, {"O'", "Ó"}, {"U'", "Ú"}, {"U\"", "Ü"},
            {"N~", "Ñ"}
        };
        for (auto it = spanishCorrections.begin(); it != spanishCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "German") {
        qDebug() << "Applying German-specific character corrections";
        QMap<QString, QString> germanCorrections = {
            // German umlauts - ONLY correct OCR errors
            {"a\"", "ä"}, {"o\"", "ö"}, {"u\"", "ü"}, {"ss", "ß"},
            {"A\"", "Ä"}, {"O\"", "Ö"}, {"U\"", "Ü"}
        };
        for (auto it = germanCorrections.begin(); it != germanCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "Portuguese") {
        qDebug() << "Applying Portuguese-specific character corrections";
        QMap<QString, QString> portugueseCorrections = {
            // Portuguese accents - ONLY correct OCR errors
            {"a'", "á"}, {"a^", "â"}, {"a~", "ã"}, {"a`", "à"},
            {"e'", "é"}, {"e^", "ê"},
            {"i'", "í"},
            {"o'", "ó"}, {"o^", "ô"}, {"o~", "õ"},
            {"u'", "ú"}, {"u\"", "ü"},
            {"c,", "ç"},
            // Capital versions
            {"A'", "Á"}, {"A^", "Â"}, {"A~", "Ã"}, {"A`", "À"},
            {"E'", "É"}, {"E^", "Ê"},
            {"I'", "Í"},
            {"O'", "Ó"}, {"O^", "Ô"}, {"O~", "Õ"},
            {"U'", "Ú"}, {"U\"", "Ü"},
            {"C,", "Ç"}
        };
        for (auto it = portugueseCorrections.begin(); it != portugueseCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "Italian") {
        qDebug() << "Applying Italian-specific character corrections";
        QMap<QString, QString> italianCorrections = {
            // Italian accents - ONLY correct OCR errors
            {"a'", "á"}, {"a`", "à"},
            {"e'", "é"}, {"e`", "è"},
            {"i'", "í"}, {"i`", "ì"},
            {"o'", "ó"}, {"o`", "ò"},
            {"u'", "ú"}, {"u`", "ù"},
            // Capital versions
            {"A'", "Á"}, {"A`", "À"},
            {"E'", "É"}, {"E`", "È"},
            {"I'", "Í"}, {"I`", "Ì"},
            {"O'", "Ó"}, {"O`", "Ò"},
            {"U'", "Ú"}, {"U`", "Ù"}
        };
        for (auto it = italianCorrections.begin(); it != italianCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }

    return correctedText;
}