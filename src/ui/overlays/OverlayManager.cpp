#include "OverlayManager.h"
#include "../screenshot/ScreenshotWidget.h"
#include "QuickTranslationOverlay.h"
#include "../tts/ModernTTSManager.h"
#include "../tts/TTSManager.h"
#include "../core/LanguageManager.h"
#include "../core/AppSettings.h"
#include <QDebug>

OverlayManager::OverlayManager(ScreenshotWidget* parent)
    : QObject(parent)
    , m_parent(parent)
    , m_quickOverlay(nullptr)
    , m_ocrEngine(nullptr)
{
    qDebug() << "OverlayManager created";
    initializeOverlays();
    initializeOCR();
    qDebug() << "OverlayManager initialized";
}

OverlayManager::~OverlayManager()
{
    qDebug() << "OverlayManager destroyed";
}

void OverlayManager::initializeOverlays()
{
    // Create quick translation overlay
    m_quickOverlay = new QuickTranslationOverlay(nullptr);
    m_quickOverlay->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_quickOverlay, &QuickTranslationOverlay::escapePressed,
            this, [this]() {
                hideAllOverlays();
                emit overlayEscapePressed();
            });

    // Connect to theme changes for immediate theming updates
    connect(&AppSettings::instance(), &AppSettings::uiSettingsChanged,
            m_quickOverlay, &QuickTranslationOverlay::updateThemeColors);

    qDebug() << "Quick translation overlay initialized with escape and theme signal connections";
}

void OverlayManager::initializeOCR()
{
    m_ocrEngine = new OCREngine(this);

    // Connect OCR signals
    connect(m_ocrEngine, &OCREngine::ocrFinished, this, &OverlayManager::onOCRFinished);
    connect(m_ocrEngine, &OCREngine::ocrProgress, this, &OverlayManager::onOCRProgress);
    connect(m_ocrEngine, &OCREngine::ocrError, this, &OverlayManager::onOCRError);

    qDebug() << "OCR engine initialized";
}

void OverlayManager::performOCR(const QPixmap& image, const QRect& selectionRect, const QPixmap& fullScreenshot, const QList<QRect>& existingSelections)
{
    qDebug() << "OverlayManager starting OCR for selection:" << selectionRect;

    // Store selection rect and source image for later use
    m_currentSelectionRect = selectionRect;
    m_currentSourceImage = fullScreenshot;
    m_existingSelections = existingSelections;

    // Configure OCR engine using centralized settings
    auto& settings = AppSettings::instance();
    auto ocrConfig = settings.getOCRConfig();
    auto translationConfig = settings.getTranslationConfig();

    // Set OCR engine type
    if (ocrConfig.engine == "AppleVision") {
        m_ocrEngine->setEngine(OCREngine::AppleVision);
    } else {
        // Default to Tesseract for all other cases (including legacy "WindowsOCR", "EasyOCR", "PaddleOCR")
        m_ocrEngine->setEngine(OCREngine::Tesseract);
    }

    // Configure OCR settings
    m_ocrEngine->setLanguage(ocrConfig.language);
    m_ocrEngine->setQualityLevel(ocrConfig.qualityLevel);
    m_ocrEngine->setPreprocessing(ocrConfig.preprocessing);
    m_ocrEngine->setAutoDetectOrientation(ocrConfig.autoDetectOrientation);

    // Configure translation settings
    m_ocrEngine->setAutoTranslate(translationConfig.autoTranslate);
    m_ocrEngine->setTranslationEngine(translationConfig.engine);
    m_ocrEngine->setTranslationSourceLanguage(translationConfig.sourceLanguage);
    m_ocrEngine->setTranslationTargetLanguage(translationConfig.targetLanguage);

    qDebug() << "OCR configured with autoTranslate:" << translationConfig.autoTranslate
             << "engine:" << translationConfig.engine
             << "source:" << translationConfig.sourceLanguage
             << "target:" << translationConfig.targetLanguage;

    // Show progress
    showProgress("🔍 Starting OCR processing...");

    // Show immediate preview overlay while OCR processes
    showImmediatePreview(selectionRect);

    // Start OCR processing
    m_ocrEngine->performOCR(image);
}

void OverlayManager::showOCRResults(const OCRResult& result, const QRect& selectionRect, const QPixmap& sourceImage)
{
    qDebug() << "OverlayManager showing OCR results";

    m_lastResult = result;

    if (!result.success || result.text.isEmpty()) {
        showError("OCR failed to extract text");
        return;
    }

    // Hide all overlays first
    hideAllOverlays();

    // Position calculation - selectionRect is already in screen coordinates since parent is fullscreen
    QRect globalSelRect = selectionRect;

    // Use quick translation overlay with new elegant design
    qDebug() << "Setting up quick overlay - text:" << result.text.left(50) << "translation:" << result.translatedText.left(50);
    qDebug() << "Translation empty?" << result.translatedText.isEmpty() << "Same as original?" << (result.translatedText == result.text);

    // Set content and mode - always show both if we have translation text
    m_quickOverlay->setContent(result.text, result.translatedText);

    if (!result.translatedText.isEmpty() && result.translatedText != result.text) {
        // We have a translation - show it
        qDebug() << "Showing both original and translation";
        m_quickOverlay->setMode(QuickTranslationOverlay::ShowBoth);
    } else {
        // No translation - just show original
        qDebug() << "Showing only original text - no translation available";
        m_quickOverlay->setMode(QuickTranslationOverlay::ShowOriginal);
    }

    // Position the overlay elegantly near the selection, avoiding existing OCR areas
    QSize screenSize = m_parent->size(); // Screenshot widget covers the full screen
    m_quickOverlay->setPositionNearRect(globalSelRect, screenSize, m_existingSelections);

    // Show the overlay
    m_quickOverlay->show();
    m_quickOverlay->raise();
    m_quickOverlay->activateWindow();
    qDebug() << "Elegant translation overlay positioned and shown near" << globalSelRect;

    // Call TTS for the OCR result
    callTTSForResult(result);
}

