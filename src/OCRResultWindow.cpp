#include "OCRResultWindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QShowEvent>
#include <QScreen>

OCRResultWindow::OCRResultWindow(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    applyModernStyling();

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
    imageLabel->setStyleSheet("border: 2px dashed #ccc; padding: 20px;");
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
    originalLabel->setStyleSheet("font-weight: bold; color: #2D3748;");
    originalLayout->addWidget(originalLabel);

    resultTextEdit = new QTextEdit();
    resultTextEdit->setMinimumHeight(120);
    resultTextEdit->setPlaceholderText("OCR results will appear here...");
    originalLayout->addWidget(resultTextEdit);

    textLayout->addLayout(originalLayout);

    // Translated text
    QVBoxLayout *translationLayout = new QVBoxLayout();
    QLabel *translationLabel = new QLabel("Translation:");
    translationLabel->setStyleSheet("font-weight: bold; color: #2D3748;");
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

void OCRResultWindow::applyModernStyling()
{
    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(240, 242, 245, 0.95),
                stop:1 rgba(250, 252, 255, 0.95));
            border-radius: 15px;
            border: 1px solid rgba(200, 200, 210, 0.3);
        }

        QGroupBox {
            font-weight: 600;
            font-size: 14px;
            color: #2D3748;
            border: 2px solid rgba(200, 200, 210, 0.2);
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background: rgba(255, 255, 255, 0.4);
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }

        QTextEdit {
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 8px;
            background: rgba(255, 255, 255, 0.9);
            padding: 10px;
            font-size: 13px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
        }

        QTextEdit:focus {
            border-color: rgba(100, 150, 250, 0.6);
            background: rgba(255, 255, 255, 1.0);
        }

        QScrollArea {
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 8px;
            background: rgba(250, 250, 252, 0.9);
        }

        QLabel {
            color: #4A5568;
            font-size: 13px;
            padding: 2px;
        }

        QProgressBar {
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 8px;
            text-align: center;
            background: rgba(240, 240, 245, 0.9);
            height: 20px;
        }

        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 rgba(100, 150, 250, 0.9),
                stop:1 rgba(120, 170, 255, 0.9));
            border-radius: 6px;
        }

        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 0.9),
                stop:1 rgba(240, 242, 245, 0.9));
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 8px;
            padding: 10px 16px;
            font-weight: 600;
            font-size: 13px;
            color: #4A5568;
            min-width: 100px;
        }

        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 1.0),
                stop:1 rgba(245, 247, 250, 1.0));
            border-color: rgba(100, 150, 250, 0.4);
        }

        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(235, 237, 240, 1.0),
                stop:1 rgba(225, 227, 230, 1.0));
        }

        QPushButton:disabled {
            background: rgba(240, 240, 245, 0.5);
            color: rgba(74, 85, 104, 0.5);
            border-color: rgba(200, 200, 210, 0.2);
        }

        QPushButton#actionBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(50, 200, 120, 0.9),
                stop:1 rgba(30, 180, 100, 0.9));
            color: white;
            border-color: rgba(30, 180, 100, 0.6);
        }

        QPushButton#actionBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(70, 220, 140, 0.95),
                stop:1 rgba(50, 200, 120, 0.95));
        }

        QPushButton#retryBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 165, 0, 0.9),
                stop:1 rgba(255, 140, 0, 0.9));
            color: white;
            border-color: rgba(255, 140, 0, 0.6);
        }

        QPushButton#retryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 185, 20, 0.95),
                stop:1 rgba(255, 165, 0, 0.95));
        }

        QPushButton#closeBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(220, 53, 69, 0.9),
                stop:1 rgba(200, 33, 49, 0.9));
            color: white;
            border-color: rgba(200, 33, 49, 0.6);
        }

        QPushButton#closeBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(240, 73, 89, 0.95),
                stop:1 rgba(220, 53, 69, 0.95));
        }
    )");
}

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
            statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
        } else {
            translationTextEdit->setPlainText("No translation available");
            translationTextEdit->setStyleSheet("color: #6c757d; font-style: italic;");
            statusLabel->setText("✅ OCR completed successfully!");
            statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
        }

        // Enable action buttons
        copyBtn->setEnabled(true);
        translateBtn->setEnabled(!result.text.isEmpty());
        saveBtn->setEnabled(!result.text.isEmpty());
    } else {
        resultTextEdit->setPlainText(QString("OCR Error: %1").arg(result.errorMessage));
        translationTextEdit->setPlainText("No translation available due to OCR failure");
        translationTextEdit->setStyleSheet("color: #dc3545; font-style: italic;");
        confidenceLabel->setText("Confidence: N/A");
        languageLabel->setText(QString("Language: %1").arg(result.language));

        // Disable action buttons
        copyBtn->setEnabled(false);
        translateBtn->setEnabled(false);
        saveBtn->setEnabled(false);

        statusLabel->setText("❌ OCR failed. Try adjusting settings or using a different engine.");
        statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
    }

    progressBar->setVisible(false);
}

void OCRResultWindow::showOCRProgress(const QString &status)
{
    statusLabel->setText(status);
    statusLabel->setStyleSheet("color: #007bff; font-weight: bold;");
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // Indeterminate progress
}

void OCRResultWindow::showOCRError(const QString &error)
{
    statusLabel->setText(QString("❌ Error: %1").arg(error));
    statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
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
        imageLabel->setStyleSheet("border: 2px dashed #ccc; padding: 20px;");
        return;
    }

    // Scale image to fit in display area while maintaining aspect ratio
    QSize displaySize = imageScrollArea->size() - QSize(20, 20); // Account for margins
    QPixmap scaledImage = image.scaled(displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    imageLabel->setPixmap(scaledImage);
    imageLabel->setStyleSheet("");
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
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");

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
    statusLabel->setStyleSheet("color: #007bff; font-weight: bold;");
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
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
}

void OCRResultWindow::onRetryClicked()
{
    emit retryRequested();
}

void OCRResultWindow::onCloseClicked()
{
    close();
}