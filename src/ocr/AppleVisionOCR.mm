#import "AppleVisionOCR.h"

#ifdef Q_OS_MACOS

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Vision/Vision.h>
#import <CoreImage/CoreImage.h>
#include <QBuffer>
#include <QDebug>

// Helper function to convert QPixmap to CGImage
static CGImageRef QPixmapToCGImage(const QPixmap& pixmap) {
    QImage image = pixmap.toImage();

    // Convert to ARGB32 format if needed
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    // Create CGImage from QImage data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL,
        image.constBits(),
        image.sizeInBytes(),
        NULL
    );

    CGImageRef cgImage = CGImageCreate(
        image.width(),
        image.height(),
        8,
        32,
        image.bytesPerLine(),
        colorSpace,
        kCGBitmapByteOrder32Host | kCGImageAlphaFirst,
        provider,
        NULL,
        false,
        kCGRenderingIntentDefault
    );

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);

    return cgImage;
}

bool AppleVisionOCR::isAvailable() {
    // Check macOS version - Vision text recognition requires 10.15+
    if (@available(macOS 10.15, *)) {
        return true;
    }
    return false;
}

OCRResult AppleVisionOCR::performOCR(const QPixmap& image, const QString& language, RecognitionLevel level) {
    OCRResult result;
    result.success = false;

    if (!isAvailable()) {
        result.errorMessage = "Apple Vision OCR requires macOS 10.15 or later";
        qWarning() << result.errorMessage;
        return result;
    }

    @autoreleasepool {
        // Convert QPixmap to CGImage
        CGImageRef cgImage = QPixmapToCGImage(image);
        if (!cgImage) {
            result.errorMessage = "Failed to convert image";
            qWarning() << "AppleVisionOCR: Failed to convert QPixmap to CGImage";
            return result;
        }

        qDebug() << "AppleVisionOCR: Processing image of size" << image.width() << "x" << image.height();
        qDebug() << "AppleVisionOCR: Language hint:" << (language.isEmpty() ? "auto-detect" : language);
        qDebug() << "AppleVisionOCR: Recognition level:" << (level == Fast ? "Fast" : "Accurate");

        // Create Vision request
        VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc] init];

        // Set recognition level
        if (@available(macOS 10.15, *)) {
            if (level == Fast) {
                request.recognitionLevel = VNRequestTextRecognitionLevelFast;
            } else {
                request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
            }
        }

        // Set language hints if provided
        if (!language.isEmpty()) {
            NSString *langCode = language.toNSString();
            NSArray<NSString *> *languages = @[langCode];
            request.recognitionLanguages = languages;
            qDebug() << "AppleVisionOCR: Using language hint:" << language;
        }

        // Set additional options for better accuracy
        request.usesLanguageCorrection = YES;

        // Create request handler
        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];

        // Perform the request
        NSError *error = nil;
        BOOL success = [handler performRequests:@[request] error:&error];

        if (!success || error) {
            NSString *errorDesc = error ? [error localizedDescription] : @"Unknown error";
            result.errorMessage = QString::fromNSString(errorDesc);
            qWarning() << "AppleVisionOCR: Recognition failed:" << result.errorMessage;
            CGImageRelease(cgImage);
            return result;
        }

        // Process results
        NSArray<VNRecognizedTextObservation *> *observations = request.results;
        qDebug() << "AppleVisionOCR: Found" << observations.count << "text observations";

        QStringList textLines;
        QVector<OCRResult::OCRToken> tokens;
        int lineId = 0;

        for (VNRecognizedTextObservation *observation in observations) {
            VNRecognizedText *recognizedText = [observation topCandidates:1].firstObject;
            if (recognizedText) {
                QString text = QString::fromNSString(recognizedText.string);
                float confidence = recognizedText.confidence;

                // Get bounding box in image coordinates
                CGRect boundingBox = observation.boundingBox;

                // Vision uses bottom-left origin, convert to top-left
                int x = boundingBox.origin.x * image.width();
                int y = (1.0 - boundingBox.origin.y - boundingBox.size.height) * image.height();
                int width = boundingBox.size.width * image.width();
                int height = boundingBox.size.height * image.height();

                QRect rect(x, y, width, height);

                // Create token
                OCRResult::OCRToken token;
                token.text = text;
                token.box = rect;
                token.confidence = confidence;
                token.lineId = lineId++;
                tokens.append(token);

                textLines.append(text);

                qDebug() << "  Line" << lineId << ":" << text
                         << "confidence:" << QString::number(confidence, 'f', 2)
                         << "box:" << rect;
            }
        }

        // Clean up
        CGImageRelease(cgImage);

        // Build final result
        result.text = textLines.join("\n");
        result.tokens = tokens;
        result.success = true;
        result.confidence = tokens.isEmpty() ? "0" : QString::number(tokens.first().confidence, 'f', 2);
        result.language = language.isEmpty() ? "auto" : language;

        qDebug() << "AppleVisionOCR: Success! Extracted" << textLines.size() << "lines";
        qDebug() << "AppleVisionOCR: Total text length:" << result.text.length() << "characters";

        return result;
    }
}

QStringList AppleVisionOCR::supportedLanguages() {
    QStringList languages;

    if (!isAvailable()) {
        return languages;
    }

    @autoreleasepool {
        if (@available(macOS 12.0, *)) {
            // Use new API for macOS 12+
            VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc] init];
            NSError *error = nil;
            NSArray<NSString *> *supportedLangs = [request supportedRecognitionLanguagesAndReturnError:&error];
            if (supportedLangs) {
                for (NSString *lang in supportedLangs) {
                    languages.append(QString::fromNSString(lang));
                }
            }
        } else if (@available(macOS 10.15, *)) {
            // Fallback for macOS 10.15-11.x
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            NSArray<NSString *> *supportedLangs = [VNRecognizeTextRequest supportedRecognitionLanguagesForTextRecognitionLevel:VNRequestTextRecognitionLevelAccurate
                                                                                                                      revision:VNRecognizeTextRequestRevision1
                                                                                                                         error:nil];
            #pragma clang diagnostic pop
            for (NSString *lang in supportedLangs) {
                languages.append(QString::fromNSString(lang));
            }
        }
    }

    qDebug() << "AppleVisionOCR: Supported languages:" << languages.size();
    return languages;
}

QString AppleVisionOCR::languageDisplayName(const QString& languageCode) {
    @autoreleasepool {
        NSString *code = languageCode.toNSString();
        NSLocale *locale = [NSLocale currentLocale];
        NSString *displayName = [locale localizedStringForLanguageCode:code];

        if (displayName) {
            return QString::fromNSString(displayName);
        }
        return languageCode;
    }
}

#else

// Stub implementations for non-macOS platforms
bool AppleVisionOCR::isAvailable() {
    return false;
}

OCRResult AppleVisionOCR::performOCR(const QPixmap& image, const QString& language, RecognitionLevel level) {
    OCRResult result;
    result.success = false;
    result.errorMessage = "Apple Vision OCR is only available on macOS";
    return result;
}

QStringList AppleVisionOCR::supportedLanguages() {
    return QStringList();
}

QString AppleVisionOCR::languageDisplayName(const QString& languageCode) {
    return languageCode;
}

#endif // Q_OS_MACOS
