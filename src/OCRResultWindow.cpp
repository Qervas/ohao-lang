#include "OCRResultWindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QShowEvent>
#include <QScreen>
#include <QStyle>

OCRResultWindow::OCRResultWindow(QWidget *parent)
    : QDialog(parent)
{
    setupUI(); // Styling handled globally now

    // Setup animations
    opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacityEffect);

    showAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    showAnimation->setDuration(300);
    showAnimation->setEasingCurve(QEasingCurve::OutQuart);
}

OCRResultWindow::~OCRResultWindow()
{
}

void OCRResultWindow::setupUI()
{
    setWindowTitle("OCR Results - Ohao Lang");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setModal(false); // Non-modal so user can interact with other windows
    resize(800, 600);
    setMinimumSize(600, 400);

    // Main layout
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Top layout with image and result side by side
    topLayout = new QHBoxLayout();
    topLayout->setSpacing(20);

    // Image display area
    QGroupBox *imageGroup = new QGroupBox("Original Image");
    QVBoxLayout *imageLayout = new QVBoxLayout(imageGroup);

    imageScrollArea = new QScrollArea();
    imageScrollArea->setMinimumSize(300, 200);
    imageScrollArea->setMaximumSize(400, 300);
    imageScrollArea->setWidgetResizable(false);
    imageScrollArea->setAlignment(Qt::AlignCenter);

    imageLabel = new QLabel();
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setText("No image loaded");
    imageLabel->setObjectName("ocrImagePlaceholder");
    imageLabel->setProperty("placeholder", true);
    imageScrollArea->setWidget(imageLabel);

    imageLayout->addWidget(imageScrollArea);
    topLayout->addWidget(imageGroup);

    // OCR result display area
    resultGroup = new QGroupBox("OCR & Translation Results");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);

    // Horizontal layout for original and translated text
    QHBoxLayout *textLayout = new QHBoxLayout();

    // Original text
    QVBoxLayout *originalLayout = new QVBoxLayout();
    QLabel *originalLabel = new QLabel("Original Text:");
    originalLabel->setObjectName("ocrOriginalLabel");
    originalLayout->addWidget(originalLabel);

    resultTextEdit = new QTextEdit();
    resultTextEdit->setMinimumHeight(120);
    resultTextEdit->setPlaceholderText("OCR results will appear here...");
    originalLayout->addWidget(resultTextEdit);

    textLayout->addLayout(originalLayout);

    // Translated text
    QVBoxLayout *translationLayout = new QVBoxLayout();
    QLabel *translationLabel = new QLabel("Translation:");
    translationLabel->setObjectName("ocrTranslationLabel");
    translationLayout->addWidget(translationLabel);

    translationTextEdit = new QTextEdit();
    translationTextEdit->setMinimumHeight(120);
    translationTextEdit->setPlaceholderText("Translation will appear here...");
    translationTextEdit->setReadOnly(true);
    translationLayout->addWidget(translationTextEdit);

    textLayout->addLayout(translationLayout);
    resultLayout->addLayout(textLayout);

    // Info labels
    QHBoxLayout *infoLayout = new QHBoxLayout();
    confidenceLabel = new QLabel("Confidence: N/A");
    languageLabel = new QLabel("Language: N/A");
    engineLabel = new QLabel("Engine: N/A");

    infoLayout->addWidget(confidenceLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(languageLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(engineLabel);

    resultLayout->addLayout(infoLayout);

    topLayout->addWidget(resultGroup, 1); // Give more space to result area

    mainLayout->addLayout(topLayout);

    // Progress area
    statusLabel = new QLabel("Ready for OCR processing...");
    progressBar = new QProgressBar();
    progressBar->setVisible(false);

    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(progressBar);

    // Button layout
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    retryBtn = new QPushButton("🔄 Retry OCR");
    retryBtn->setObjectName("retryBtn");
    connect(retryBtn, &QPushButton::clicked, this, &OCRResultWindow::onRetryClicked);

    copyBtn = new QPushButton("📋 Copy Text");
    copyBtn->setObjectName("actionBtn");
    connect(copyBtn, &QPushButton::clicked, this, &OCRResultWindow::onCopyClicked);

    translateBtn = new QPushButton("🌐 Translate");
    translateBtn->setObjectName("actionBtn");
    connect(translateBtn, &QPushButton::clicked, this, &OCRResultWindow::onTranslateClicked);

    saveBtn = new QPushButton("💾 Save Text");
    saveBtn->setObjectName("actionBtn");
    connect(saveBtn, &QPushButton::clicked, this, &OCRResultWindow::onSaveClicked);

    closeBtn = new QPushButton("❌ Close");
    closeBtn->setObjectName("closeBtn");
    connect(closeBtn, &QPushButton::clicked, this, &OCRResultWindow::onCloseClicked);

    buttonLayout->addWidget(retryBtn);
    buttonLayout->addSpacing(10);
    buttonLayout->addWidget(copyBtn);
    buttonLayout->addWidget(translateBtn);
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(closeBtn);

    mainLayout->addLayout(buttonLayout);

    // Initially disable buttons that require OCR results
    copyBtn->setEnabled(false);
    translateBtn->setEnabled(false);
    saveBtn->setEnabled(false);

    // Center window on screen
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2, (screenGeometry.height() - height()) / 2);
    }
}