void OverlayManager::showProgress(const QString& message)
{
    qDebug() << "OverlayManager showing progress:" << message;
    // For now, progress is shown by ScreenshotWidget's paintEvent
    // Could be refactored to show in overlays if needed
}

void OverlayManager::showError(const QString& error)
{
    qDebug() << "OverlayManager showing error:" << error;
    
    // Show error message in the overlay
    if (m_quickOverlay && !error.isEmpty()) {
        m_quickOverlay->setContent("⚠️ No Text Found", error);
        m_quickOverlay->setMode(QuickTranslationOverlay::ShowBoth);
        
        // Position near the selection if we have it, otherwise center
        QSize screenSize = m_parent->size();
        if (!m_currentSelectionRect.isEmpty()) {
            // Position near selection like normal results
            m_quickOverlay->setPositionNearRect(m_currentSelectionRect, screenSize, m_existingSelections);
        } else {
            // Fallback to center
            int x = (screenSize.width() - 300) / 2;
            int y = (screenSize.height() - 150) / 2;
            m_quickOverlay->setGeometry(x, y, 300, 150);
        }
        
        m_quickOverlay->show();
        m_quickOverlay->raise();
        m_quickOverlay->activateWindow();
        
        qDebug() << "Error overlay shown near selection";
    } else {
        hideAllOverlays();
    }
}

void OverlayManager::hideAllOverlays()
{
    if (m_quickOverlay) {
        m_quickOverlay->hide();
    }
    qDebug() << "All overlays hidden";
}

bool OverlayManager::areOverlaysVisible() const
{
    return (m_quickOverlay && m_quickOverlay->isVisible());
}

void OverlayManager::callTTSForResult(const OCRResult& result)
{
    QString textToSpeak;
    QString languageCode;

    // Get user preference from settings (reload to ensure fresh)
    AppSettings::instance().reload();
    bool speakTranslation = AppSettings::instance().getTTSConfig().speakTranslation;

    // If user wants translation AND we have a translation, speak it
    if (speakTranslation && result.hasTranslation && !result.translatedText.isEmpty() && result.translatedText != result.text) {
        textToSpeak = result.translatedText;
        languageCode = result.targetLanguage;
    } else {
        // Otherwise speak original text in source language
        textToSpeak = result.text;
        languageCode = result.language;
        if (languageCode.isEmpty()) {
            languageCode = AppSettings::instance().getTranslationConfig().sourceLanguage;
        }
    }

    // Ensure we have a language code, not a display name
    languageCode = LanguageManager::instance().languageCodeFromDisplayName(languageCode);

    // Convert language code to QLocale
    QLocale targetLocale = LanguageManager::instance().localeFromLanguageCode(languageCode);

    // Check if text is empty
    if (textToSpeak.isEmpty()) {
        return;
    }

    // Apply word-by-word reading if enabled
    bool wordByWord = AppSettings::instance().getTTSConfig().wordByWordReading;
    if (wordByWord) {
        // Split text into words and add commas for pauses
        QStringList words = textToSpeak.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        textToSpeak = words.join(", ");  // Comma creates a natural pause in TTS
    }

    // Use ModernTTSManager (same as test button in settings)
    ModernTTSManager::instance().speak(textToSpeak, targetLocale);
}

void OverlayManager::onTTSFinished()
{
    qDebug() << "TTS finished for overlay content";
    // Could trigger additional UI updates if needed
}

// Removed getOverlayModeFromSettings() - now using AppSettings::instance()

void OverlayManager::onOCRFinished(const OCRResult& result)
{
    qDebug() << "OverlayManager OCR finished. Success:" << result.success << "Text:" << result.text;

    if (result.success && !result.text.isEmpty()) {
        // Show results with current selection rect and stored source image
        showOCRResults(result, m_currentSelectionRect, m_currentSourceImage);
    } else {
        // Show error with appropriate message
        QString errorMsg = result.errorMessage;
        if (errorMsg.isEmpty()) {
            if (result.success) {
                errorMsg = "No text found in the selected area.\nTry selecting a larger area with visible text.";
            } else {
                errorMsg = "OCR processing failed.\nPlease try again.";
            }
        }
        showError(errorMsg);
    }
}

void OverlayManager::onOCRProgress(const QString& status)
{
    qDebug() << "OverlayManager OCR progress:" << status;
    showProgress(status);
}

void OverlayManager::onOCRError(const QString& error)
{
    qDebug() << "OverlayManager OCR error:" << error;
    showError(error);
}

void OverlayManager::showImmediatePreview(const QRect& selectionRect)
{
    // Show a preview overlay with processing message
    m_quickOverlay->setContent("🔍 Processing...", "⏳ Translating...");
    m_quickOverlay->setMode(QuickTranslationOverlay::ShowOriginal);

    // Position the overlay (use existing selections to avoid overlaps)
    QSize screenSize = m_parent->size();
    m_quickOverlay->setPositionNearRect(selectionRect, screenSize, m_existingSelections);

    // Show the preview
    m_quickOverlay->show();
    m_quickOverlay->raise();

    qDebug() << "Immediate preview overlay shown at" << selectionRect;
}