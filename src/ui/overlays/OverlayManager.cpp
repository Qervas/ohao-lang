#include "OverlayManager.h"
#include "../screenshot/ScreenshotWidget.h"
#include "QuickTranslationOverlay.h"
#include "LanguageLearningOverlay.h"
#include "../tts/ModernTTSManager.h"
#include "../core/LanguageManager.h"
#include "../core/AppSettings.h"
#include <QDebug>

OverlayManager::OverlayManager(ScreenshotWidget* parent)
    : QObject(parent)
    , m_parent(parent)
    , m_quickOverlay(nullptr)
    , m_deepOverlay(nullptr)
    , m_currentMode(QuickTranslation)
    , m_ocrEngine(nullptr)
{
    qDebug() << "OverlayManager created";
    initializeOverlays();
    initializeOCR();

    // Load overlay mode from centralized settings
    QString modeString = AppSettings::instance().getTranslationConfig().overlayMode;
    if (modeString == "Deep Learning Mode") {
        m_currentMode = DeepLearning;
    } else {
        m_currentMode = QuickTranslation;
    }

    qDebug() << "OverlayManager initialized with mode:" << (m_currentMode == DeepLearning ? "Deep Learning" : "Quick Translation");
}

OverlayManager::~OverlayManager()
{
    qDebug() << "OverlayManager destroyed";
}

void OverlayManager::initializeOverlays()
{
    // Create both overlay types
    m_quickOverlay = new QuickTranslationOverlay(nullptr);
    m_quickOverlay->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_quickOverlay, &QuickTranslationOverlay::escapePressed,
            this, [this]() {
                hideAllOverlays();
                emit overlayEscapePressed();
            });

    m_deepOverlay = new LanguageLearningOverlay(nullptr);
    m_deepOverlay->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_deepOverlay, &LanguageLearningOverlay::escapePressed,
            this, [this]() {
                hideAllOverlays();
                emit overlayEscapePressed();
            });

    // Connect to theme changes for immediate theming updates
    connect(&AppSettings::instance(), &AppSettings::uiSettingsChanged,
            m_quickOverlay, &QuickTranslationOverlay::updateThemeColors);
    connect(&AppSettings::instance(), &AppSettings::uiSettingsChanged,
            m_deepOverlay, &LanguageLearningOverlay::applyTheme);

    qDebug() << "Both overlay types initialized with escape and theme signal connections";
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

void OverlayManager::performOCR(const QPixmap& image, const QRect& selectionRect, const QPixmap& fullScreenshot)
{
    qDebug() << "OverlayManager starting OCR for selection:" << selectionRect;

    // Store selection rect and source image for later use
    m_currentSelectionRect = selectionRect;
    m_currentSourceImage = fullScreenshot;

    // Configure OCR engine using centralized settings
    auto& settings = AppSettings::instance();
    auto ocrConfig = settings.getOCRConfig();
    auto translationConfig = settings.getTranslationConfig();

    // Set OCR engine type
    if (ocrConfig.engine == "Tesseract") {
        m_ocrEngine->setEngine(OCREngine::Tesseract);
    } else if (ocrConfig.engine == "EasyOCR") {
        m_ocrEngine->setEngine(OCREngine::EasyOCR);
    } else if (ocrConfig.engine == "PaddleOCR") {
        m_ocrEngine->setEngine(OCREngine::PaddleOCR);
    } else if (ocrConfig.engine == "Windows OCR") {
        m_ocrEngine->setEngine(OCREngine::WindowsOCR);
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
    qDebug() << "OverlayManager showing OCR results, mode:" << (m_currentMode == DeepLearning ? "Deep Learning" : "Quick Translation");

    m_lastResult = result;

    if (!result.success || result.text.isEmpty()) {
        showError("OCR failed to extract text");
        return;
    }

    // Hide all overlays first
    hideAllOverlays();

    // Position calculation - selectionRect is already in screen coordinates since parent is fullscreen
    QRect globalSelRect = selectionRect;

    if (m_currentMode == DeepLearning) {
        // Use deep learning overlay
        m_deepOverlay->showLearningContent(result);
        m_deepOverlay->positionNearSelection(globalSelRect);
        qDebug() << "Deep learning overlay positioned and shown";
    } else {
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

        // Position the overlay elegantly near the selection
        QSize screenSize = m_parent->size(); // Screenshot widget covers the full screen
        m_quickOverlay->setPositionNearRect(globalSelRect, screenSize);

        // Show the overlay
        m_quickOverlay->show();
        m_quickOverlay->raise();
        m_quickOverlay->activateWindow();
        qDebug() << "Elegant translation overlay positioned and shown near" << globalSelRect;
    }

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
    hideAllOverlays();
    // Error display could be enhanced with a specific error overlay
}

void OverlayManager::hideAllOverlays()
{
    if (m_quickOverlay) {
        m_quickOverlay->hide();
    }
    if (m_deepOverlay) {
        m_deepOverlay->hide();
    }
    qDebug() << "All overlays hidden";
}

void OverlayManager::setOverlayMode(OverlayMode mode)
{
    if (m_currentMode != mode) {
        hideAllOverlays();
        m_currentMode = mode;
        qDebug() << "Overlay mode changed to:" << (mode == DeepLearning ? "Deep Learning" : "Quick Translation");
    }
}

OverlayManager::OverlayMode OverlayManager::getOverlayMode() const
{
    return m_currentMode;
}

bool OverlayManager::areOverlaysVisible() const
{
    return (m_quickOverlay && m_quickOverlay->isVisible()) ||
           (m_deepOverlay && m_deepOverlay->isVisible());
}

void OverlayManager::callTTSForResult(const OCRResult& result)
{
    qDebug() << "OverlayManager calling TTS for OCR result";

    // Get the language from OCR result or use default
    QString languageCode = result.language;
    if (languageCode.isEmpty()) {
        // Fallback to centralized settings
        languageCode = AppSettings::instance().getTranslationConfig().sourceLanguage;
    }

    // Ensure we have a language code, not a display name
    QString originalLanguageCode = languageCode;
    languageCode = LanguageManager::instance().languageCodeFromDisplayName(languageCode);

    if (languageCode != originalLanguageCode) {
        qDebug() << "Converted display name to language code:" << originalLanguageCode << "->" << languageCode;
    }

    // Use ModernTTSManager to speak the text - much simpler API!
    ModernTTSManager::instance().speak(result.text, languageCode);

    qDebug() << "ModernTTS called for text:" << result.text.left(50) << "with language:" << languageCode;
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
        // Show error
        showError(result.errorMessage);
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
    if (m_currentMode != QuickTranslation) {
        return; // Only show immediate preview for quick translation mode
    }

    // Show a preview overlay with processing message
    m_quickOverlay->setContent("🔍 Processing...", "⏳ Translating...");
    m_quickOverlay->setMode(QuickTranslationOverlay::ShowOriginal);

    // Position the overlay
    QSize screenSize = m_parent->size();
    m_quickOverlay->setPositionNearRect(selectionRect, screenSize);

    // Show the preview
    m_quickOverlay->show();
    m_quickOverlay->raise();

    qDebug() << "Immediate preview overlay shown at" << selectionRect;
}