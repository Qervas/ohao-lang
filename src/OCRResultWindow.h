#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QScrollArea>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QClipboard>
#include <QApplication>

#include "OCREngine.h"

class OCRResultWindow : public QDialog
{
    Q_OBJECT

public:
    explicit OCRResultWindow(QWidget *parent = nullptr);
    ~OCRResultWindow();

    void showOCRResult(const OCRResult &result, const QPixmap &originalImage);
    void showOCRProgress(const QString &status);
    void showOCRError(const QString &error);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onCopyClicked();
    void onTranslateClicked();
    void onSaveClicked();
    void onCloseClicked();
    void onRetryClicked();

private:
    void setupUI();
    void applyModernStyling();
    void animateShow();
    void updateImageDisplay(const QPixmap &image);

    // UI Components
    QVBoxLayout *mainLayout;
    QHBoxLayout *topLayout;
    QHBoxLayout *buttonLayout;

    // Image display
    QLabel *imageLabel;
    QScrollArea *imageScrollArea;

    // OCR result display
    QGroupBox *resultGroup;
    QTextEdit *resultTextEdit;
    QTextEdit *translationTextEdit;
    QLabel *confidenceLabel;
    QLabel *languageLabel;
    QLabel *engineLabel;

    // Progress display
    QProgressBar *progressBar;
    QLabel *statusLabel;

    // Buttons
    QPushButton *copyBtn;
    QPushButton *translateBtn;
    QPushButton *saveBtn;
    QPushButton *retryBtn;
    QPushButton *closeBtn;

    // Animation
    QPropertyAnimation *showAnimation;
    QGraphicsOpacityEffect *opacityEffect;

    // Data
    OCRResult m_currentResult;
    QPixmap m_originalImage;

signals:
    void retryRequested();
    void translationRequested(const QString &text);
};