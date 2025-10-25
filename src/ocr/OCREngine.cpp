#include "OCREngine.h"
#include "AppleVisionOCR.h"
#include "TranslationEngine.h"
#include "SpellChecker.h"
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
#include <functional>

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
        QString appDir = QCoreApplication::applicationDirPath();
        QString errorMsg = QString("Tesseract OCR not found!\n\nSearched locations:\n- Current directory: %1\n- Subdirectories (recursive)\n- System PATH\n\nPlease reinstall the application.").arg(appDir);
        qDebug() << "Tesseract not available. Application directory:" << appDir;
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
#endif

        for (const QString &dirPath : candidateDirs) {
            if (QDir(dirPath).exists() && QFile::exists(dirPath + "/eng.traineddata")) {
                // Set TESSDATA_PREFIX to the tessdata directory itself
                qputenv("TESSDATA_PREFIX", dirPath.toUtf8());
                emit ocrProgress(QString("Set TESSDATA_PREFIX to %1").arg(dirPath));
                qDebug() << "Auto-detected tessdata at:" << dirPath << "Setting TESSDATA_PREFIX to:" << dirPath;
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

    // Prepare Tesseract command (TSV output for token-level data)
    QStringList arguments;
    arguments << processedImagePath;
    arguments << "stdout";
    arguments << "tsv"; // Output TSV format with positional data

    // Robust tessdata detection: prefer explicit env, otherwise scan common directories
    QString tessdataDir;
    if (!qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        // TESSDATA_PREFIX should point to tessdata dir or parent containing tessdata/
        QString prefix = QString::fromUtf8(qgetenv("TESSDATA_PREFIX"));
        if (QFileInfo(prefix + "/tessdata/eng.traineddata").exists()) {
            tessdataDir = prefix + "/tessdata";
        } else if (QFileInfo(prefix + "/eng.traineddata").exists()) {
            tessdataDir = prefix; // already at tessdata layer
        }
    }
    if (tessdataDir.isEmpty()) {
        QStringList probe;
        // Bundled tessdata (first priority - included in release package)
        probe << QCoreApplication::applicationDirPath() + "/tesseract/tessdata";
#ifdef Q_OS_WIN
        probe << "C:/Program Files/Tesseract-OCR/tessdata";
        probe << "C:/Program Files (x86)/Tesseract-OCR/tessdata";
#endif
        for (const QString &p : probe) {
            if (QFileInfo(p + "/eng.traineddata").exists()) { tessdataDir = p; break; }
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

    // OCR Engine Mode (OEM) - LSTM neural engine for non-English languages
    // LSTM is critical for:
    // - Latin scripts with diacritics (French, Spanish, German, Italian, Portuguese, Swedish, Dutch, Polish, Vietnamese)
    // - Cyrillic script (Russian)
    // - CJK scripts (Chinese, Japanese, Korean) - MANDATORY
    // - Arabic script (right-to-left, contextual forms)
    // - Devanagari (Hindi), Thai, and other complex scripts
    // LSTM is 10x better than legacy engine for non-ASCII characters
    bool needsLSTM = (m_language != "English" && m_language != "Auto-Detect") ||
                     (m_autoDetectOrientation && m_qualityLevel >= 4);

    if (needsLSTM) {
        arguments << "--oem" << "1"; // LSTM neural network engine
    }

    // Let Tesseract use the full character set for the selected language
    // No character whitelist restrictions - improves recognition of long paragraphs

    emit ocrProgress("Running Tesseract OCR (TSV mode)...");

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OCREngine::onTesseractFinished);

    // Find Tesseract executable using our robust search (current dir, subdirs, PATH)
    QString tesseractExe = findTesseractExecutable();
    if (tesseractExe.isEmpty()) {
        emit ocrError("Tesseract executable not found");
        return;
    }
    qDebug() << "===== TESSERACT OCR DEBUG =====";
    qDebug() << "Using Tesseract:" << tesseractExe;
    qDebug() << "Full command:" << tesseractExe << arguments.join(" ");

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
    qDebug() << "===== TESSERACT FINISHED =====";
    qDebug() << "Exit code:" << exitCode;
    qDebug() << "Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");

    OCRResult result;

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        result.success = false;
        result.errorMessage = m_process->readAllStandardError();
        qDebug() << "Tesseract ERROR:" << result.errorMessage;
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = "Tesseract process failed with exit code " + QString::number(exitCode);
        }
    } else {
        QString output = m_process->readAllStandardOutput();
        // Parse TSV directly from output
        QTextStream ts(&output, QIODevice::ReadOnly);
        QString header = ts.readLine();
        Q_UNUSED(header)

        // Structure to hold line information for proper sorting
        struct LineData {
            int lineId;
            QString text;
            int topY;      // Y coordinate (for vertical sorting)
            int leftX;     // X coordinate (for horizontal sorting)
        };

        QMap<int, LineData> lineMap; // lineId -> line data
        int tokenCount = 0;

        while (!ts.atEnd()) {
            QString line = ts.readLine();
            if (line.isEmpty()) continue;
            QStringList cols = line.split('\t');
            if (cols.size() < 12) continue;
            int level = cols[0].toInt();
            if (level != 5) continue; // word level

            // Tesseract TSV columns: level page_num block_num par_num line_num word_num left top width height conf text
            int blockNum = cols[2].toInt();
            int parNum = cols[3].toInt();
            int lineNumInPar = cols[4].toInt();
            int left = cols[6].toInt();
            int top = cols[7].toInt();
            int width = cols[8].toInt();
            int height = cols[9].toInt();
            float conf = cols[10].toFloat();
            QString tokenText = cols[11];
            if (tokenText.trimmed().isEmpty()) continue;

            // Create composite line ID: block * 10000 + paragraph * 100 + line
            int lineId = blockNum * 10000 + parNum * 100 + lineNumInPar;

            OCRResult::OCRToken token;
            token.text = tokenText;
            token.box = QRect(left, top, width, height);
            token.confidence = conf;
            token.lineId = lineId;
            result.tokens.push_back(token);

            if (!lineMap.contains(lineId)) {
                LineData data;
                data.lineId = lineId;
                data.text = tokenText;
                data.topY = top;
                data.leftX = left;
                lineMap[lineId] = data;
            } else {
                lineMap[lineId].text += " " + tokenText;
                // Update leftmost X position
                if (left < lineMap[lineId].leftX) {
                    lineMap[lineId].leftX = left;
                }
            }
            ++tokenCount;
        }

        // Sort lines by reading order: top-to-bottom, then left-to-right
        QList<LineData> sortedLines = lineMap.values();
        std::sort(sortedLines.begin(), sortedLines.end(), [](const LineData &a, const LineData &b) {
            // Define a threshold for "same line" vertically (within 10 pixels)
            const int verticalThreshold = 10;

            if (qAbs(a.topY - b.topY) < verticalThreshold) {
                // Same horizontal line, sort by X position (left to right)
                return a.leftX < b.leftX;
            } else {
                // Different vertical position, sort by Y (top to bottom)
                return a.topY < b.topY;
            }
        });

        // Reconstruct text in proper reading order
        QStringList lines;
        for (const LineData &lineData : sortedLines) {
            lines << lineData.text;
        }
        
        // Apply intelligent paragraph merging
        result.text = mergeParagraphLines(lines, result.tokens);

        // Apply language-specific character corrections
        result.text = correctLanguageSpecificCharacters(result.text, m_language);

        // Apply spellcheck correction using Hunspell
        auto spellChecker = getSpellChecker(m_language);
        if (spellChecker) {
            qDebug() << "Applying Hunspell spellcheck for" << m_language;
            QString originalText = result.text;
            result.text = spellChecker->correctText(result.text);

            if (originalText != result.text) {
                qDebug() << "Spellcheck made corrections:";
                qDebug() << "  Before:" << originalText.left(100);
                qDebug() << "  After:" << result.text.left(100);
            }
        }

        result.success = !result.text.isEmpty();
        result.confidence = "N/A";
        result.language = m_language;

        qDebug() << "Tesseract TSV parsing result:";
        qDebug() << "  Tokens found:" << result.tokens.size();
        qDebug() << "  Text length:" << result.text.length();
        qDebug() << "  Text preview:" << result.text.left(200);
        qDebug() << "  Success:" << result.success;

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

QString OCREngine::findTesseractExecutable()
{
    // Helper function to recursively search for tesseract executable
    // Returns empty string if not found

#ifdef Q_OS_WIN
    QString exeName = "tesseract.exe";
#else
    QString exeName = "tesseract";
#endif

    // Helper lambda for recursive directory search with depth limit
    std::function<QString(const QString&, int)> searchDirectory = [&](const QString& dirPath, int maxDepth) -> QString {
        if (maxDepth <= 0) return QString();

        QDir dir(dirPath);
        if (!dir.exists()) return QString();

        // Check current directory for tesseract executable
        QString candidatePath = dir.filePath(exeName);
        if (QFile::exists(candidatePath)) {
            // Verify it's a valid executable by running --version
            QProcess process;
            process.start(candidatePath, QStringList() << "--version");
            if (process.waitForFinished(3000) && process.exitCode() == 0) {
                qDebug() << "Found Tesseract at:" << candidatePath;
                return candidatePath;
            }
        }

        // Recursively search subdirectories
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subdir : subdirs) {
            QString found = searchDirectory(dir.filePath(subdir), maxDepth - 1);
            if (!found.isEmpty()) {
                return found;
            }
        }

        return QString();
    };

    // Search order as requested by user:
    // 1. Current directory (applicationDirPath)
    // 2. Current subdirectories recursively (depth 3)
    QString appDir = QCoreApplication::applicationDirPath();
    qDebug() << "Searching for Tesseract in application directory:" << appDir;
    QString found = searchDirectory(appDir, 3);
    if (!found.isEmpty()) {
        return found;
    }

    // 3. System PATH - check if "tesseract" command works
    QProcess process;
    process.start("tesseract", QStringList() << "--version");
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        qDebug() << "Found Tesseract in system PATH";
        return "tesseract"; // Return command name for PATH-based execution
    }

    qDebug() << "Tesseract not found in application directory, subdirectories, or system PATH";
    return QString(); // Not found
}

