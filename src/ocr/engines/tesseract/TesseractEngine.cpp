#include "TesseractEngine.h"
#include "TesseractConfig.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QMessageBox>

bool TesseractEngine::isAvailable()
{
    QString tesseractPath = findTesseractExecutable();
    return !tesseractPath.isEmpty();
}

OCRResult TesseractEngine::performOCR(
    const QPixmap& image,
    const QString& language,
    int qualityLevel,
    bool preprocessing,
    bool autoDetectOrientation)
{
    OCRResult result;
    result.success = false;

    if (!isAvailable()) {
        result.errorMessage = "Bundled Tesseract not found";
        QMessageBox::critical(nullptr, "OCR Error", "Bundled Tesseract not found at: " + QCoreApplication::applicationDirPath() + "/tesseract/tesseract.exe");
        return result;
    }

    // Save image to temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/ohao-ocr";
    QDir().mkpath(tempDir);
    QString imagePath = tempDir + "/img_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

    if (!image.save(imagePath, "PNG")) {
        result.errorMessage = "Failed to save image";
        return result;
    }

    // Preprocess image if enabled (grayscale, sharpen, contrast)
    QString processedImagePath = imagePath;
    if (preprocessing) {
        QProcess convertProcess;
        QString preprocessedPath = imagePath;
        preprocessedPath.replace(".png", "_preprocessed.png");

        QStringList convertArgs;
        convertArgs << imagePath;
        convertArgs << "-colorspace" << "Gray";       // Grayscale
        convertArgs << "-sharpen" << "0x1";           // Sharpen edges
        convertArgs << "-contrast-stretch" << "0";    // Auto contrast
        convertArgs << preprocessedPath;

        convertProcess.start("convert", convertArgs);
        if (convertProcess.waitForFinished(10000) && convertProcess.exitCode() == 0) {
            processedImagePath = preprocessedPath;
        }
    }

    // Build Tesseract arguments (plain text output for maximal compatibility)
    QStringList arguments;
    arguments << processedImagePath;  // Use preprocessed image
    arguments << "stdout";  // Output to stdout (plain text)

    // Tessdata directory
    QString tessdataDir = findTessdataDirectory();
    if (!tessdataDir.isEmpty()) {
        arguments << "--tessdata-dir" << tessdataDir;
    }

    // Language
    QString langCode = TesseractConfig::getLanguageCode(language);
    if (!langCode.isEmpty()) {
        arguments << "-l" << langCode;
    }

    // PSM (Page Segmentation Mode)
    int psm = TesseractConfig::getPSMForQualityLevel(qualityLevel);
    arguments << "--psm" << QString::number(psm);

    // OCR Engine Mode (OEM)
    // Let Tesseract auto-detect the best mode (OEM 3) based on traineddata
    // Only force LSTM (OEM 1) for non-English languages
    bool useLSTM = TesseractConfig::shouldUseLSTM(language, qualityLevel, autoDetectOrientation);
    if (useLSTM) {
        arguments << "--oem" << "1";  // Force LSTM mode for better accuracy
    }
    // Don't set OEM for English - let it auto-detect (OEM 3 default)

    // No whitelist or DAWG tweaks — let the language model output native diacritics

    // Log the complete Tesseract command for debugging
    qDebug() << "===== TESSERACT COMMAND =====";
    qDebug() << "Language:" << language << "-> Code:" << langCode;
    qDebug() << "Arguments:" << arguments.join(" ");
    qDebug() << "============================";

    // Run Tesseract (plain text)
    QString textOutput = runTesseractProcess(arguments);

    if (textOutput.isEmpty()) {
        result.errorMessage = "Tesseract returned empty output";
        QFile::remove(imagePath);
        return result;
    }

    // Plain text result
    result.text = textOutput.trimmed();
    result.success = !result.text.isEmpty();
    result.language = language;

    // Cleanup temp files
    QFile::remove(imagePath);
    if (processedImagePath != imagePath) {
        QFile::remove(processedImagePath);
    }

    return result;
}

QString TesseractEngine::findTesseractExecutable()
{
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    QString path = appDir + "/tesseract/tesseract.exe";
#else
    QString path = appDir + "/tesseract/tesseract";
#endif

    QProcess process;
    process.start(path, QStringList() << "--version");
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        return path;
    }
    return QString();
}

