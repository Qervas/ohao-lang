#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

#include "OCREngine.h"
#include "ScreenCapture.h"
#include "../overlays/OverlayManager.h"

class ScreenshotWidget : public QWidget
{
    Q_OBJECT

signals:
    void screenshotFinished();

public:
    ScreenshotWidget(QWidget *parent = nullptr);
    ScreenshotWidget(const QPixmap &screenshot, QWidget *parent = nullptr);
    ~ScreenshotWidget();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void handleCopy();
    void handleSave();
    void handleOCR();
    void handleCancel();

private:
    void captureScreen();
    void setupWidget();

    QPixmap screenshot;
    QPoint startPoint;
    QPoint endPoint;
    bool selecting;
    bool hasSelection;

    QPixmap m_lastOCRImage;

    // Results overlay - now managed by OverlayManager
    bool showingResults;
    OCRResult m_currentResult;
    QString m_progressText;
    QRect m_resultAreaRect;
    OverlayManager *m_overlayManager;
    
    // Track all OCR selections to keep them visible
    QList<QRect> m_ocrSelections;
    
    // Dimming opacity (loaded from settings)
    int m_dimmingOpacity;
    
    // Track first selection after screenshot (workaround for macOS menu bar offset)
    bool m_isFirstSelection;
    QRect m_firstSelectionRect;  // Store first selection coordinates for auto-retry

};