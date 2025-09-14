#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

#include "OCREngine.h"

class OCRResultWindow;

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
    void onOCRFinished(const OCRResult &result);
    void onOCRProgress(const QString &status);
    void onOCRError(const QString &error);
    void onOCRRetryRequested();

private:
    void captureScreen();
    void setupWidget();
    void drawToolbar(QPainter &painter);
    void drawResultsOverlay(QPainter &painter);
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

    // OCR functionality
    OCREngine *ocrEngine;
    OCRResultWindow *ocrResultWindow;
    QPixmap m_lastOCRImage;

    // Results overlay
    bool showingResults;
    OCRResult m_currentResult;
    QString m_progressText;
    QRect m_resultAreaRect;
};