QString TesseractEngine::findTessdataDirectory()
{
    QString appDir = QCoreApplication::applicationDirPath();

    // 1) Bundled tessdata next to the app (preferred per request)
    QString bundled = appDir + "/tesseract/tessdata";
    if (QFile::exists(bundled + "/eng.traineddata")) {
        qDebug() << "Using bundled tessdata:" << bundled;
        return bundled;
    }

    // 2) Allow TESSDATA_PREFIX override
    if (!qEnvironmentVariableIsEmpty("TESSDATA_PREFIX")) {
        QString prefix = QString::fromUtf8(qgetenv("TESSDATA_PREFIX"));
        QString candidate = prefix.endsWith("/tessdata") || prefix.endsWith("\\tessdata")
            ? prefix
            : (prefix + "/tessdata");
        if (QFile::exists(candidate + "/eng.traineddata")) {
            qDebug() << "Using tessdata from TESSDATA_PREFIX:" << candidate;
            return candidate;
        }
    }

    // 3) Fall back to system installation (Scoop path)
#ifdef Q_OS_WIN
    QString home = QDir::homePath();
    QString scoopTessdata = home + "/scoop/persist/tesseract/tessdata";
    if (QFile::exists(scoopTessdata + "/eng.traineddata")) {
        qDebug() << "Using Scoop tessdata:" << scoopTessdata;
        return scoopTessdata;
    }
#endif

    qDebug() << "WARNING: No tessdata directory found!";
    return QString();
}

QString TesseractEngine::runTesseractProcess(const QStringList& arguments)
{
    QString tesseractPath = findTesseractExecutable();

    QProcess process;

    // Don't set TESSDATA_PREFIX - we use --tessdata-dir argument instead
    // Setting both causes confusion

    process.start(tesseractPath, arguments);

    if (!process.waitForStarted(5000)) {
        QMessageBox::critical(nullptr, "OCR Error", "Failed to start Tesseract process");
        return QString();
    }

    if (!process.waitForFinished(60000)) {
        process.kill();
        QMessageBox::critical(nullptr, "OCR Error", "Tesseract process timed out");
        return QString();
    }

    // CRITICAL: Tesseract outputs UTF-8, must decode explicitly!
    // readAllStandardOutput() returns QByteArray - we MUST use fromUtf8()
    // Otherwise Windows default codec (Latin-1/Windows-1252) will corrupt åäö, éèê, etc.
    QByteArray outputBytes = process.readAllStandardOutput();
    QByteArray errorBytes = process.readAllStandardError();

    QString output = QString::fromUtf8(outputBytes);
    QString errorOutput = QString::fromUtf8(errorBytes);

    qDebug() << "Tesseract raw output bytes (first 200):" << outputBytes.left(200).toHex();
    qDebug() << "Tesseract UTF-8 decoded (first 200 chars):" << output.left(200);

    if (process.exitCode() != 0) {
        QMessageBox::critical(nullptr, "OCR Error",
            "Tesseract failed with exit code " + QString::number(process.exitCode()) +
            "\nError: " + errorOutput);
        return QString();
    }

    return output;
}

OCRResult TesseractEngine::parseTSVOutput(const QString& tsvOutput, const QSize& imageSize)
{
    OCRResult result;
    result.success = false;

    QStringList lines = tsvOutput.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return result;
    }

    lines.removeFirst(); // Remove header

    QStringList textLines;
    int currentLineNum = -1;
    QString currentLine;

    for (const QString& line : lines) {
        QStringList fields = line.split('\t');
        if (fields.size() < 12) continue;

        int level = fields[0].toInt();
        if (level != 5) continue; // Word level only

        int lineNum = fields[4].toInt();
        QString text = fields[11].trimmed();  // Trim to remove leading/trailing spaces

        if (text.isEmpty()) continue;

        int left = fields[6].toInt();
        int top = fields[7].toInt();
        int width = fields[8].toInt();
        int height = fields[9].toInt();
        float confidence = fields[10].toFloat();

        OCRResult::OCRToken token;
        token.text = text;
        token.box = QRect(left, top, width, height);
        token.confidence = confidence / 100.0f;
        token.lineId = lineNum;
        result.tokens.push_back(token);

        if (lineNum != currentLineNum) {
            if (!currentLine.isEmpty()) {
                textLines << currentLine.trimmed();
            }
            currentLine = text;
            currentLineNum = lineNum;
        } else {
            if (!currentLine.isEmpty()) {
                currentLine += " ";
            }
            currentLine += text;
        }
    }

    if (!currentLine.isEmpty()) {
        textLines << currentLine.trimmed();
    }

    result.text = textLines.join(" ");  // Join lines with single space
    result.success = !result.text.isEmpty();

    qDebug() << "===== PARSE RESULT =====";
    qDebug() << "Parsed text:" << result.text;
    qDebug() << "Parsed text (UTF-8 bytes):" << result.text.toUtf8().toHex();
    qDebug() << "Number of tokens:" << result.tokens.size();
    qDebug() << "========================";

    return result;
}