bool OCREngine::isTesseractAvailable()
{
    QString tesseractPath = findTesseractExecutable();
    return !tesseractPath.isEmpty();
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

        // STEP 1: Remove foreign diacritics that don't belong in Swedish
        QMap<QString, QString> swedishCleanup = {
            // Portuguese/Spanish tildes → plain letters (Swedish doesn't use tildes)
            {"ẽ", "e"}, {"ã", "a"}, {"õ", "o"},
            {"Ẽ", "E"}, {"Ã", "A"}, {"Õ", "O"},

            // French accents (grave, acute, circumflex - Swedish uses ä/ö/å instead)
            {"é", "e"}, {"è", "e"}, {"ê", "e"},
            {"à", "a"}, {"â", "a"},
            {"ù", "u"}, {"û", "u"}, {"ú", "u"},
            {"î", "i"}, {"ï", "i"}, {"í", "i"},

            // Spanish/Polish n
            {"ñ", "n"}, {"ń", "n"},

            // French cedilla and ligatures
            {"ç", "c"},
            {"œ", "oe"}, {"æ", "ae"},
        };

        for (auto it = swedishCleanup.begin(); it != swedishCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions specific to Swedish
        QMap<QString, QString> swedishCorrections = {
            // ä - Various OCR misrecognitions
            {"ä", "ä"},      // Sometimes encoded wrong
            {"a\"", "ä"},    // OCR sees ä as a with quotes
            {"a'", "ä"},     // OCR sees ä as a with apostrophe
            {"ã", "ä"},      // Sometimes confused with tilde
            {"à", "ä"},      // Sometimes confused with grave accent
            {"â", "ä"},      // Sometimes confused with circumflex

            // ö - Various OCR misrecognitions
            {"ö", "ö"},      // Sometimes encoded wrong
            {"o\"", "ö"},    // OCR sees ö as o with quotes
            {"o'", "ö"},     // OCR sees ö as o with apostrophe
            {"õ", "ö"},      // Sometimes confused with tilde
            {"ô", "ö"},      // Sometimes confused with circumflex
            {"ò", "ö"},      // Sometimes confused with grave accent

            // å - Various OCR misrecognitions
            {"å", "å"},      // Sometimes encoded wrong
            {"a°", "å"},     // OCR sees å as a with degree symbol
            {"ao", "å"},     // OCR sees å as a followed by o
            {"ã", "å"},      // Sometimes confused (rare)
            {"ª", "å"},      // Feminine ordinal confused with å

            // Capital versions - comprehensive
            {"Ä", "Ä"},      {"A\"", "Ä"},    {"A'", "Ä"},
            {"Ã", "Ä"},      {"À", "Ä"},      {"Â", "Ä"},
            {"Ö", "Ö"},      {"O\"", "Ö"},    {"O'", "Ö"},
            {"Õ", "Ö"},      {"Ô", "Ö"},      {"Ò", "Ö"},
            {"Å", "Å"},      {"A°", "Å"},     {"AO", "Å"},
            {"Ao", "Å"},     {"ª", "Å"}
        };

        // Apply character corrections
        for (auto it = swedishCorrections.begin(); it != swedishCorrections.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // Intelligent Swedish word dictionary - Fix OCR misrecognitions
        // Maps commonly misread words (without diacritics) to correct Swedish spelling
        QMap<QString, QString> swedishWordCorrections = {
            // Critical words with ä
            {"tackmantel", "täckmantel"},   // cover/mantle (FIXES USER'S EXAMPLE!)
            {"har", "här"},                 // here (vs. have)
            {"aven", "även"},               // also/even
            {"val", "väl"},                 // well
            {"var", "vår"},                 // our/spring (context-dependent, but vår more common)
            {"alska", "älska"},             // love
            {"andra", "ändra"},             // change (vs. second/others - context)
            {"lat", "låt"},                 // let/song
            {"nagot", "något"},             // something
            {"manader", "månader"},         // months

            // Critical words with ö
            {"for", "för"},                 // for
            {"hor", "hör"},                 // hear
            {"mor", "mör"},                 // tender
            {"kon", "kön"},                 // sex/gender
            {"kott", "kött"},               // meat
            {"hoger", "höger"},             // right
            {"moter", "möter"},             // meets
            {"oronen", "öronen"},           // ears
            {"folja", "följa"},             // follow
            {"tro", "tro"},                 // believe (already correct, but common)

            // Critical words with å
            {"ar", "år"},                   // year/is (context-dependent)
            {"pa", "på"},                   // on
            {"da", "då"},                   // then
            {"ga", "gå"},                   // go/walk
            {"ma", "må"},                   // may/feel
            {"sta", "stå"},                 // stand
            {"fa", "få"},                   // get/few/sheep
            {"sa", "så"},                   // so/sow
            {"ater", "åter"},               // again
            {"aterkommer", "återkommer"},   // returns
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

        // STEP 1: Remove foreign diacritics that don't belong in French
        QMap<QString, QString> frenchCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},

            // Spanish (French uses different accents)
            {"ñ", "n"}, {"Ñ", "N"},
            {"á", "a"}, {"Á", "A"},  // French uses à not á
            {"í", "i"}, {"Í", "I"},
            {"ó", "o"}, {"Ó", "O"},  // French uses ô not ó
            {"ú", "u"}, {"Ú", "U"},  // French uses ù/û not ú

            // Portuguese tildes
            {"ã", "a"}, {"Ã", "A"},
            {"õ", "o"}, {"Õ", "O"},

            // German
            {"ß", "ss"},
        };

        for (auto it = frenchCleanup.begin(); it != frenchCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions
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

        // STEP 1: Remove foreign diacritics that don't belong in Spanish
        QMap<QString, QString> spanishCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ä", "a"}, {"Ä", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Portuguese tildes (Spanish doesn't use tildes on a/o, only on n)
            {"ã", "a"}, {"Ã", "A"},
            {"õ", "o"}, {"Õ", "O"},

            // French accents (Spanish uses acute, not grave/circumflex)
            {"à", "a"}, {"À", "A"},
            {"è", "e"}, {"È", "E"},
            {"ê", "e"}, {"Ê", "E"},
            {"ë", "e"}, {"Ë", "E"},
            {"î", "i"}, {"Î", "I"},
            {"ï", "i"}, {"Ï", "I"},
            {"ô", "o"}, {"Ô", "O"},
            {"ù", "u"}, {"Ù", "U"},
            {"û", "u"}, {"Û", "U"},
            {"ç", "c"}, {"Ç", "C"},  // Spanish uses c/z not ç

            // German
            {"ß", "ss"},
        };

        for (auto it = spanishCleanup.begin(); it != spanishCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions
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

        // STEP 1: Remove foreign diacritics that don't belong in German
        QMap<QString, QString> germanCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},

            // Spanish
            {"ñ", "n"}, {"Ñ", "N"},
            {"á", "a"}, {"Á", "A"},
            {"é", "e"}, {"É", "E"},
            {"í", "i"}, {"Í", "I"},
            {"ó", "o"}, {"Ó", "O"},
            {"ú", "u"}, {"Ú", "U"},

            // Portuguese
            {"ã", "a"}, {"Ã", "A"},
            {"õ", "o"}, {"Õ", "O"},

            // French
            {"à", "a"}, {"À", "A"},
            {"è", "e"}, {"È", "E"},
            {"ê", "e"}, {"Ê", "E"},
            {"î", "i"}, {"Î", "I"},
            {"ô", "o"}, {"Ô", "O"},
            {"ù", "u"}, {"Ù", "U"},
            {"û", "u"}, {"Û", "U"},
            {"ç", "c"}, {"Ç", "C"},
            {"œ", "oe"}, {"æ", "ae"},
        };

        for (auto it = germanCleanup.begin(); it != germanCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions
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

        // STEP 1: Remove foreign diacritics that don't belong in Portuguese
        QMap<QString, QString> portugueseCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ä", "a"}, {"Ä", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Spanish (ñ not in Portuguese, uses nh)
            {"ñ", "n"}, {"Ñ", "N"},

            // French
            {"è", "e"}, {"È", "E"},  // Portuguese uses ê not è
            {"ù", "u"}, {"Ù", "U"},  // Portuguese uses ú not ù
            {"ï", "i"}, {"Ï", "I"},
            {"ë", "e"}, {"Ë", "E"},
            {"ÿ", "y"},

            // German
            {"ü", "u"}, {"Ü", "U"},
            {"ß", "ss"},
        };

        for (auto it = portugueseCleanup.begin(); it != portugueseCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions
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

        // STEP 1: Remove foreign diacritics that don't belong in Italian
        QMap<QString, QString> italianCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ä", "a"}, {"Ä", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Spanish
            {"ñ", "n"}, {"Ñ", "N"},
            {"á", "a"}, {"Á", "A"},  // Italian uses à not á
            {"í", "i"}, {"Í", "I"},  // Italian uses ì not í
            {"ó", "o"}, {"Ó", "O"},  // Italian uses ò not ó
            {"ú", "u"}, {"Ú", "U"},  // Italian uses ù not ú

            // Portuguese
            {"ã", "a"}, {"Ã", "A"},
            {"õ", "o"}, {"Õ", "O"},

            // French
            {"â", "a"}, {"Â", "A"},
            {"ê", "e"}, {"Ê", "E"},
            {"ë", "e"}, {"Ë", "E"},
            {"î", "i"}, {"Î", "I"},
            {"ï", "i"}, {"Ï", "I"},
            {"ô", "o"}, {"Ô", "O"},
            {"û", "u"}, {"Û", "U"},
            {"ç", "c"}, {"Ç", "C"},

            // German
            {"ü", "u"}, {"Ü", "U"},
            {"ß", "ss"},
        };

        for (auto it = italianCleanup.begin(); it != italianCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }

        // STEP 2: Fix OCR character misrecognitions
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
    else if (language == "Dutch") {
        qDebug() << "Applying Dutch-specific character corrections";

        // STEP 1: Remove foreign diacritics that don't belong in Dutch
        QMap<QString, QString> dutchCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ä", "a"}, {"Ä", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Spanish
            {"ñ", "n"}, {"Ñ", "N"},
            {"á", "a"}, {"Á", "A"},
            {"í", "i"}, {"Í", "I"},
            {"ó", "o"}, {"Ó", "O"},
            {"ú", "u"}, {"Ú", "U"},

            // Portuguese
            {"ã", "a"}, {"Ã", "A"},
            {"õ", "o"}, {"Õ", "O"},
            {"ç", "c"}, {"Ç", "C"},

            // French
            {"à", "a"}, {"À", "A"},
            {"è", "e"}, {"È", "E"},
            {"ê", "e"}, {"Ê", "E"},
            {"î", "i"}, {"Î", "I"},
            {"ô", "o"}, {"Ô", "O"},
            {"ù", "u"}, {"Ù", "U"},
            {"û", "u"}, {"Û", "U"},

            // German
            {"ü", "u"}, {"Ü", "U"},
            {"ß", "ss"},
        };

        for (auto it = dutchCleanup.begin(); it != dutchCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "Polish") {
        qDebug() << "Applying Polish-specific character corrections";

        // STEP 1: Remove foreign diacritics that don't belong in Polish
        QMap<QString, QString> polishCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ä", "a"}, {"Ä", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Spanish
            {"ñ", "n"}, {"Ñ", "N"},  // Polish uses ń not ñ

            // French
            {"à", "a"}, {"é", "e"}, {"è", "e"}, {"ê", "e"},
            {"ç", "c"},

            // German
            {"ü", "u"}, {"Ü", "U"},
            {"ß", "ss"},

            // Portuguese
            {"ã", "a"}, {"õ", "o"},
        };

        for (auto it = polishCleanup.begin(); it != polishCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }
    else if (language == "Vietnamese") {
        qDebug() << "Applying Vietnamese-specific character corrections";

        // STEP 1: Remove foreign diacritics that don't belong in Vietnamese
        QMap<QString, QString> vietnameseCleanup = {
            // Swedish
            {"å", "a"}, {"Å", "A"},
            {"ö", "o"}, {"Ö", "O"},

            // Spanish
            {"ñ", "n"}, {"Ñ", "N"},

            // German
            {"ü", "u"}, {"Ü", "U"},
            {"ß", "ss"},

            // French ligatures
            {"œ", "oe"}, {"æ", "ae"},
        };

        for (auto it = vietnameseCleanup.begin(); it != vietnameseCleanup.end(); ++it) {
            correctedText.replace(it.key(), it.value());
        }
    }

    return correctedText;
}

