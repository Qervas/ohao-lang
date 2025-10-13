#pragma once

#include <QString>
#include <QPixmap>
#include <QStringList>
#include "OCREngine.h"

/**
 * Apple Vision Framework OCR - macOS Native OCR
 * 
 * Uses Apple's Vision framework (VNRecognizeTextRequest) for text recognition.
 * 
 * Features:
 * - Zero dependencies - built into macOS 10.15+
 * - High accuracy with Apple's ML models
 * - Hardware accelerated on Apple Silicon
 * - Supports 50+ languages out of the box
 * - Works completely offline
 * - Returns text with bounding boxes
 * 
 * This is a pure C++ interface that hides the Objective-C++ implementation.
 */
class AppleVisionOCR
{
public:
    enum RecognitionLevel {
        Fast,      // VNRequestTextRecognitionLevelFast - quick recognition
        Accurate   // VNRequestTextRecognitionLevelAccurate - best quality
    };

    /**
     * Check if Apple Vision OCR is available on this system
     * Requires macOS 10.15 (Catalina) or later
     */
    static bool isAvailable();

    /**
     * Perform OCR on an image using Apple Vision framework
     * 
     * @param image The image to process
     * @param language Language hint (e.g., "en-US", "zh-CN") - optional, auto-detect if empty
     * @param level Recognition quality level
     * @return OCRResult with extracted text and token positions
     */
    static OCRResult performOCR(const QPixmap& image, 
                                 const QString& language = QString(),
                                 RecognitionLevel level = Accurate);

    /**
     * Get list of supported language codes
     * Returns all languages supported by the Vision framework
     */
    static QStringList supportedLanguages();

    /**
     * Get display name for a language code
     * e.g., "en-US" → "English (United States)"
     */
    static QString languageDisplayName(const QString& languageCode);
};
