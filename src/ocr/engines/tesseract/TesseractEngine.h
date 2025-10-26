#pragma once

#include <QString>
#include <QPixmap>
#include "../../OCREngine.h"

/**
 * Tesseract OCR Engine - Bundled version for production
 * Only uses ./tesseract/tesseract.exe bundled with application
 */
class TesseractEngine
{
public:
    static bool isAvailable();
    static OCRResult performOCR(
        const QPixmap& image,
        const QString& language,
        int qualityLevel,
        bool preprocessing,
        bool autoDetectOrientation
    );

private:
    static QString findTesseractExecutable();
    static QString findTessdataDirectory();
    static QString runTesseractProcess(const QStringList& arguments);
    static OCRResult parseTSVOutput(const QString& tsvOutput, const QSize& imageSize);
};
