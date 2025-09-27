#pragma once

#include <QObject>
#include <QRect>
#include <QPixmap>
#include "../ocr/OCREngine.h"

class ScreenshotWidget;
class QuickTranslationOverlay;
class LanguageLearningOverlay;

class OverlayManager : public QObject
{
    Q_OBJECT

signals:
    void overlayEscapePressed();

public:
    enum OverlayMode {
        QuickTranslation,
        DeepLearning
    };

    explicit OverlayManager(ScreenshotWidget* parent);
    ~OverlayManager();

    // Main interface
    void performOCR(const QPixmap& image, const QRect& selectionRect, const QPixmap& fullScreenshot = QPixmap());
    void showOCRResults(const OCRResult& result, const QRect& selectionRect, const QPixmap& sourceImage);
    void showProgress(const QString& message);
    void showError(const QString& error);
    void hideAllOverlays();

    // Configuration
    void setOverlayMode(OverlayMode mode);
    OverlayMode getOverlayMode() const;

    // State queries
    bool areOverlaysVisible() const;

private slots:
    void onTTSFinished();
    void onOCRFinished(const OCRResult& result);
    void onOCRProgress(const QString& status);
    void onOCRError(const QString& error);

private:
    void initializeOverlays();
    void initializeOCR();
    void callTTSForResult(const OCRResult& result);
    void showImmediatePreview(const QRect& selectionRect);

    ScreenshotWidget* m_parent;
    QuickTranslationOverlay* m_quickOverlay;
    LanguageLearningOverlay* m_deepOverlay;
    OverlayMode m_currentMode;
    OCRResult m_lastResult;

    // OCR management
    OCREngine* m_ocrEngine;
    QRect m_currentSelectionRect;
    QPixmap m_currentSourceImage;
};