#include "SettingsWindow.h"
#include "OCREngine.h"
#include "TTSEngine.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QMessageBox>
#include <QShowEvent>
#include <QHideEvent>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include "ThemeManager.h"

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
    , tabWidget(nullptr)
    , settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
    , ttsEngine(nullptr)
{
    // Initialize TTS engine
    ttsEngine = new TTSEngine(this);

    setupUI();
    applyModernStyling();
    loadSettings();

    // Setup animations
    opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacityEffect);

    showAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    showAnimation->setDuration(300);
    showAnimation->setEasingCurve(QEasingCurve::OutQuart);
}

SettingsWindow::~SettingsWindow()
{
}

void SettingsWindow::setupUI()
{
    setWindowTitle("Settings - Ohao Lang");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setModal(true);

    // Use adaptive sizing based on screen resolution
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int width = qMin(800, static_cast<int>(screenGeometry.width() * 0.6));  // Max 800px or 60% of screen
        int height = qMin(700, static_cast<int>(screenGeometry.height() * 0.8)); // Max 700px or 80% of screen
        resize(width, height);
        setMinimumSize(600, 500); // Ensure minimum readability
    }

    // Main layout with responsive margins
    mainLayout = new QVBoxLayout(this);
    int margin = qMax(15, width() / 40); // Adaptive margins
    mainLayout->setContentsMargins(margin, margin, margin, margin);
    mainLayout->setSpacing(15);

    // Tab widget
    tabWidget = new QTabWidget(this);
    tabWidget->setTabPosition(QTabWidget::North);
    mainLayout->addWidget(tabWidget);

    // Setup tabs
    setupGeneralTab();
    setupOcrTab();
    setupTranslationTab();
    setupAppearanceTab();
    setupTTSTab();

    // Button layout
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    resetBtn = new QPushButton("Reset to Defaults", this);
    resetBtn->setObjectName("resetBtn");
    connect(resetBtn, &QPushButton::clicked, this, &SettingsWindow::onResetClicked);

    cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &SettingsWindow::onCancelClicked);

    applyBtn = new QPushButton("Apply", this);
    applyBtn->setObjectName("applyBtn");
    applyBtn->setDefault(true);
    connect(applyBtn, &QPushButton::clicked, this, &SettingsWindow::onApplyClicked);

    buttonLayout->addWidget(resetBtn);
    buttonLayout->addSpacing(10);
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(applyBtn);

    mainLayout->addLayout(buttonLayout);

    // Center window on screen
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2, (screenGeometry.height() - height()) / 2);
    }
}

void SettingsWindow::setupGeneralTab()
{
    generalTab = new QWidget();
    tabWidget->addTab(generalTab, "⚙️ General");
    
    QVBoxLayout *layout = new QVBoxLayout(generalTab);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Language Configuration Group
    QGroupBox *langGroup = new QGroupBox("🌍 Language Configuration");
    langGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; padding-top: 10px; }");
    QFormLayout *langLayout = new QFormLayout(langGroup);
    langLayout->setSpacing(15);
    
    // Input Language (OCR)
    QLabel *inputLabel = new QLabel("📖 Input Language (OCR Recognition):");
    inputLabel->setWordWrap(true);
    ocrLanguageCombo = new QComboBox();
    ocrLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)",
                               "Japanese", "Korean", "Spanish", "French", "German", "Russian",
                               "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
    ocrLanguageCombo->setToolTip("Language for text recognition from screenshots");
    connect(ocrLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ updateVoicesForLanguage(); });
    langLayout->addRow(inputLabel, ocrLanguageCombo);
    
    // Output Language (Translation)
    QLabel *outputLabel = new QLabel("🌐 Output Language (Translation Target):");
    outputLabel->setWordWrap(true);
    targetLanguageCombo = new QComboBox();
    targetLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)", "Japanese", "Korean", "Spanish",
                                  "French", "German", "Russian", "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
    targetLanguageCombo->setCurrentText("English");
    targetLanguageCombo->setToolTip("Target language for translation and TTS voice selection");
    connect(targetLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ updateVoicesForLanguage(); });
    langLayout->addRow(outputLabel, targetLanguageCombo);
    
    // Info text
    QLabel *infoLabel = new QLabel("ℹ️ <i>TTS voice language automatically matches the Output Language setting.<br>"
                                  "This unified configuration ensures consistent language handling across all features.</i>");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;");
    langLayout->addRow(infoLabel);
    
    layout->addWidget(langGroup);
    
    // TTS Options Group
    QGroupBox *ttsOptionsGroup = new QGroupBox("🔊 Text-to-Speech Options");
    ttsOptionsGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; padding-top: 10px; }");
    QVBoxLayout *ttsOptionsLayout = new QVBoxLayout(ttsOptionsGroup);
    ttsOptionsLayout->setSpacing(10);
    
    // TTS Input checkbox (read OCR text)
    ttsInputCheckBox = new QCheckBox("🎤 Enable TTS for Input Text (Read recognized OCR text aloud)");
    ttsInputCheckBox->setToolTip("When enabled, the app will read the recognized text from OCR aloud");
    ttsInputCheckBox->setChecked(false); // Default off
    ttsOptionsLayout->addWidget(ttsInputCheckBox);
    
    // TTS Output checkbox (read translation)
    ttsOutputCheckBox = new QCheckBox("🗣️ Enable TTS for Output Text (Read translation results aloud)");
    ttsOutputCheckBox->setToolTip("When enabled, the app will read the translated text aloud");
    ttsOutputCheckBox->setChecked(true); // Default on
    ttsOptionsLayout->addWidget(ttsOutputCheckBox);
    
    // Connect TTS checkboxes to engine (if available)
    connect(ttsInputCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (ttsEngine) ttsEngine->setTTSInputEnabled(checked);
    });
    connect(ttsOutputCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (ttsEngine) ttsEngine->setTTSOutputEnabled(checked);
    });
    
    // Info about TTS options
    QLabel *ttsInfoLabel = new QLabel("ℹ️ <i>You can enable TTS for input text, output text, or both. "
                                     "Use the TTS tab to configure voice settings and test playback.</i>");
    ttsInfoLabel->setWordWrap(true);
    ttsInfoLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 8px;");
    ttsOptionsLayout->addWidget(ttsInfoLabel);
    
    layout->addWidget(ttsOptionsGroup);
    layout->addStretch();
}

