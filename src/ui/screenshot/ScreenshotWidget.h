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


enum class ToolbarButton {
    None,
    Copy,
    Save,
    OCR,
    Cancel
};

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
    void drawToolbar(QPainter &painter);
    QRect getToolbarRect();
    ToolbarButton getButtonAt(QPoint pos);
    void handleToolbarClick(ToolbarButton button);

    QPixmap screenshot;
    QPoint startPoint;
    QPoint endPoint;
    bool selecting;
    bool hasSelection;
    bool showingToolbar;
    QRect toolbarRect;
    QRect copyButtonRect;
    QRect saveButtonRect;
    QRect ocrButtonRect;
    QRect cancelButtonRect;

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

};