std::shared_ptr<SpellChecker> OCREngine::getSpellChecker(const QString& language)
{
    // Map language names to language codes
    QString langCode;
    if (language == "English") langCode = "en_US";
    else if (language == "Swedish") langCode = "sv_SE";
    else if (language == "French") langCode = "fr_FR";
    else if (language == "German") langCode = "de_DE";
    else if (language == "Spanish") langCode = "es_ES";
    else if (language == "Portuguese") langCode = "pt_PT";
    else if (language == "Italian") langCode = "it_IT";
    else if (language == "Dutch") langCode = "nl_NL";
    else if (language == "Polish") langCode = "pl_PL";
    else if (language == "Russian") langCode = "ru_RU";
    else if (language == "Vietnamese") langCode = "vi_VN";
    else if (language == "Ukrainian") langCode = "uk_UA";
    else if (language == "Danish") langCode = "da_DK";
    else if (language == "Norwegian") langCode = "nb_NO";
    else if (language == "Turkish") langCode = "tr_TR";
    else return nullptr; // No spellchecker for this language

    // Check cache
    if (m_spellCheckers.contains(langCode)) {
        return m_spellCheckers[langCode];
    }

    // Create new spellchecker
    auto spellChecker = std::make_shared<SpellChecker>(langCode);
    if (spellChecker->isLoaded()) {
        m_spellCheckers[langCode] = spellChecker;
        qDebug() << "Created and cached spellchecker for" << language << "(" << langCode << ")";
        return spellChecker;
    }

    qWarning() << "Failed to load spellchecker for" << language << "(" << langCode << ")";
    return nullptr;
}