#pragma once

#include <QObject>
#include <QRect>
#include <QPixmap>
#include "../ocr/OCREngine.h"

class ScreenshotWidget;
class QuickTranslationOverlay;

class OverlayManager : public QObject
{
    Q_OBJECT

signals:
    void overlayEscapePressed();

public:
    explicit OverlayManager(ScreenshotWidget* parent);
    ~OverlayManager();

    // Main interface
    void performOCR(const QPixmap& image, const QRect& selectionRect, const QPixmap& fullScreenshot = QPixmap(), const QList<QRect>& existingSelections = QList<QRect>());
    void showOCRResults(const OCRResult& result, const QRect& selectionRect, const QPixmap& sourceImage);
    void showProgress(const QString& message);
    void showError(const QString& error);
    void hideAllOverlays();

    // State queries
    bool areOverlaysVisible() const;
    OCRResult getLastOCRResult() const { return m_lastResult; }

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
    OCRResult m_lastResult;

    // OCR management
    OCREngine* m_ocrEngine;
    QRect m_currentSelectionRect;
    QPixmap m_currentSourceImage;
    QList<QRect> m_existingSelections;
};