void OCRResultWindow::applyModernStyling() {}

void OCRResultWindow::showOCRResult(const OCRResult &result, const QPixmap &originalImage)
{
    m_currentResult = result;
    m_originalImage = originalImage;

    // Update image display
    updateImageDisplay(originalImage);

    // Update result display
    if (result.success) {
        resultTextEdit->setPlainText(result.text);
        confidenceLabel->setText(QString("Confidence: %1").arg(result.confidence));
        languageLabel->setText(QString("Language: %1").arg(result.language));

        // Update translation display
        if (result.hasTranslation && !result.translatedText.isEmpty()) {
            translationTextEdit->setPlainText(result.translatedText);
            statusLabel->setText("✅ OCR and translation completed successfully!");
            statusLabel->setProperty("status","success");
            statusLabel->style()->unpolish(statusLabel);
            statusLabel->style()->polish(statusLabel);
        } else {
            translationTextEdit->setPlainText("No translation available");
            translationTextEdit->setProperty("hint", true);
            translationTextEdit->style()->unpolish(translationTextEdit);
            translationTextEdit->style()->polish(translationTextEdit);
            statusLabel->setText("✅ OCR completed successfully!");
            statusLabel->setProperty("status","success");
            statusLabel->style()->unpolish(statusLabel);
            statusLabel->style()->polish(statusLabel);
        }

        // Enable action buttons
        copyBtn->setEnabled(true);
        translateBtn->setEnabled(!result.text.isEmpty());
        saveBtn->setEnabled(!result.text.isEmpty());
    } else {
        resultTextEdit->setPlainText(QString("OCR Error: %1").arg(result.errorMessage));
    translationTextEdit->setPlainText("No translation available due to OCR failure");
    translationTextEdit->setProperty("error", true);
    translationTextEdit->style()->unpolish(translationTextEdit);
    translationTextEdit->style()->polish(translationTextEdit);
        confidenceLabel->setText("Confidence: N/A");
        languageLabel->setText(QString("Language: %1").arg(result.language));

        // Disable action buttons
        copyBtn->setEnabled(false);
        translateBtn->setEnabled(false);
        saveBtn->setEnabled(false);

    statusLabel->setText("❌ OCR failed. Try adjusting settings or using a different engine.");
    statusLabel->setProperty("status","error");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    }

    progressBar->setVisible(false);
}

void OCRResultWindow::showOCRProgress(const QString &status)
{
    statusLabel->setText(status);
    statusLabel->setProperty("status","progress");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // Indeterminate progress
}