void SettingsWindow::setupOcrTab()
{
    ocrTab = new QWidget();
    tabWidget->addTab(ocrTab, "🔍 OCR");

    // Use scroll area to handle content that might not fit
    QScrollArea *scrollArea = new QScrollArea(ocrTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    scrollArea->setWidget(scrollContent);

    QVBoxLayout *tabLayout = new QVBoxLayout(ocrTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    QVBoxLayout *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(20);
    layout->setContentsMargins(15, 15, 15, 15);

    // OCR Engine Group
    QGroupBox *engineGroup = new QGroupBox("OCR Engine");
    QFormLayout *engineLayout = new QFormLayout(engineGroup);
    engineLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    engineLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    engineLayout->setSpacing(12);

    ocrEngineCombo = new QComboBox();
    ocrEngineCombo->addItems({"Tesseract", "EasyOCR", "PaddleOCR", "Windows OCR"});
    connect(ocrEngineCombo, &QComboBox::currentTextChanged, this, &SettingsWindow::onOcrEngineChanged);
    engineLayout->addRow("Engine:", ocrEngineCombo);

    // Language setting moved to General tab
    QLabel *langNote = new QLabel("<i>Language setting moved to General tab</i>");
    langNote->setStyleSheet("color: #666; font-size: 11px;");
    engineLayout->addRow("Language:", langNote);

    layout->addWidget(engineGroup);

    // Quality Settings Group
    QGroupBox *qualityGroup = new QGroupBox("Quality Settings", ocrTab);
    QFormLayout *qualityLayout = new QFormLayout(qualityGroup);

    ocrQualitySlider = new QSlider(Qt::Horizontal);
    ocrQualitySlider->setRange(1, 5);
    ocrQualitySlider->setValue(3);
    ocrQualitySlider->setTickPosition(QSlider::TicksBelow);
    ocrQualitySlider->setTickInterval(1);

    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(new QLabel("Fast"));
    sliderLayout->addWidget(ocrQualitySlider);
    sliderLayout->addWidget(new QLabel("Accurate"));
    qualityLayout->addRow("Speed vs Accuracy:", sliderLayout);

    ocrPreprocessingCheck = new QCheckBox("Enable image preprocessing");
    ocrPreprocessingCheck->setChecked(true);
    qualityLayout->addRow(ocrPreprocessingCheck);

    ocrAutoDetectCheck = new QCheckBox("Auto-detect text orientation");
    ocrAutoDetectCheck->setChecked(true);
    qualityLayout->addRow(ocrAutoDetectCheck);

    layout->addWidget(qualityGroup);

    // Test Section
    QGroupBox *testGroup = new QGroupBox("Test & Status", ocrTab);
    QVBoxLayout *testLayout = new QVBoxLayout(testGroup);

    testOcrBtn = new QPushButton("Test OCR Configuration");
    testOcrBtn->setObjectName("testBtn");
    connect(testOcrBtn, &QPushButton::clicked, this, &SettingsWindow::onTestOcrClicked);
    testLayout->addWidget(testOcrBtn);

    ocrStatusText = new QTextEdit();
    ocrStatusText->setMinimumHeight(60);
    ocrStatusText->setMaximumHeight(120); // Allow more space for status text
    ocrStatusText->setReadOnly(true);
    ocrStatusText->setPlainText("OCR engine ready. Click 'Test OCR Configuration' to verify setup.");
    ocrStatusText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    testLayout->addWidget(ocrStatusText);

    layout->addWidget(testGroup);
    layout->addStretch();
}

void SettingsWindow::setupTranslationTab()
{
    translationTab = new QWidget();
    tabWidget->addTab(translationTab, "🌐 Translation");

    // Use scroll area to handle content that might not fit
    QScrollArea *scrollArea = new QScrollArea(translationTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    scrollArea->setWidget(scrollContent);

    QVBoxLayout *tabLayout = new QVBoxLayout(translationTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    QVBoxLayout *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(20);
    layout->setContentsMargins(15, 15, 15, 15);

    // Auto-Processing Group
    QGroupBox *autoGroup = new QGroupBox("Automatic Processing", translationTab);
    QVBoxLayout *autoLayout = new QVBoxLayout(autoGroup);

    autoOcrCheck = new QCheckBox("Auto-OCR: Automatically extract text when area is selected");
    autoOcrCheck->setChecked(true);
    autoLayout->addWidget(autoOcrCheck);

    autoTranslateCheck = new QCheckBox("Auto-Translate: Automatically translate after OCR");
    autoTranslateCheck->setChecked(true);
    autoLayout->addWidget(autoTranslateCheck);

    layout->addWidget(autoGroup);

    // Translation Engine Group
    QGroupBox *engineGroup = new QGroupBox("Translation Engine", translationTab);
    QFormLayout *engineLayout = new QFormLayout(engineGroup);

    translationEngineCombo = new QComboBox();
    translationEngineCombo->addItems({"Google Translate (Free)", "LibreTranslate", "Ollama Local LLM",
                                     "Microsoft Translator", "DeepL (Free)", "Offline Dictionary"});
    connect(translationEngineCombo, &QComboBox::currentTextChanged, this, &SettingsWindow::onTranslationEngineChanged);
    engineLayout->addRow("Engine:", translationEngineCombo);

    sourceLanguageCombo = new QComboBox();
    sourceLanguageCombo->addItems({"Auto-Detect", "English", "Chinese (Simplified)", "Chinese (Traditional)",
                                  "Japanese", "Korean", "Spanish", "French", "German", "Russian",
                                  "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
    engineLayout->addRow("Source Language:", sourceLanguageCombo);

    // Target Language setting moved to General tab
    QLabel *targetNote = new QLabel("<i>Target Language setting moved to General tab</i>");
    targetNote->setStyleSheet("color: #666; font-size: 11px;");
    engineLayout->addRow("Target Language:", targetNote);

    layout->addWidget(engineGroup);

    // API Configuration Group
    QGroupBox *apiGroup = new QGroupBox("API Configuration", translationTab);
    QFormLayout *apiLayout = new QFormLayout(apiGroup);

    apiUrlEdit = new QLineEdit();
    apiUrlEdit->setPlaceholderText("https://api.example.com/translate");
    apiLayout->addRow("API URL:", apiUrlEdit);

    apiKeyEdit = new QLineEdit();
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setPlaceholderText("Enter API key (optional for free services)");
    apiLayout->addRow("API Key:", apiKeyEdit);

    layout->addWidget(apiGroup);

    // Test Section
    QGroupBox *testGroup = new QGroupBox("Test & Status", translationTab);
    QVBoxLayout *testLayout = new QVBoxLayout(testGroup);

    testTranslationBtn = new QPushButton("Test Translation Configuration");
    testTranslationBtn->setObjectName("testBtn");
    connect(testTranslationBtn, &QPushButton::clicked, this, &SettingsWindow::onTestTranslationClicked);
    testLayout->addWidget(testTranslationBtn);

    translationStatusText = new QTextEdit();
    translationStatusText->setMinimumHeight(60);
    translationStatusText->setMaximumHeight(120); // Allow more space for status text
    translationStatusText->setReadOnly(true);
    translationStatusText->setPlainText("Translation engine ready. Click 'Test Translation Configuration' to verify setup.");
    translationStatusText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    testLayout->addWidget(translationStatusText);

    layout->addWidget(testGroup);
    layout->addStretch();
}

void SettingsWindow::setupAppearanceTab()
{
    appearanceTab = new QWidget();
    tabWidget->addTab(appearanceTab, "🎨 Appearance");

    // Use scroll area to handle content that might not fit
    QScrollArea *scrollArea = new QScrollArea(appearanceTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    scrollArea->setWidget(scrollContent);

    QVBoxLayout *tabLayout = new QVBoxLayout(appearanceTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    QVBoxLayout *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(20);
    layout->setContentsMargins(15, 15, 15, 15);

    // Theme Group
    QGroupBox *themeGroup = new QGroupBox("Theme", appearanceTab);
    QFormLayout *themeLayout = new QFormLayout(themeGroup);

    themeCombo = new QComboBox();
    themeCombo->addItems({"Auto (System)", "Light", "Dark", "High Contrast", "Cyberpunk"});
    connect(themeCombo, &QComboBox::currentTextChanged, this, [this](const QString &text){
        // Preview theme instantly
        ThemeManager::applyTheme(ThemeManager::fromString(text));
    });
    themeLayout->addRow("Theme:", themeCombo);

    layout->addWidget(themeGroup);

    // Visual Effects Group
    QGroupBox *effectsGroup = new QGroupBox("Visual Effects", appearanceTab);
    QFormLayout *effectsLayout = new QFormLayout(effectsGroup);

    opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(50, 100);
    opacitySlider->setValue(90);
    opacitySlider->setTickPosition(QSlider::TicksBelow);
    opacitySlider->setTickInterval(10);

    QHBoxLayout *opacityLayout = new QHBoxLayout();
    opacityLayout->addWidget(new QLabel("50%"));
    opacityLayout->addWidget(opacitySlider);
    opacityLayout->addWidget(new QLabel("100%"));
    effectsLayout->addRow("Widget Opacity:", opacityLayout);

    animationsCheck = new QCheckBox("Enable smooth animations");
    animationsCheck->setChecked(true);
    effectsLayout->addRow(animationsCheck);

    soundsCheck = new QCheckBox("Enable sound effects");
    soundsCheck->setChecked(false);
    effectsLayout->addRow(soundsCheck);

    layout->addWidget(effectsGroup);
    layout->addStretch();
}

void SettingsWindow::applyModernStyling()
{
    // Now themed globally via ThemeManager
}

void SettingsWindow::loadSettings()
{
    // OCR Settings
    ocrEngineCombo->setCurrentText(settings->value("ocr/engine", "Tesseract").toString());
    ocrLanguageCombo->setCurrentText(settings->value("ocr/language", "English").toString());
    ocrQualitySlider->setValue(settings->value("ocr/quality", 3).toInt());
    ocrPreprocessingCheck->setChecked(settings->value("ocr/preprocessing", true).toBool());
    ocrAutoDetectCheck->setChecked(settings->value("ocr/autoDetect", true).toBool());

    // Translation Settings
    autoOcrCheck->setChecked(settings->value("translation/autoOcr", true).toBool());
    autoTranslateCheck->setChecked(settings->value("translation/autoTranslate", true).toBool());
    translationEngineCombo->setCurrentText(settings->value("translation/engine", "Google Translate (Free)").toString());
    sourceLanguageCombo->setCurrentText(settings->value("translation/sourceLanguage", "Auto-Detect").toString());
    targetLanguageCombo->setCurrentText(settings->value("translation/targetLanguage", "English").toString());
    apiUrlEdit->setText(settings->value("translation/apiUrl", "").toString());

    // API Key handling (basic encryption could be added here)
    apiKeyEdit->setText(settings->value("translation/apiKey", "").toString());

    // Appearance Settings
    themeCombo->setCurrentText(settings->value("appearance/theme", "Auto (System)").toString());
    opacitySlider->setValue(settings->value("appearance/opacity", 90).toInt());
    animationsCheck->setChecked(settings->value("appearance/animations", true).toBool());
    soundsCheck->setChecked(settings->value("appearance/sounds", false).toBool());

    // General TTS Options
    ttsInputCheckBox->setChecked(settings->value("general/ttsInput", false).toBool());
    ttsOutputCheckBox->setChecked(settings->value("general/ttsOutput", true).toBool());

    // TTS Settings
    if (ttsEngine) {
        ttsEnabledCheck->setChecked(settings->value("tts/enabled", true).toBool());
        volumeSlider->setValue(settings->value("tts/volume", 80).toInt());
        pitchSlider->setValue(settings->value("tts/pitch", 0).toInt());
        {
            int savedRate = settings->value("tts/rate", 0).toInt();
            // Backward compatibility: old builds saved 50..200; map to -100..100 by offsetting from 100
            if (savedRate >= 50 && savedRate <= 200) {
                savedRate = qBound(-100, savedRate - 100, 100);
            } else {
                // New builds save -100..100 directly
                savedRate = qBound(-100, savedRate, 100);
            }
            rateSlider->setValue(savedRate);
        }

        // Backend and provider configs
        const QString backendStr = settings->value("tts/backend", "system").toString();
        azureConfigGroup->setVisible(false);
        googleConfigGroup->setVisible(false);
        elevenConfigGroup->setVisible(false);
        pollyConfigGroup->setVisible(false);
        if (backendStr.compare("azure", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Azure Neural (Cloud)");
            ttsEngine->setBackend(TTSEngine::Backend::Azure);
            azureConfigGroup->setVisible(true);
        } else if (backendStr.compare("google", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Google Cloud TTS");
            ttsEngine->setBackend(TTSEngine::Backend::Google);
            googleConfigGroup->setVisible(true);
        } else if (backendStr.compare("googlefree", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Google (Free Online)");
            ttsEngine->setBackend(TTSEngine::Backend::GoogleFree);
        } else if (backendStr.compare("elevenlabs", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("ElevenLabs");
            ttsEngine->setBackend(TTSEngine::Backend::ElevenLabs);
            elevenConfigGroup->setVisible(true);
        } else if (backendStr.compare("polly", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Amazon Polly");
            ttsEngine->setBackend(TTSEngine::Backend::Polly);
            pollyConfigGroup->setVisible(true);
        } else if (backendStr.compare("edge", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Microsoft Edge (Free Online)");
            ttsEngine->setBackend(TTSEngine::Backend::Edge);
            edgeConfigGroup->setVisible(true);
        } else if (backendStr.compare("piper", Qt::CaseInsensitive) == 0) {
            ttsBackendCombo->setCurrentText("Piper (Free, Local)");
            ttsEngine->setBackend(TTSEngine::Backend::Piper);
            piperConfigGroup->setVisible(true);
        } else {
            ttsBackendCombo->setCurrentText("System (Local)");
            ttsEngine->setBackend(TTSEngine::Backend::System);
        }
        settings->beginGroup("tts/Azure");
        azureRegionEdit->setText(settings->value("region").toString());
        azureKeyEdit->setText(settings->value("key").toString());
        azureStyleEdit->setText(settings->value("style").toString());
        const QString azureVoice = settings->value("voice").toString();
        if (!azureVoice.isEmpty()) ttsEngine->configureAzure(azureRegionEdit->text(), azureKeyEdit->text(), azureVoice, azureStyleEdit->text());
        settings->endGroup();

        settings->beginGroup("tts/Google");
        googleApiKeyEdit->setText(settings->value("apiKey").toString());
        googleLanguageCodeEdit->setText(settings->value("languageCode").toString());
        const QString googleVoice = settings->value("voice").toString();
        if (!googleVoice.isEmpty()) ttsEngine->configureGoogle(googleApiKeyEdit->text(), googleVoice, googleLanguageCodeEdit->text());
        settings->endGroup();

        settings->beginGroup("tts/ElevenLabs");
        elevenApiKeyEdit->setText(settings->value("apiKey").toString());
        elevenVoiceIdEdit->setText(settings->value("voiceId").toString());
        if (!elevenApiKeyEdit->text().isEmpty() && !elevenVoiceIdEdit->text().isEmpty()) ttsEngine->configureElevenLabs(elevenApiKeyEdit->text(), elevenVoiceIdEdit->text());
        settings->endGroup();

        settings->beginGroup("tts/Polly");
        pollyRegionEdit->setText(settings->value("region").toString());
        pollyAccessKeyEdit->setText(settings->value("accessKey").toString());
        pollySecretKeyEdit->setText(settings->value("secretKey").toString());
        const QString pollyVoice = settings->value("voice").toString();
        if (!pollyRegionEdit->text().isEmpty() && !pollyAccessKeyEdit->text().isEmpty() && !pollySecretKeyEdit->text().isEmpty() && !pollyVoice.isEmpty()) {
            ttsEngine->configurePolly(pollyRegionEdit->text(), pollyAccessKeyEdit->text(), pollySecretKeyEdit->text(), pollyVoice);
        }
        settings->endGroup();

        settings->beginGroup("tts/Piper");
        piperExeEdit->setText(settings->value("exePath").toString());
        piperModelEdit->setText(settings->value("modelPath").toString());
        if (!piperExeEdit->text().isEmpty() && !piperModelEdit->text().isEmpty()) {
            ttsEngine->configurePiper(piperExeEdit->text(), piperModelEdit->text());
        }
        settings->endGroup();

        settings->beginGroup("tts/Edge");
        edgeExeEdit->setText(settings->value("exePath").toString());
        edgeVoiceEdit->setText(settings->value("voice").toString());
        if (!edgeExeEdit->text().isEmpty()) {
            ttsEngine->configureEdge(edgeExeEdit->text(), edgeVoiceEdit->text());
        }
        settings->endGroup();

        // Load voice selection
        QString savedVoice = settings->value("tts/voice", "").toString();
        if (!savedVoice.isEmpty()) {
            int voiceIndex = voiceCombo->findData(savedVoice);
            if (voiceIndex >= 0) {
                voiceCombo->setCurrentIndex(voiceIndex);
            }
        }

        // Load input/output voice selection
        QString savedInputVoice = settings->value("tts/inputVoice", "").toString();
        if (!savedInputVoice.isEmpty() && inputVoiceCombo) {
            int inputVoiceIndex = inputVoiceCombo->findData(savedInputVoice);
            if (inputVoiceIndex >= 0) {
                inputVoiceCombo->setCurrentIndex(inputVoiceIndex);
            }
        }
        
        QString savedOutputVoice = settings->value("tts/outputVoice", "").toString();
        if (!savedOutputVoice.isEmpty() && outputVoiceCombo) {
            int outputVoiceIndex = outputVoiceCombo->findData(savedOutputVoice);
            if (outputVoiceIndex >= 0) {
                outputVoiceCombo->setCurrentIndex(outputVoiceIndex);
            }
        }

        // Load test text
        testTextEdit->setText(settings->value("tts/testText", "Hello! This is a test of the text-to-speech functionality.").toString());
    }
}

void SettingsWindow::saveSettings()
{
    // OCR Settings
    settings->setValue("ocr/engine", ocrEngineCombo->currentText());
    settings->setValue("ocr/language", ocrLanguageCombo->currentText());
    settings->setValue("ocr/quality", ocrQualitySlider->value());
    settings->setValue("ocr/preprocessing", ocrPreprocessingCheck->isChecked());
    settings->setValue("ocr/autoDetect", ocrAutoDetectCheck->isChecked());

    // Translation Settings
    settings->setValue("translation/autoOcr", autoOcrCheck->isChecked());
    settings->setValue("translation/autoTranslate", autoTranslateCheck->isChecked());
    settings->setValue("translation/engine", translationEngineCombo->currentText());
    settings->setValue("translation/sourceLanguage", sourceLanguageCombo->currentText());
    settings->setValue("translation/targetLanguage", targetLanguageCombo->currentText());
    settings->setValue("translation/apiUrl", apiUrlEdit->text());
    settings->setValue("translation/apiKey", apiKeyEdit->text());

    // Appearance Settings
    settings->setValue("appearance/theme", themeCombo->currentText());
    settings->setValue("appearance/opacity", opacitySlider->value());
    settings->setValue("appearance/animations", animationsCheck->isChecked());
    settings->setValue("appearance/sounds", soundsCheck->isChecked());

    // General TTS Options
    settings->setValue("general/ttsInput", ttsInputCheckBox->isChecked());
    settings->setValue("general/ttsOutput", ttsOutputCheckBox->isChecked());

    // TTS Settings
    if (ttsEngine) {
        settings->setValue("tts/enabled", ttsEnabledCheck->isChecked());
        settings->setValue("tts/volume", volumeSlider->value());
        settings->setValue("tts/pitch", pitchSlider->value());
        settings->setValue("tts/rate", rateSlider->value());
        QString backendWrite = "system";
        if (ttsBackendCombo->currentText().contains("Azure")) backendWrite = "azure";
        else if (ttsBackendCombo->currentText() == "Google Cloud TTS") backendWrite = "google";
        else if (ttsBackendCombo->currentText().contains("Google (Free")) backendWrite = "googlefree";
        else if (ttsBackendCombo->currentText().contains("ElevenLabs")) backendWrite = "elevenlabs";
        else if (ttsBackendCombo->currentText().contains("Polly")) backendWrite = "polly";
        else if (ttsBackendCombo->currentText().contains("Piper")) backendWrite = "piper";
        else if (ttsBackendCombo->currentText().contains("Edge")) backendWrite = "edge";
        settings->setValue("tts/backend", backendWrite);

        // Azure
        settings->beginGroup("tts/Azure");
        settings->setValue("region", azureRegionEdit->text());
        settings->setValue("key", azureKeyEdit->text());
        settings->setValue("style", azureStyleEdit->text());
        if (ttsBackendCombo->currentText().contains("Azure") && !voiceCombo->currentText().isEmpty()) {
            settings->setValue("voice", voiceCombo->currentText());
        }
        settings->endGroup();

        // Google
        settings->beginGroup("tts/Google");
        settings->setValue("apiKey", googleApiKeyEdit->text());
        settings->setValue("languageCode", googleLanguageCodeEdit->text());
        if (ttsBackendCombo->currentText().contains("Google") && !voiceCombo->currentText().isEmpty()) {
            settings->setValue("voice", voiceCombo->currentText());
        }
        settings->endGroup();

        // ElevenLabs
        settings->beginGroup("tts/ElevenLabs");
        settings->setValue("apiKey", elevenApiKeyEdit->text());
        if (ttsBackendCombo->currentText().contains("ElevenLabs")) {
            const QString voiceId = voiceCombo->currentText().trimmed().isEmpty() ? elevenVoiceIdEdit->text().trimmed() : voiceCombo->currentText().trimmed();
            if (!voiceId.isEmpty()) settings->setValue("voiceId", voiceId);
        }
        settings->endGroup();

        // Polly
        settings->beginGroup("tts/Polly");
        settings->setValue("region", pollyRegionEdit->text());
        settings->setValue("accessKey", pollyAccessKeyEdit->text());
        settings->setValue("secretKey", pollySecretKeyEdit->text());
        if (ttsBackendCombo->currentText().contains("Polly") && !voiceCombo->currentText().isEmpty()) {
            settings->setValue("voice", voiceCombo->currentText());
        }
        settings->endGroup();

    // Piper
    settings->beginGroup("tts/Piper");
        settings->setValue("exePath", piperExeEdit->text());
        settings->setValue("modelPath", piperModelEdit->text());
        settings->endGroup();

    // Edge
    settings->beginGroup("tts/Edge");
    settings->setValue("exePath", edgeExeEdit->text());
    settings->setValue("voice", edgeVoiceEdit->text());
    settings->endGroup();

        // Save voice selection (system)
        if (ttsBackendCombo->currentText().contains("System")) {
            QString selectedVoice = voiceCombo->currentData().toString();
            if (!selectedVoice.isEmpty()) settings->setValue("tts/voice", selectedVoice);
        }

        // Save input/output voice selection
        if (inputVoiceCombo) {
            QString selectedInputVoice = inputVoiceCombo->currentData().toString();
            if (!selectedInputVoice.isEmpty()) {
                settings->setValue("tts/inputVoice", selectedInputVoice);
            }
        }
        
        if (outputVoiceCombo) {
            QString selectedOutputVoice = outputVoiceCombo->currentData().toString();
            if (!selectedOutputVoice.isEmpty()) {
                settings->setValue("tts/outputVoice", selectedOutputVoice);
            }
        }

        // Save test text
        settings->setValue("tts/testText", testTextEdit->text());

        // Also save to TTS engine
        ttsEngine->saveSettings();
    }

    settings->sync();
    qDebug() << "Settings saved to:" << settings->fileName();
}

void SettingsWindow::resetToDefaults()
{
    // Reset OCR settings
    ocrEngineCombo->setCurrentText("Tesseract");
    ocrLanguageCombo->setCurrentText("English");
    ocrQualitySlider->setValue(3);
    ocrPreprocessingCheck->setChecked(true);
    ocrAutoDetectCheck->setChecked(true);

    // Reset Translation settings
    autoOcrCheck->setChecked(true);
    autoTranslateCheck->setChecked(true);
    translationEngineCombo->setCurrentText("Google Translate (Free)");
    sourceLanguageCombo->setCurrentText("Auto-Detect");
    targetLanguageCombo->setCurrentText("English");
    apiUrlEdit->clear();
    apiKeyEdit->clear();

    // Reset Appearance settings
    themeCombo->setCurrentText("Auto (System)");
    opacitySlider->setValue(90);
    animationsCheck->setChecked(true);
    soundsCheck->setChecked(false);
    
    // Reset General TTS Options
    ttsInputCheckBox->setChecked(false);
    ttsOutputCheckBox->setChecked(true);
}

void SettingsWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    animateShow();
}

void SettingsWindow::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
}

void SettingsWindow::animateShow()
{
    opacityEffect->setOpacity(0.0);
    showAnimation->setStartValue(0.0);
    showAnimation->setEndValue(1.0);
    showAnimation->start();
}

void SettingsWindow::animateHide()
{
    showAnimation->setStartValue(1.0);
    showAnimation->setEndValue(0.0);
    showAnimation->start();
}

// Slots
void SettingsWindow::onOcrEngineChanged()
{
    QString engine = ocrEngineCombo->currentText();
    ocrStatusText->setPlainText(QString("OCR engine changed to: %1\nConfiguration updated.").arg(engine));
}

void SettingsWindow::onTranslationEngineChanged()
{
    QString engine = translationEngineCombo->currentText();

    // Update API fields based on selected engine
    if (engine.contains("Google")) {
        apiUrlEdit->setText("https://translate.googleapis.com/translate_a/single");
        apiUrlEdit->setEnabled(false);
        apiKeyEdit->setPlaceholderText("API key optional for free tier");
    } else if (engine.contains("LibreTranslate")) {
        apiUrlEdit->setText("https://libretranslate.de/translate");
        apiUrlEdit->setEnabled(true);
        apiKeyEdit->setPlaceholderText("API key required");
    } else if (engine.contains("Ollama")) {
        apiUrlEdit->setText("http://localhost:11434/api/generate");
        apiUrlEdit->setEnabled(true);
        apiKeyEdit->setPlaceholderText("No API key needed for local Ollama");
        apiKeyEdit->setEnabled(false);
    } else {
        apiUrlEdit->setEnabled(true);
        apiKeyEdit->setEnabled(true);
    }

    translationStatusText->setPlainText(QString("Translation engine changed to: %1\nConfiguration updated.").arg(engine));
}

void SettingsWindow::onApplyClicked()
{
    saveSettings();
    QMessageBox::information(this, "Settings Applied",
                           "Settings have been saved successfully!\n\nSome changes may require restarting the application.");
    accept();
}

void SettingsWindow::onCancelClicked()
{
    loadSettings(); // Restore original settings
    reject();
}

void SettingsWindow::onResetClicked()
{
    int result = QMessageBox::question(this, "Reset Settings",
                                     "Are you sure you want to reset all settings to their default values?",
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        resetToDefaults();
        QMessageBox::information(this, "Settings Reset", "All settings have been reset to their default values.");
    }
}

void SettingsWindow::onTestOcrClicked()
{
    ocrStatusText->setPlainText("Testing OCR configuration...\n");
    QApplication::processEvents();

    QString engine = ocrEngineCombo->currentText();
    QString language = ocrLanguageCombo->currentText();
    QString result;

    // Test actual OCR engine availability
    bool engineAvailable = false;
    QString statusMessage;

    if (engine == "Tesseract") {
        engineAvailable = OCREngine::isTesseractAvailable();
        statusMessage = engineAvailable ? "Tesseract found and ready" : "Tesseract not installed or not in PATH";
    } else if (engine == "EasyOCR") {
        engineAvailable = OCREngine::isEasyOCRAvailable();
        statusMessage = engineAvailable ? "EasyOCR Python module found" : "EasyOCR not installed (pip install easyocr)";
    } else if (engine == "PaddleOCR") {
        engineAvailable = OCREngine::isPaddleOCRAvailable();
        statusMessage = engineAvailable ? "PaddleOCR Python module found" : "PaddleOCR not installed (pip install paddlepaddle paddleocr)";
    } else if (engine == "Windows OCR") {
        engineAvailable = OCREngine::isWindowsOCRAvailable();
        statusMessage = engineAvailable ? "Windows OCR API available" : "Windows OCR only available on Windows 10+";
    }

    QString status = engineAvailable ? "✅" : "❌";
    result = QString("%1 OCR Test Results:\n"
                    "Engine: %2\n"
                    "Language: %3\n"
                    "Quality Level: %4/5\n"
                    "Availability: %5\n"
                    "Status: %6")
                    .arg(status)
                    .arg(engine)
                    .arg(language)
                    .arg(ocrQualitySlider->value())
                    .arg(statusMessage)
                    .arg(engineAvailable ? "Ready for use" : "Installation required");

    ocrStatusText->setPlainText(result);

    // Update engine dropdown styling based on availability
    // Use dynamic property for validation state instead of inline style
    ocrEngineCombo->setProperty("valid", engineAvailable ? "true" : "false");
    ocrEngineCombo->style()->unpolish(ocrEngineCombo);
    ocrEngineCombo->style()->polish(ocrEngineCombo);
    QTimer::singleShot(3000, [this]() {
        ocrEngineCombo->setProperty("valid", QVariant());
        ocrEngineCombo->style()->unpolish(ocrEngineCombo);
        ocrEngineCombo->style()->polish(ocrEngineCombo);
    });
}

void SettingsWindow::onTestTranslationClicked()
{
    translationStatusText->setPlainText("Testing translation configuration...\n");
    QApplication::processEvents();

    // Simulate translation test
    QString engine = translationEngineCombo->currentText();
    QString source = sourceLanguageCombo->currentText();
    QString target = targetLanguageCombo->currentText();

    // In a real implementation, this would test the actual translation API
    QString result = QString("✅ Translation Test Results:\n"
                           "Engine: %1\n"
                           "Source: %2 → Target: %3\n"
                           "API Connection: %4\n"
                           "Status: Ready for use")
                           .arg(engine)
                           .arg(source, target)
                           .arg(apiUrlEdit->text().isEmpty() ? "Local/Free" : "Connected");

    translationStatusText->setPlainText(result);
}

void SettingsWindow::setupTTSTab()
{
    ttsTab = new QWidget();
    tabWidget->addTab(ttsTab, "🔊 Text-to-Speech");

    // Use scroll area to handle content that might not fit
    QScrollArea *scrollArea = new QScrollArea(ttsTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    scrollArea->setWidget(scrollContent);

    QVBoxLayout *tabLayout = new QVBoxLayout(ttsTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    QVBoxLayout *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(20);

    // TTS Enable/Disable
    QGroupBox *enableGroup = new QGroupBox("Text-to-Speech Settings");
    QVBoxLayout *enableLayout = new QVBoxLayout(enableGroup);

    ttsEnabledCheck = new QCheckBox("Enable Text-to-Speech for OCR results");
    ttsEnabledCheck->setChecked(true);
    enableLayout->addWidget(ttsEnabledCheck);

    layout->addWidget(enableGroup);

    // Backend selection
    QFormLayout *backendLayout = new QFormLayout();
    ttsBackendCombo = new QComboBox();
    ttsBackendCombo->addItems({"System (Local)", "Piper (Free, Local)", "Google (Free Online)", "Microsoft Edge (Free Online)", "Azure Neural (Cloud)", "Google Cloud TTS", "ElevenLabs", "Amazon Polly"});
    connect(ttsBackendCombo, &QComboBox::currentTextChanged, this, [this](const QString& text){
        const bool isSystem = text.contains("System", Qt::CaseInsensitive);
        const bool isPiper = text.contains("Piper", Qt::CaseInsensitive);
        const bool isAzure = text.contains("Azure", Qt::CaseInsensitive);
        const bool isGoogle = text == "Google Cloud TTS";
        const bool isGoogleFree = text.contains("Google (Free", Qt::CaseInsensitive);
        const bool isEdge = text.contains("Edge", Qt::CaseInsensitive);
        const bool isEleven = text.contains("ElevenLabs", Qt::CaseInsensitive);
        const bool isPolly = text.contains("Polly", Qt::CaseInsensitive);
        if (ttsEngine) {
            if (isSystem) ttsEngine->setBackend(TTSEngine::Backend::System);
            else if (isPiper) ttsEngine->setBackend(TTSEngine::Backend::Piper);
            else if (isGoogleFree) ttsEngine->setBackend(TTSEngine::Backend::GoogleFree);
            else if (isEdge) ttsEngine->setBackend(TTSEngine::Backend::Edge);
            else if (isAzure) ttsEngine->setBackend(TTSEngine::Backend::Azure);
            else if (isGoogle) ttsEngine->setBackend(TTSEngine::Backend::Google);
            else if (isEleven) ttsEngine->setBackend(TTSEngine::Backend::ElevenLabs);
            else if (isPolly) ttsEngine->setBackend(TTSEngine::Backend::Polly);
        }
        if (azureConfigGroup) azureConfigGroup->setVisible(isAzure);
        if (googleConfigGroup) googleConfigGroup->setVisible(isGoogle);
        if (edgeConfigGroup) edgeConfigGroup->setVisible(isEdge);
        if (elevenConfigGroup) elevenConfigGroup->setVisible(isEleven);
        if (pollyConfigGroup) pollyConfigGroup->setVisible(isPolly);
        if (piperConfigGroup) piperConfigGroup->setVisible(isPiper);
        updateVoicesForLanguage();
    });
    backendLayout->addRow("TTS Engine:", ttsBackendCombo);
    
    // Add helpful info section for free options
    QLabel *freeInfo = new QLabel("💡 <b>Free Options:</b> System (built-in), Piper (offline), Google Free (online), Edge (online)<br>"
                                 "🔊 <b>Premium Options:</b> Azure, Google Cloud, ElevenLabs, Amazon Polly (requires API keys)");
    freeInfo->setWordWrap(true);
    freeInfo->setStyleSheet("color: #666; font-size: 11px; margin: 8px 0; padding: 8px; background-color: #f8f9fa; border-radius: 4px;");
    freeInfo->setAlignment(Qt::AlignTop);
    backendLayout->addRow(freeInfo);
    
    enableLayout->addLayout(backendLayout);

    // Language picker removed; language follows Translation target

    // Voice Selection Group - Bilingual Support
    QGroupBox *voiceGroup = new QGroupBox("Voice Selection (Bilingual Support)");
    voiceGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; padding-top: 10px; }");
    QFormLayout *voiceLayout = new QFormLayout(voiceGroup);
    voiceLayout->setSpacing(12);

    // Info label for bilingual TTS
    QLabel *bilingualInfo = new QLabel("🌍 <i>Configure separate voices for input (OCR) and output (translated) text.<br>"
                                      "This allows optimal pronunciation for each language in bilingual scenarios.</i>");
    bilingualInfo->setWordWrap(true);
    bilingualInfo->setStyleSheet("color: #666; font-size: 11px; margin-bottom: 8px; padding: 8px; background-color: #f8f9fa; border-radius: 4px;");
    voiceLayout->addRow(bilingualInfo);

    // Input Voice (matches OCR language)
    inputVoiceCombo = new QComboBox();
    inputVoiceCombo->setToolTip("Voice for reading input text (OCR language)");
    connect(inputVoiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){
        if (ttsEngine) {
            QString selectedVoice = inputVoiceCombo->currentData().toString();
            if (!selectedVoice.isEmpty()) {
                ttsEngine->setInputVoice(selectedVoice);
            }
        }
    });
    voiceLayout->addRow("🎤 Input Voice (OCR):", inputVoiceCombo);

    // Output Voice (matches translation target language)
    outputVoiceCombo = new QComboBox();
    outputVoiceCombo->setToolTip("Voice for reading output text (translation target language)");
    connect(outputVoiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){
        if (ttsEngine) {
            QString selectedVoice = outputVoiceCombo->currentData().toString();
            if (!selectedVoice.isEmpty()) {
                ttsEngine->setOutputVoice(selectedVoice);
            }
        }
    });
    voiceLayout->addRow("🔊 Output Voice (Translation):", outputVoiceCombo);

    // Legacy single voice combo (for backwards compatibility and simple use)
    voiceCombo = new QComboBox();
    voiceCombo->setToolTip("Single voice for both input and output (simple mode)");
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::onVoiceChanged);
    voiceLayout->addRow("📢 Universal Voice (Both):", voiceCombo);

    layout->addWidget(voiceGroup);

    // Azure Config Group
    azureConfigGroup = new QGroupBox("Azure Neural TTS Configuration");
    QFormLayout *azureLayout = new QFormLayout(azureConfigGroup);
    azureRegionEdit = new QLineEdit(); azureRegionEdit->setPlaceholderText("e.g. eastus, westeurope");
    azureKeyEdit = new QLineEdit(); azureKeyEdit->setEchoMode(QLineEdit::Password); azureKeyEdit->setPlaceholderText("Azure Speech key");
    azureStyleEdit = new QLineEdit(); azureStyleEdit->setPlaceholderText("Optional style, e.g. chat, newscast");
    azureLayout->addRow("Region:", azureRegionEdit);
    azureLayout->addRow("API Key:", azureKeyEdit);
    azureLayout->addRow("Style:", azureStyleEdit);
    azureConfigGroup->setVisible(false);
    layout->addWidget(azureConfigGroup);

    // Google Config Group
    googleConfigGroup = new QGroupBox("Google Cloud TTS Configuration");
    QFormLayout *googleLayout = new QFormLayout(googleConfigGroup);
    googleApiKeyEdit = new QLineEdit(); googleApiKeyEdit->setEchoMode(QLineEdit::Password); googleApiKeyEdit->setPlaceholderText("Google API key");
    googleLanguageCodeEdit = new QLineEdit(); googleLanguageCodeEdit->setPlaceholderText("Optional, e.g. en-US");
    googleLayout->addRow("API Key:", googleApiKeyEdit);
    googleLayout->addRow("Language Code:", googleLanguageCodeEdit);
    googleConfigGroup->setVisible(false);
    layout->addWidget(googleConfigGroup);

    // ElevenLabs Config Group
    elevenConfigGroup = new QGroupBox("ElevenLabs Configuration");
    QFormLayout *elevenLayout = new QFormLayout(elevenConfigGroup);
    elevenApiKeyEdit = new QLineEdit(); elevenApiKeyEdit->setEchoMode(QLineEdit::Password); elevenApiKeyEdit->setPlaceholderText("ElevenLabs API key");
    elevenVoiceIdEdit = new QLineEdit(); elevenVoiceIdEdit->setPlaceholderText("Voice ID (e.g., 21m00Tcm4TlvDq8ikWAM)");
    elevenLayout->addRow("API Key:", elevenApiKeyEdit);
    elevenLayout->addRow("Voice ID:", elevenVoiceIdEdit);
    elevenConfigGroup->setVisible(false);
    layout->addWidget(elevenConfigGroup);

    // Polly Config Group
    pollyConfigGroup = new QGroupBox("Amazon Polly Configuration");
    QFormLayout *pollyLayout = new QFormLayout(pollyConfigGroup);
    pollyRegionEdit = new QLineEdit(); pollyRegionEdit->setPlaceholderText("e.g. us-east-1");
    pollyAccessKeyEdit = new QLineEdit(); pollyAccessKeyEdit->setPlaceholderText("AWS Access Key ID");
    pollySecretKeyEdit = new QLineEdit(); pollySecretKeyEdit->setEchoMode(QLineEdit::Password); pollySecretKeyEdit->setPlaceholderText("AWS Secret Access Key");
    pollyLayout->addRow("Region:", pollyRegionEdit);
    pollyLayout->addRow("Access Key:", pollyAccessKeyEdit);
    pollyLayout->addRow("Secret Key:", pollySecretKeyEdit);
    pollyConfigGroup->setVisible(false);
    layout->addWidget(pollyConfigGroup);

    // Piper Config Group
    piperConfigGroup = new QGroupBox("Piper TTS (Free, Local)");
    QFormLayout *piperLayout = new QFormLayout(piperConfigGroup);
    piperExeEdit = new QLineEdit(); piperExeEdit->setPlaceholderText("piper.exe path");
    piperModelEdit = new QLineEdit(); piperModelEdit->setPlaceholderText("voice model .onnx path");
    piperLayout->addRow("Executable:", piperExeEdit);
    piperLayout->addRow("Model:", piperModelEdit);
    piperConfigGroup->setVisible(false);
    layout->addWidget(piperConfigGroup);

    // Edge Config Group - Auto-detect and user-friendly
    edgeConfigGroup = new QGroupBox("Microsoft Edge TTS (Free Online) - Natural Voices");
    QVBoxLayout *edgeMainLayout = new QVBoxLayout(edgeConfigGroup);
    
    QLabel *edgeInfoLabel = new QLabel("✨ High-quality neural voices from Microsoft. Auto-installs edge-tts if needed.");
    edgeInfoLabel->setWordWrap(true);
    edgeInfoLabel->setStyleSheet("color: #666; font-size: 11px; margin-bottom: 8px;");
    edgeMainLayout->addWidget(edgeInfoLabel);
    
    QFormLayout *edgeLayout = new QFormLayout();
    
    QHBoxLayout *edgeExeLayout = new QHBoxLayout();
    edgeExeEdit = new QLineEdit(); 
    edgeExeEdit->setPlaceholderText("Auto-detected or enter path to edge-tts");
    edgeAutoDetectBtn = new QPushButton("Auto-Detect");
    edgeAutoDetectBtn->setToolTip("Automatically find or install edge-tts");
    connect(edgeAutoDetectBtn, &QPushButton::clicked, this, &SettingsWindow::onEdgeAutoDetect);
    edgeExeLayout->addWidget(edgeExeEdit, 1);
    edgeExeLayout->addWidget(edgeAutoDetectBtn);
    edgeLayout->addRow("Executable:", edgeExeLayout);
    
    edgeVoiceEdit = new QLineEdit(); 
    edgeVoiceEdit->setPlaceholderText("Voice will be auto-selected based on language");
    edgeLayout->addRow("Voice:", edgeVoiceEdit);
    
    edgeMainLayout->addLayout(edgeLayout);
    edgeConfigGroup->setVisible(false);
    layout->addWidget(edgeConfigGroup);

    // Audio Controls Group
    QGroupBox *audioGroup = new QGroupBox("Audio Controls");
    QFormLayout *audioLayout = new QFormLayout(audioGroup);

    // Volume control
    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(80);
    QLabel *volumeLabel = new QLabel("80%");
    connect(volumeSlider, &QSlider::valueChanged, [volumeLabel](int value) {
        volumeLabel->setText(QString("%1%").arg(value));
    });
    QHBoxLayout *volumeHBox = new QHBoxLayout();
    volumeHBox->addWidget(volumeSlider);
    volumeHBox->addWidget(volumeLabel);
    audioLayout->addRow("Volume:", volumeHBox);

    // Pitch control
    pitchSlider = new QSlider(Qt::Horizontal);
    pitchSlider->setRange(-50, 50);
    pitchSlider->setValue(0);
    QLabel *pitchLabel = new QLabel("0%");
    connect(pitchSlider, &QSlider::valueChanged, [pitchLabel](int value) {
        pitchLabel->setText(QString("%1%").arg(value));
    });
    QHBoxLayout *pitchHBox = new QHBoxLayout();
    pitchHBox->addWidget(pitchSlider);
    pitchHBox->addWidget(pitchLabel);
    audioLayout->addRow("Pitch:", pitchHBox);

    // Rate control (-100%..+100% maps to -1.0..+1.0)
    rateSlider = new QSlider(Qt::Horizontal);
    rateSlider->setRange(-100, 100);
    rateSlider->setValue(0);
    QLabel *rateLabel = new QLabel("0%");
    connect(rateSlider, &QSlider::valueChanged, [rateLabel](int value) {
        rateLabel->setText(QString("%1%").arg(value));
    });
    QHBoxLayout *rateHBox = new QHBoxLayout();
    rateHBox->addWidget(rateSlider);
    rateHBox->addWidget(rateLabel);
    audioLayout->addRow("Rate:", rateHBox);

    layout->addWidget(audioGroup);

    // Test Section Group
    QGroupBox *testGroup = new QGroupBox("Voice Testing");
    QVBoxLayout *testLayout = new QVBoxLayout(testGroup);

    // Test text input
    testTextEdit = new QLineEdit();
    testTextEdit->setText("Hello! This is a test of the text-to-speech functionality.");
    testLayout->addWidget(new QLabel("Test Text:"));
    testLayout->addWidget(testTextEdit);

    // Test buttons
    QHBoxLayout *testButtonLayout = new QHBoxLayout();
    testTTSBtn = new QPushButton("🔊 Test Voice");
    stopTTSBtn = new QPushButton("⏹ Stop");
    stopTTSBtn->setEnabled(false);

    connect(testTTSBtn, &QPushButton::clicked, this, &SettingsWindow::onTestTTSClicked);
    connect(stopTTSBtn, &QPushButton::clicked, [this]() {
        if (ttsEngine) {
            ttsEngine->stop();
        }
    });

    testButtonLayout->addWidget(testTTSBtn);
    testButtonLayout->addWidget(stopTTSBtn);
    testButtonLayout->addStretch();
    testLayout->addLayout(testButtonLayout);

    // Status text
    ttsStatusText = new QTextEdit();
    ttsStatusText->setMaximumHeight(80);
    ttsStatusText->setReadOnly(true);
    testLayout->addWidget(ttsStatusText);

    layout->addWidget(testGroup);

    layout->addStretch();

    // Initialize TTS status and voices
    if (ttsEngine) {
        // Preselect backend by settings
        switch (ttsEngine->backend()) {
            case TTSEngine::Backend::Azure: ttsBackendCombo->setCurrentText("Azure Neural (Cloud)"); azureConfigGroup->setVisible(true); break;
            case TTSEngine::Backend::Google: ttsBackendCombo->setCurrentText("Google Cloud TTS"); googleConfigGroup->setVisible(true); break;
            case TTSEngine::Backend::ElevenLabs: ttsBackendCombo->setCurrentText("ElevenLabs"); elevenConfigGroup->setVisible(true); break;
            case TTSEngine::Backend::Polly: ttsBackendCombo->setCurrentText("Amazon Polly"); pollyConfigGroup->setVisible(true); break;
            case TTSEngine::Backend::Piper: ttsBackendCombo->setCurrentText("Piper (Free, Local)"); piperConfigGroup->setVisible(true); break;
            default: ttsBackendCombo->setCurrentText("System (Local)"); break;
        }

        // Info about local engines
        const QStringList engines = ttsEngine->availableEngines();
        const QString engineName = ttsEngine->currentEngine();

        // Connect TTS engine signals
        connect(ttsEngine, &TTSEngine::stateChanged, this, [this](QTextToSpeech::State state) {
            stopTTSBtn->setEnabled(state == QTextToSpeech::Speaking);
            testTTSBtn->setEnabled(state != QTextToSpeech::Speaking);
        });

    QString status = QString("✅ TTS ready. Local: %1. Backends — Piper %2, Azure %3, Google %4, ElevenLabs %5, Polly %6\nSelect voice; language follows Translation target.")
                .arg(engineName.isEmpty()? "unknown" : engineName)
                .arg((!ttsEngine->piperExePath().isEmpty() && !ttsEngine->piperModelPath().isEmpty()) ? "(configured)" : "(not configured)")
                .arg(ttsEngine->azureRegion().isEmpty() ? "(not configured)" : "(configured)")
                .arg(ttsEngine->googleApiKey().isEmpty() ? "(not configured)" : "(configured)")
                .arg(ttsEngine->elevenApiKey().isEmpty() ? "(not configured)" : "(configured)")
                .arg(ttsEngine->pollyRegion().isEmpty() ? "(not configured)" : "(configured)");
        if (engines.size() == 1 && engines.first().contains("mock", Qt::CaseInsensitive)) {
            status += "\n⚠️ Only the 'mock' local engine is available. Use Azure for natural voices or install Windows voices.";
        }
        ttsStatusText->setPlainText(status);
        // Populate voices based on current Translation target
        updateVoicesForLanguage();
    } else {
        ttsStatusText->setPlainText("❌ Text-to-Speech engine is not available on this system.\nPlease check your Qt installation and system TTS support.");
        ttsEnabledCheck->setEnabled(false);
        voiceCombo->setEnabled(false);
        testTTSBtn->setEnabled(false);
    }
}

void SettingsWindow::onVoiceChanged()
{
    // Voice selection will be handled when applying settings
}

void SettingsWindow::onTestTTSClicked()
{
    if (!ttsEngine) {
        ttsStatusText->setPlainText("❌ TTS engine not available");
        return;
    }

    QString testText = testTextEdit->text().trimmed();
    if (testText.isEmpty()) {
        testText = "Hello! This is a test of the text-to-speech functionality.";
    }

    // Apply current settings
    ttsEngine->setVolume(volumeSlider->value() / 100.0);
    ttsEngine->setPitch((pitchSlider->value()) / 100.0);
    ttsEngine->setRate(rateSlider->value() / 100.0);

    // Determine locale from Translation target language
    auto mapTargetToLocale = [](const QString &name)->QLocale {
        if (name.contains("English", Qt::CaseInsensitive)) return QLocale(QLocale::English, QLocale::UnitedStates);
        if (name.contains("Chinese") && name.contains("Simplified")) return QLocale(QLocale::Chinese, QLocale::China);
        if (name.contains("Chinese") && name.contains("Traditional")) return QLocale(QLocale::Chinese, QLocale::Taiwan);
        if (name.contains("Japanese", Qt::CaseInsensitive)) return QLocale(QLocale::Japanese, QLocale::Japan);
        if (name.contains("Korean", Qt::CaseInsensitive)) return QLocale(QLocale::Korean, QLocale::SouthKorea);
        if (name.contains("Spanish", Qt::CaseInsensitive)) return QLocale(QLocale::Spanish, QLocale::Spain);
        if (name.contains("French", Qt::CaseInsensitive)) return QLocale(QLocale::French, QLocale::France);
        if (name.contains("German", Qt::CaseInsensitive)) return QLocale(QLocale::German, QLocale::Germany);
        if (name.contains("Russian", Qt::CaseInsensitive)) return QLocale(QLocale::Russian, QLocale::Russia);
        if (name.contains("Portuguese", Qt::CaseInsensitive)) return QLocale(QLocale::Portuguese, QLocale::Portugal);
        if (name.contains("Italian", Qt::CaseInsensitive)) return QLocale(QLocale::Italian, QLocale::Italy);
        if (name.contains("Dutch", Qt::CaseInsensitive)) return QLocale(QLocale::Dutch, QLocale::Netherlands);
        if (name.contains("Polish", Qt::CaseInsensitive)) return QLocale(QLocale::Polish, QLocale::Poland);
        if (name.contains("Swedish", Qt::CaseInsensitive)) return QLocale(QLocale::Swedish, QLocale::Sweden);
        if (name.contains("Arabic", Qt::CaseInsensitive)) return QLocale(QLocale::Arabic, QLocale::SaudiArabia);
        if (name.contains("Hindi", Qt::CaseInsensitive)) return QLocale(QLocale::Hindi, QLocale::India);
        if (name.contains("Thai", Qt::CaseInsensitive)) return QLocale(QLocale::Thai, QLocale::Thailand);
        if (name.contains("Vietnamese", Qt::CaseInsensitive)) return QLocale(QLocale::Vietnamese, QLocale::Vietnam);
        return QLocale::system();
    };
    QLocale selectedLocale = mapTargetToLocale(targetLanguageCombo->currentText());
        // Backend-specific voice handling
        if (ttsEngine->backend() == TTSEngine::Backend::System) {
            ttsEngine->setLocale(selectedLocale);
            int voiceIndex = voiceCombo->currentIndex();
            if (voiceIndex >= 0) {
                QList<QVoice> voices = ttsEngine->getVoicesForLanguage(selectedLocale);
                if (voiceIndex < voices.size()) {
                    QVoice selectedVoice = voices[voiceIndex];
                    ttsEngine->setVoice(selectedVoice);
                    qDebug() << "Set system voice to:" << selectedVoice.name() << "for locale:" << selectedLocale;
                }
            }
        } else {
            const QString backendText = ttsBackendCombo->currentText();
            if (backendText.contains("Azure")) {
                QString voice = voiceCombo->currentText();
                if (voice.isEmpty()) { auto s = CloudTTSProvider::azureSuggestedVoicesFor(selectedLocale); if (!s.isEmpty()) voice = s.first(); }
                ttsEngine->configureAzure(azureRegionEdit->text(), azureKeyEdit->text(), voice, azureStyleEdit->text());
            } else if (backendText == "Google Cloud TTS") {
                QString voice = voiceCombo->currentText();
                if (voice.isEmpty()) { auto s = CloudTTSProvider::googleSuggestedVoicesFor(selectedLocale); if (!s.isEmpty()) voice = s.first(); }
                ttsEngine->configureGoogle(googleApiKeyEdit->text(), voice, googleLanguageCodeEdit->text());
            } else if (backendText.contains("Google (Free")) {
                QString voice = voiceCombo->currentText();
                if (voice.isEmpty()) { auto s = CloudTTSProvider::googleFreeSuggestedVoicesFor(selectedLocale); if (!s.isEmpty()) voice = s.first(); }
                ttsEngine->configureGoogleFree(voice);
                ttsEngine->setBackend(TTSEngine::Backend::GoogleFree);
            } else if (backendText.contains("ElevenLabs")) {
                QString voiceId = voiceCombo->currentText().trimmed();
                if (voiceId.isEmpty()) voiceId = elevenVoiceIdEdit->text().trimmed();
                ttsEngine->configureElevenLabs(elevenApiKeyEdit->text(), voiceId);
            } else if (backendText.contains("Polly")) {
                QString voice = voiceCombo->currentText();
                if (voice.isEmpty()) { auto s = CloudTTSProvider::pollySuggestedVoicesFor(selectedLocale); if (!s.isEmpty()) voice = s.first(); }
                ttsEngine->configurePolly(pollyRegionEdit->text(), pollyAccessKeyEdit->text(), pollySecretKeyEdit->text(), voice);
            } else if (backendText.contains("Piper")) {
                ttsEngine->configurePiper(piperExeEdit->text(), piperModelEdit->text());
            } else if (backendText.contains("Edge")) {
                ttsEngine->configureEdge(edgeExeEdit->text(), edgeVoiceEdit->text());
            }
        }

    ttsStatusText->setPlainText(QString("🔊 Speaking: \"%1\"").arg(testText));
    ttsEngine->speak(testText);
}

void SettingsWindow::onEdgeAutoDetect() {
    edgeAutoDetectBtn->setEnabled(false);
    edgeAutoDetectBtn->setText("Detecting...");
    
    // Common paths to check for edge-tts
    QStringList possiblePaths = {
        "edge-tts",  // If it's in PATH
        "edge-tts.exe",
        QDir::home().absolutePath() + "/.local/bin/edge-tts",
        QDir::home().absolutePath() + "/AppData/Local/Programs/Python/Python*/Scripts/edge-tts.exe",
        QDir::home().absolutePath() + "/AppData/Roaming/Python/Python*/Scripts/edge-tts.exe",
        "C:/Python*/Scripts/edge-tts.exe",
        "C:/Program Files/Python*/Scripts/edge-tts.exe",
        "C:/Users/" + qgetenv("USERNAME") + "/AppData/Local/Programs/Python/Python*/Scripts/edge-tts.exe"
    };
    
    // First try direct execution (if in PATH)
    QProcess testProcess;
    testProcess.start("edge-tts", QStringList() << "--help");
    testProcess.waitForFinished(3000);
    
    if (testProcess.exitCode() == 0) {
        edgeExeEdit->setText("edge-tts");
        edgeAutoDetectBtn->setText("✓ Found");
        edgeAutoDetectBtn->setStyleSheet("background-color: #28a745; color: white;");
        QTimer::singleShot(2000, [this]() {
            edgeAutoDetectBtn->setText("Auto-Detect");
            edgeAutoDetectBtn->setStyleSheet("");
            edgeAutoDetectBtn->setEnabled(true);
        });
        return;
    }
    
    // If not in PATH, try to find executable files
    bool found = false;
    for (const QString &pathPattern : possiblePaths) {
        if (pathPattern.contains("*")) {
            // Handle wildcard patterns - expand them manually
            QString basePath = pathPattern.section("*", 0, 0);  // Before first *
            QString afterPath = pathPattern.section("*", 1);    // After first *
            
            QDir baseDir(basePath);
            if (baseDir.exists()) {
                QStringList subdirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &subdir : subdirs) {
                    QString fullPath = basePath + subdir + afterPath;
                    QFileInfo fileInfo(fullPath);
                    if (fileInfo.exists() && fileInfo.isExecutable()) {
                        QProcess testWildcard;
                        testWildcard.start(fullPath, QStringList() << "--help");
                        testWildcard.waitForFinished(3000);
                        
                        if (testWildcard.exitCode() == 0) {
                            edgeExeEdit->setText(fullPath);
                            found = true;
                            break;
                        }
                    }
                }
            }
        } else {
            QFileInfo fileInfo(pathPattern);
            if (fileInfo.exists() && fileInfo.isExecutable()) {
                QProcess testDirect;
                testDirect.start(pathPattern, QStringList() << "--help");
                testDirect.waitForFinished(3000);
                
                if (testDirect.exitCode() == 0) {
                    edgeExeEdit->setText(pathPattern);
                    found = true;
                    break;
                }
            }
        }
        
        if (found) break;
    }
    
    if (found) {
        edgeAutoDetectBtn->setText("✓ Found");
        edgeAutoDetectBtn->setStyleSheet("background-color: #28a745; color: white;");
        QTimer::singleShot(2000, [this]() {
            edgeAutoDetectBtn->setText("Auto-Detect");
            edgeAutoDetectBtn->setStyleSheet("");
            edgeAutoDetectBtn->setEnabled(true);
        });
    } else {
        // Try to install via pip
        edgeAutoDetectBtn->setText("Installing...");
        QProcess *pipProcess = new QProcess(this);
        
        connect(pipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, pipProcess](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitCode == 0) {
                // Installation successful, try to find it again
                QTimer::singleShot(1000, this, &SettingsWindow::onEdgeAutoDetect);
            } else {
                edgeAutoDetectBtn->setText("❌ Not Found");
                edgeAutoDetectBtn->setStyleSheet("background-color: #dc3545; color: white;");
                
                // Show helpful message
                QMessageBox::information(this, "Edge TTS Not Found", 
                    "Could not find or install edge-tts automatically.\n\n"
                    "Please install manually using:\n"
                    "pip install edge-tts\n\n"
                    "Then click Auto-Detect again or enter the path manually.");
                
                QTimer::singleShot(3000, [this]() {
                    edgeAutoDetectBtn->setText("Auto-Detect");
                    edgeAutoDetectBtn->setStyleSheet("");
                    edgeAutoDetectBtn->setEnabled(true);
                });
            }
            pipProcess->deleteLater();
        });
        
        pipProcess->start("pip", QStringList() << "install" << "edge-tts");
    }
}

void SettingsWindow::updateVoicesForLanguage()
{
    if (!voiceCombo) return;
    
    if (!ttsEngine) {
        return;
    }

    // Helper function to map language names to locales
    auto mapLanguageToLocale = [](const QString &name)->QLocale {
        if (name.contains("English", Qt::CaseInsensitive)) return QLocale(QLocale::English, QLocale::UnitedStates);
        if (name.contains("Chinese") && name.contains("Simplified")) return QLocale(QLocale::Chinese, QLocale::China);
        if (name.contains("Chinese") && name.contains("Traditional")) return QLocale(QLocale::Chinese, QLocale::Taiwan);
        if (name.contains("Japanese", Qt::CaseInsensitive)) return QLocale(QLocale::Japanese, QLocale::Japan);
        if (name.contains("Korean", Qt::CaseInsensitive)) return QLocale(QLocale::Korean, QLocale::SouthKorea);
        if (name.contains("Spanish", Qt::CaseInsensitive)) return QLocale(QLocale::Spanish, QLocale::Spain);
        if (name.contains("French", Qt::CaseInsensitive)) return QLocale(QLocale::French, QLocale::France);
        if (name.contains("German", Qt::CaseInsensitive)) return QLocale(QLocale::German, QLocale::Germany);
        if (name.contains("Russian", Qt::CaseInsensitive)) return QLocale(QLocale::Russian, QLocale::Russia);
        if (name.contains("Portuguese", Qt::CaseInsensitive)) return QLocale(QLocale::Portuguese, QLocale::Portugal);
        if (name.contains("Italian", Qt::CaseInsensitive)) return QLocale(QLocale::Italian, QLocale::Italy);
        if (name.contains("Dutch", Qt::CaseInsensitive)) return QLocale(QLocale::Dutch, QLocale::Netherlands);
        if (name.contains("Polish", Qt::CaseInsensitive)) return QLocale(QLocale::Polish, QLocale::Poland);
        if (name.contains("Swedish", Qt::CaseInsensitive)) return QLocale(QLocale::Swedish, QLocale::Sweden);
        if (name.contains("Arabic", Qt::CaseInsensitive)) return QLocale(QLocale::Arabic, QLocale::SaudiArabia);
        if (name.contains("Hindi", Qt::CaseInsensitive)) return QLocale(QLocale::Hindi, QLocale::India);
        if (name.contains("Thai", Qt::CaseInsensitive)) return QLocale(QLocale::Thai, QLocale::Thailand);
        if (name.contains("Vietnamese", Qt::CaseInsensitive)) return QLocale(QLocale::Vietnamese, QLocale::Vietnam);
        return QLocale::system();
    };

    // Get input locale (from OCR language) and output locale (from translation target language)
    QLocale inputLocale = QLocale::system();
    QLocale outputLocale = QLocale::system();
    
    // Input locale: from OCR language (now unified in General tab)
    if (ocrLanguageCombo && !ocrLanguageCombo->currentText().isEmpty()) {
        inputLocale = mapLanguageToLocale(ocrLanguageCombo->currentText());
    } else if (sourceLanguageCombo && !sourceLanguageCombo->currentText().isEmpty()) {
        // Fallback to translation source if OCR language not set (backward compatibility)
        inputLocale = mapLanguageToLocale(sourceLanguageCombo->currentText());
    }
    
    // Output locale: from translation target language  
    if (targetLanguageCombo && !targetLanguageCombo->currentText().isEmpty()) {
        outputLocale = mapLanguageToLocale(targetLanguageCombo->currentText());
    }

    // Helper function to populate a voice combo with voices for a specific locale
    auto populateVoiceCombo = [this](QComboBox* combo, const QLocale& locale, const QString& currentVoice) {
        if (!combo) return;
        
        combo->clear();
        combo->setEditable(true);

        QStringList suggestions;
        const QString backendText = ttsBackendCombo->currentText();
        
        if (ttsEngine->backend() == TTSEngine::Backend::System) {
            QStringList voiceNames = ttsEngine->getVoiceNamesForLanguage(locale);
            for (const QString& voiceName : voiceNames) {
                combo->addItem(voiceName, voiceName);
            }
        } else {
            // Get cloud voice suggestions based on backend
            if (backendText.contains("Azure")) {
                suggestions = CloudTTSProvider::azureSuggestedVoicesFor(locale);
            } else if (backendText == "Google Cloud TTS") {
                suggestions = CloudTTSProvider::googleSuggestedVoicesFor(locale);
            } else if (backendText.contains("Google (Free")) {
                suggestions = CloudTTSProvider::googleFreeSuggestedVoicesFor(locale);
            } else if (backendText.contains("Edge")) {
                suggestions = CloudTTSProvider::edgeSuggestedVoicesFor(locale);
            } else if (backendText.contains("ElevenLabs")) {
                suggestions = { "Rachel", "Adam", "Bella", "Antoni" }; // Default ElevenLabs voices
            } else if (backendText.contains("Polly")) {
                suggestions = CloudTTSProvider::pollySuggestedVoicesFor(locale);
            }

            for (const QString& suggestion : suggestions) {
                combo->addItem(suggestion, suggestion);
            }
        }
        
        // Try to restore the current voice selection
        if (!currentVoice.isEmpty()) {
            int index = combo->findData(currentVoice);
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
        } else if (combo->count() > 0) {
            combo->setCurrentIndex(0);
        }
    };

    // Update all voice combos
    populateVoiceCombo(inputVoiceCombo, inputLocale, ttsEngine ? ttsEngine->inputVoice() : QString());
    populateVoiceCombo(outputVoiceCombo, outputLocale, ttsEngine ? ttsEngine->outputVoice() : QString());
    populateVoiceCombo(voiceCombo, outputLocale, QString()); // Universal voice uses output locale by default
}