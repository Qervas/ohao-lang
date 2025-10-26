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

    // Build Tesseract arguments
    QStringList arguments;
    arguments << processedImagePath;  // Use preprocessed image
    arguments << "stdout";  // Output to stdout
    arguments << "tsv";     // TSV format

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

    // LSTM
    bool useLSTM = TesseractConfig::shouldUseLSTM(language, qualityLevel, autoDetectOrientation);
    if (useLSTM) {
        arguments << "--oem" << "1";
    }

    // Character whitelist - DISABLED to respect Tesseract's original output
    // QString whitelist = TesseractConfig::getCharacterWhitelist(language);
    // if (!whitelist.isEmpty()) {
    //     arguments << "-c" << ("tessedit_char_whitelist=" + whitelist);
    // }

    // Run Tesseract
    QString tsvOutput = runTesseractProcess(arguments);

    if (tsvOutput.isEmpty()) {
        result.errorMessage = "Tesseract returned empty output";
        QFile::remove(imagePath);
        return result;
    }

    // Parse TSV
    result = parseTSVOutput(tsvOutput, image.size());
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
    QString tessdataPath = appDir + "/tesseract/tessdata";

    if (QFile::exists(tessdataPath + "/eng.traineddata")) {
        return tessdataPath;
    }
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

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

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

    return result;
}