void OCRResultWindow::showOCRError(const QString &error)
{
    statusLabel->setText(QString("❌ Error: %1").arg(error));
    statusLabel->setProperty("status","error");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    progressBar->setVisible(false);

    resultTextEdit->setPlainText(QString("OCR processing failed: %1").arg(error));

    // Disable action buttons
    copyBtn->setEnabled(false);
    translateBtn->setEnabled(false);
    saveBtn->setEnabled(false);
}

void OCRResultWindow::updateImageDisplay(const QPixmap &image)
{
    if (image.isNull()) {
        imageLabel->setText("No image loaded");
        imageLabel->setProperty("placeholder", true);
        imageLabel->style()->unpolish(imageLabel);
        imageLabel->style()->polish(imageLabel);
        return;
    }

    // Scale image to fit in display area while maintaining aspect ratio
    QSize displaySize = imageScrollArea->size() - QSize(20, 20); // Account for margins
    QPixmap scaledImage = image.scaled(displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    imageLabel->setPixmap(scaledImage);
    imageLabel->setProperty("placeholder", false);
    imageLabel->style()->unpolish(imageLabel);
    imageLabel->style()->polish(imageLabel);
    imageLabel->adjustSize();
}

void OCRResultWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    animateShow();
}

void OCRResultWindow::animateShow()
{
    opacityEffect->setOpacity(0.0);
    showAnimation->setStartValue(0.0);
    showAnimation->setEndValue(1.0);
    showAnimation->start();
}

void OCRResultWindow::onCopyClicked()
{
    if (!m_currentResult.success || m_currentResult.text.isEmpty()) {
        QMessageBox::warning(this, "Copy Text", "No text available to copy.");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();

    QString textToCopy = m_currentResult.text;

    // If we have translation, copy both
    if (m_currentResult.hasTranslation && !m_currentResult.translatedText.isEmpty()) {
        textToCopy = QString("Original: %1\n\nTranslation: %2")
                        .arg(m_currentResult.text)
                        .arg(m_currentResult.translatedText);
    }

    clipboard->setText(textToCopy);

    statusLabel->setText("✅ Text copied to clipboard!");
    statusLabel->setProperty("status","success");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);

    // Reset status after 3 seconds
    QTimer::singleShot(3000, [this]() {
        if (m_currentResult.hasTranslation) {
            statusLabel->setText("OCR and translation completed successfully!");
        } else {
            statusLabel->setText("OCR completed successfully!");
        }
    });
}

void OCRResultWindow::onTranslateClicked()
{
    if (!m_currentResult.success || m_currentResult.text.isEmpty()) {
        QMessageBox::warning(this, "Translate Text", "No text available to translate.");
        return;
    }

    emit translationRequested(m_currentResult.text);
    statusLabel->setText("🌐 Opening translation...");
    statusLabel->setProperty("status","progress");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
}

void OCRResultWindow::onSaveClicked()
{
    if (!m_currentResult.success || m_currentResult.text.isEmpty()) {
        QMessageBox::warning(this, "Save Text", "No text available to save.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                                   "Save OCR Text",
                                                   "ocr_result.txt",
                                                   "Text Files (*.txt);;All Files (*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save Error", "Failed to save file: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream << "OCR Results - Ohao Lang\n";
    stream << "========================\n\n";
    stream << "Engine: " << engineLabel->text() << "\n";
    stream << "Language: " << m_currentResult.language << "\n";
    stream << "Confidence: " << m_currentResult.confidence << "\n\n";
    stream << "Extracted Text:\n";
    stream << "---------------\n";
    stream << m_currentResult.text;

    statusLabel->setText("✅ Text saved successfully!");
    statusLabel->setProperty("status","success");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
}

void OCRResultWindow::onRetryClicked()
{
    emit retryRequested();
}

void OCRResultWindow::onCloseClicked()
{
    close();
}