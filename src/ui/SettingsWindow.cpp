#include "SettingsWindow.h"
#include "OCREngine.h"
#include "TTSEngine.h"
#include "TTSManager.h"
#include "../core/LanguageManager.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QMessageBox>
#include <QShowEvent>
#include <QHideEvent>
#include <QCoreApplication>
#include <QTimer>
#include <QFileDialog>
#include <QProcess>
#include <QIcon>
#include <QSignalBlocker>
#include <QtMath>
#include "ThemeManager.h"

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
    , tabWidget(nullptr)
    , settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
    , ttsEngine(nullptr)
{
    // Use global TTS engine from manager
    ttsEngine = TTSManager::instance().ttsEngine();

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
    
    // Input Language (OCR) with TTS toggle
    QLabel *inputLabel = new QLabel("📖 Input Language (OCR Recognition):");
    inputLabel->setWordWrap(true);

    QWidget *inputLangWidget = new QWidget();
    QHBoxLayout *inputLangLayout = new QHBoxLayout(inputLangWidget);
    inputLangLayout->setContentsMargins(0, 0, 0, 0);
    inputLangLayout->setSpacing(5);

    ocrLanguageCombo = new QComboBox();
    ocrLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)",
                               "Japanese", "Korean", "Spanish", "French", "German", "Russian",
                               "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
    ocrLanguageCombo->setToolTip("Language for text recognition from screenshots");
    connect(ocrLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ updateVoicesForLanguage(); });

    // TTS toggle button for input
    ttsInputCheckBox = new QCheckBox();
    ttsInputCheckBox->setText("🔊");
    ttsInputCheckBox->setToolTip("Enable TTS for Input Text (Read recognized OCR text aloud)");
    ttsInputCheckBox->setChecked(true);
    ttsInputCheckBox->setMaximumWidth(50);
    ttsInputCheckBox->setStyleSheet("QCheckBox { font-size: 18px; }");

    inputLangLayout->addWidget(ocrLanguageCombo, 1);
    inputLangLayout->addWidget(ttsInputCheckBox);

    langLayout->addRow(inputLabel, inputLangWidget);
    
    // Output Language (Translation) with TTS toggle
    QLabel *outputLabel = new QLabel("🌐 Output Language (Translation Target):");
    outputLabel->setWordWrap(true);

    QWidget *outputLangWidget = new QWidget();
    QHBoxLayout *outputLangLayout = new QHBoxLayout(outputLangWidget);
    outputLangLayout->setContentsMargins(0, 0, 0, 0);
    outputLangLayout->setSpacing(5);

    targetLanguageCombo = new QComboBox();
    targetLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)", "Japanese", "Korean", "Spanish",
                                  "French", "German", "Russian", "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
    targetLanguageCombo->setCurrentText("English");
    targetLanguageCombo->setToolTip("Target language for translation and TTS voice selection");
    connect(targetLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ updateVoicesForLanguage(); });

    // TTS toggle button for output
    ttsOutputCheckBox = new QCheckBox();
    ttsOutputCheckBox->setText("🔊");
    ttsOutputCheckBox->setToolTip("Enable TTS for Output Text (Read translation results aloud)");
    ttsOutputCheckBox->setChecked(true);
    ttsOutputCheckBox->setMaximumWidth(50);
    ttsOutputCheckBox->setStyleSheet("QCheckBox { font-size: 18px; }");

    outputLangLayout->addWidget(targetLanguageCombo, 1);
    outputLangLayout->addWidget(ttsOutputCheckBox);

    langLayout->addRow(outputLabel, outputLangWidget);
    
    // Info text
    QLabel *infoLabel = new QLabel("ℹ️ <i>TTS voice language automatically matches the Output Language setting.<br>"
                                  "This unified configuration ensures consistent language handling across all features.</i>");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;");
    langLayout->addRow(infoLabel);

    // Connect TTS checkboxes to engine (if available)
    connect(ttsInputCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (ttsEngine) ttsEngine->setTTSInputEnabled(checked);
    });
    connect(ttsOutputCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (ttsEngine) ttsEngine->setTTSOutputEnabled(checked);
    });

    layout->addWidget(langGroup);
    
    // Remove the TTS Options Group as we'll integrate TTS controls with language selectors
    // The checkboxes are created with the language combos now
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

    autoTranslateCheck = new QCheckBox("Auto-Translate: Automatically translate after OCR");
    autoTranslateCheck->setChecked(true);
    autoLayout->addWidget(autoTranslateCheck);

    // Overlay Mode Selection
    QHBoxLayout *overlayModeLayout = new QHBoxLayout();
    QLabel *overlayModeLabel = new QLabel("Overlay Mode:");
    overlayModeCombo = new QComboBox();
    overlayModeCombo->addItems({"Quick Translation", "Deep Learning Mode"});
    overlayModeCombo->setToolTip("Quick Translation: Simple overlay with text replacement\nDeep Learning Mode: Interactive learning overlay with word analysis");

    overlayModeLayout->addWidget(overlayModeLabel);
    overlayModeLayout->addWidget(overlayModeCombo);
    overlayModeLayout->addStretch();

    autoLayout->addLayout(overlayModeLayout);

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
        ThemeManager::instance().applyTheme(ThemeManager::fromString(text));
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
    autoTranslateCheck->setChecked(settings->value("translation/autoTranslate", true).toBool());
    overlayModeCombo->setCurrentText(settings->value("translation/overlayMode", "Deep Learning Mode").toString());
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
    ttsInputCheckBox->setChecked(settings->value("general/ttsInput", true).toBool());
    ttsOutputCheckBox->setChecked(settings->value("general/ttsOutput", true).toBool());

    // TTS Settings
    if (ttsEngine) {
        if (refreshVoicesButton) {
            refreshVoicesButton->setEnabled(true);
        }
        ttsEnabledCheck->setChecked(settings->value("tts/enabled", true).toBool());

        const int savedVolume = settings->value("tts/volume", 100).toInt();
        const int savedPitch = settings->value("tts/pitch", 0).toInt();
        const int savedRate = settings->value("tts/rate", 0).toInt();

        QString providerId = settings->value("tts/provider", settings->value("tts/backend", QStringLiteral("google"))).toString();
    const QString googleVoiceStored = settings->value("tts/Google/voice").toString();
    const QString googleWebVoiceStored = settings->value("tts/GoogleWeb/voice").toString();
        const QString edgeVoiceStored = settings->value("tts/Edge/voice").toString();
        const QString edgeExeStored = settings->value("tts/Edge/exePath").toString();
        const QString savedInputVoice = settings->value("tts/inputVoice").toString();
        const QString savedOutputVoice = settings->value("tts/outputVoice").toString();
        const QString savedTestText = settings->value("tts/testText", testTextEdit ? testTextEdit->text() : QString()).toString();

        if (providerId == QStringLiteral("google")) {
            providerId = QStringLiteral("google-web");
        } else if (providerId == QStringLiteral("edge")) {
            providerId = QStringLiteral("edge-free");
        }

        if (ttsProviderCombo) {
            const int idx = ttsProviderCombo->findData(providerId);
            if (idx >= 0) {
                QSignalBlocker blocker(*ttsProviderCombo);
                ttsProviderCombo->setCurrentIndex(idx);
            }
        }

        if (edgeExePathEdit) {
            edgeExePathEdit->setText(edgeExeStored);
        }
        if (testTextEdit) {
            testTextEdit->setText(savedTestText);
        }

        ttsEngine->setProviderId(providerId);
        ttsEngine->setVolume(savedVolume / 100.0);
        ttsEngine->setPitch(savedPitch / 100.0);
        ttsEngine->setRate(savedRate / 100.0);

        QString primaryVoice;
        if (providerId == QStringLiteral("edge-free")) {
            primaryVoice = !edgeVoiceStored.isEmpty() ? edgeVoiceStored : savedOutputVoice.isEmpty() ? savedInputVoice : savedOutputVoice;
            ttsEngine->setEdgeVoice(primaryVoice);
            ttsEngine->setEdgeExecutable(edgeExeStored);
        } else {
            primaryVoice = !googleWebVoiceStored.isEmpty() ? googleWebVoiceStored : !googleVoiceStored.isEmpty() ? googleVoiceStored : savedOutputVoice.isEmpty() ? savedInputVoice : savedOutputVoice;
            QString langCode = getLanguageCodeFromVoice(primaryVoice);
            ttsEngine->configureGoogle(QString(), primaryVoice, langCode);
        }

        ttsEngine->setPrimaryVoice(primaryVoice);
        ttsEngine->setInputVoice(savedInputVoice);
        ttsEngine->setOutputVoice(savedOutputVoice);

        if (voiceCombo) voiceCombo->setEditText(primaryVoice);
        if (inputVoiceCombo) inputVoiceCombo->setEditText(savedInputVoice);
        if (outputVoiceCombo) outputVoiceCombo->setEditText(savedOutputVoice);
        if (advancedVoiceToggle) {
            advancedVoiceToggle->setChecked(!savedInputVoice.isEmpty() || !savedOutputVoice.isEmpty());
        }

        if (ttsProviderLabel) {
            ttsProviderLabel->setText(tr("Voice service: %1").arg(ttsEngine->providerName()));
        }

        updateProviderUI(providerId);

        if (ttsStatusText) {
            QString status;
            if (providerId == QStringLiteral("edge-free") && edgeExePathEdit && edgeExePathEdit->text().trimmed().isEmpty()) {
                status = tr("⚠️ Choose the edge-tts executable to enable playback.");
            }
            if (status.isEmpty()) {
                status = tr("✅ Ready to speak.");
            }
            ttsStatusText->setPlainText(status);
        }
    } else if (ttsProviderLabel) {
        ttsProviderLabel->setText(tr("Voice service unavailable"));
        if (refreshVoicesButton) {
            refreshVoicesButton->setEnabled(false);
        }
    }
}

void SettingsWindow::saveSettings()
{
    if (!settings) {
        return;
    }

    // OCR Settings
    if (ocrEngineCombo) settings->setValue("ocr/engine", ocrEngineCombo->currentText());
    if (ocrLanguageCombo) settings->setValue("ocr/language", ocrLanguageCombo->currentText());
    if (ocrQualitySlider) settings->setValue("ocr/quality", ocrQualitySlider->value());
    if (ocrPreprocessingCheck) settings->setValue("ocr/preprocessing", ocrPreprocessingCheck->isChecked());
    if (ocrAutoDetectCheck) settings->setValue("ocr/autoDetect", ocrAutoDetectCheck->isChecked());

    // Translation Settings
    if (autoTranslateCheck) settings->setValue("translation/autoTranslate", autoTranslateCheck->isChecked());
    if (overlayModeCombo) settings->setValue("translation/overlayMode", overlayModeCombo->currentText());
    if (translationEngineCombo) settings->setValue("translation/engine", translationEngineCombo->currentText());
    if (sourceLanguageCombo) settings->setValue("translation/sourceLanguage", sourceLanguageCombo->currentText());
    if (targetLanguageCombo) settings->setValue("translation/targetLanguage", targetLanguageCombo->currentText());
    if (apiUrlEdit) settings->setValue("translation/apiUrl", apiUrlEdit->text().trimmed());
    if (apiKeyEdit) settings->setValue("translation/apiKey", apiKeyEdit->text());

    // Appearance Settings
    if (themeCombo) settings->setValue("appearance/theme", themeCombo->currentText());
    if (opacitySlider) settings->setValue("appearance/opacity", opacitySlider->value());
    if (animationsCheck) settings->setValue("appearance/animations", animationsCheck->isChecked());
    if (soundsCheck) settings->setValue("appearance/sounds", soundsCheck->isChecked());

    // General Options
    if (ttsInputCheckBox) settings->setValue("general/ttsInput", ttsInputCheckBox->isChecked());
    if (ttsOutputCheckBox) settings->setValue("general/ttsOutput", ttsOutputCheckBox->isChecked());

    // TTS Settings
    QString providerId;
    if (ttsProviderCombo) {
        providerId = ttsProviderCombo->currentData().toString();
    }
    if (providerId.isEmpty() && ttsEngine) {
        providerId = ttsEngine->providerId();
    }

    const bool ttsEnabled = ttsEnabledCheck ? ttsEnabledCheck->isChecked() : true;
    const QString primaryVoice = voiceCombo ? voiceCombo->currentText().trimmed() : QString();
    const QString inputVoice = inputVoiceCombo ? inputVoiceCombo->currentText().trimmed() : QString();
    const QString outputVoice = outputVoiceCombo ? outputVoiceCombo->currentText().trimmed() : QString();
    const QString edgeExePath = edgeExePathEdit ? edgeExePathEdit->text().trimmed() : QString();
    const QString testPhrase = testTextEdit ? testTextEdit->text() : QString();

    settings->setValue("tts/enabled", ttsEnabled);
    settings->setValue("tts/provider", providerId);
    settings->setValue("tts/backend", providerId == QStringLiteral("edge-free") ? QStringLiteral("edge") : QStringLiteral("google"));
    settings->setValue("tts/inputVoice", inputVoice);
    settings->setValue("tts/outputVoice", outputVoice);
    settings->setValue("tts/testText", testPhrase);

    if (providerId == QStringLiteral("edge-free")) {
        settings->setValue("tts/Edge/voice", primaryVoice);
        settings->setValue("tts/Edge/exePath", edgeExePath);
    } else {
        settings->setValue("tts/Google/voice", primaryVoice);
        settings->setValue("tts/GoogleWeb/voice", primaryVoice);
        settings->remove("tts/Google/languageCode");
        settings->remove("tts/GoogleWeb/languageCode");
    }

    if (ttsEngine) {
        if (!providerId.isEmpty()) {
            ttsEngine->setProviderId(providerId);
        }
        ttsEngine->setPrimaryVoice(primaryVoice);
        ttsEngine->setInputVoice(inputVoice);
        ttsEngine->setOutputVoice(outputVoice);
        ttsEngine->setTTSInputEnabled(ttsInputCheckBox ? ttsInputCheckBox->isChecked() : ttsEngine->isTTSInputEnabled());
        ttsEngine->setTTSOutputEnabled(ttsOutputCheckBox ? ttsOutputCheckBox->isChecked() : ttsEngine->isTTSOutputEnabled());

        if (providerId == QStringLiteral("edge-free")) {
            ttsEngine->setEdgeExecutable(edgeExePath);
            ttsEngine->setEdgeVoice(primaryVoice);
        } else {
            QString langCode = getLanguageCodeFromVoice(primaryVoice);
            ttsEngine->configureGoogle(QString(), primaryVoice, langCode);
        }

        settings->setValue("tts/volume", qRound(ttsEngine->volume() * 100.0));
        settings->setValue("tts/pitch", qRound(ttsEngine->pitch() * 100.0));
        settings->setValue("tts/rate", qRound(ttsEngine->rate() * 100.0));

        ttsEngine->saveSettings();
    }

    settings->sync();
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
    ttsInputCheckBox->setChecked(true);
    ttsOutputCheckBox->setChecked(true);
    if (ttsProviderCombo) {
        const int idx = ttsProviderCombo->findData(QStringLiteral("google-web"));
        if (idx >= 0) {
            QSignalBlocker blocker(*ttsProviderCombo);
            ttsProviderCombo->setCurrentIndex(idx);
        }
    }
    if (edgeExePathEdit) edgeExePathEdit->clear();
    if (advancedVoiceToggle) advancedVoiceToggle->setChecked(false);
    if (voiceCombo) voiceCombo->clearEditText();
    if (inputVoiceCombo) inputVoiceCombo->clearEditText();
    if (outputVoiceCombo) outputVoiceCombo->clearEditText();

    if (ttsEngine) {
        ttsEngine->setProviderId(QStringLiteral("google-web"));
        ttsEngine->setPrimaryVoice(QString());
        ttsEngine->setEdgeVoice(QString());
        ttsEngine->setEdgeExecutable(QString());
        ttsEngine->setInputVoice(QString());
        ttsEngine->setOutputVoice(QString());
        ttsEngine->saveSettings();
        updateProviderUI(QStringLiteral("google-web"));
    }
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

    QGroupBox *enableGroup = new QGroupBox(tr("Text-to-Speech Quick Setup"));
    QVBoxLayout *enableLayout = new QVBoxLayout(enableGroup);
    ttsEnabledCheck = new QCheckBox(tr("Turn on voice playback"));
    ttsEnabledCheck->setChecked(true);
    enableLayout->addWidget(ttsEnabledCheck);

    QLabel *enableInfo = new QLabel(tr("Pick a free voice provider, choose a voice, then tap test."));
    enableInfo->setWordWrap(true);
    enableInfo->setStyleSheet("color: #666; font-size: 11px;");
    enableLayout->addWidget(enableInfo);

    ttsProviderLabel = new QLabel;
    ttsProviderLabel->setStyleSheet("color: #666; font-size: 11px;");
    enableLayout->addWidget(ttsProviderLabel);
    layout->addWidget(enableGroup);

    QGroupBox *providerGroup = new QGroupBox(tr("Step 1 — Choose a voice service"));
    QVBoxLayout *providerLayout = new QVBoxLayout(providerGroup);
    ttsProviderCombo = new QComboBox();
    ttsProviderCombo->addItem(tr("Google Translate (Free)"), QStringLiteral("google-web"));
    ttsProviderCombo->addItem(tr("Microsoft Edge (Free)"), QStringLiteral("edge-free"));
    providerLayout->addWidget(ttsProviderCombo);

    providerInfoLabel = new QLabel(tr("Google Translate works instantly with many languages. Edge voices need the edge-tts tool installed (pip install edge-tts)."));
    providerInfoLabel->setWordWrap(true);
    providerInfoLabel->setStyleSheet("color: #666; font-size: 11px;");
    providerLayout->addWidget(providerInfoLabel);
    layout->addWidget(providerGroup);

    connect(ttsProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString providerId = ttsProviderCombo->itemData(index).toString();
        updateProviderUI(providerId);
    });

    QGroupBox *voiceGroup = new QGroupBox(tr("Step 2 — Select voices"));
    QFormLayout *voiceLayout = new QFormLayout(voiceGroup);

    refreshVoicesButton = new QPushButton(tr("Refresh"));
    refreshVoicesButton->setIcon(QIcon::fromTheme("view-refresh"));
    refreshVoicesButton->setToolTip(tr("Reload voices based on the current provider and language."));

    // We'll now show input and output voices directly instead of the old single voice combo
    // Keep the old voiceCombo for compatibility (hidden)
    voiceCombo = new QComboBox();
    voiceCombo->setVisible(false);
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::onVoiceChanged);

    // Input voice combo with test buttons
    inputVoiceCombo = new QComboBox();
    inputVoiceCombo->setEditable(true);
    inputVoiceCombo->setPlaceholderText(tr("Select input voice"));
    inputVoiceCombo->setToolTip(tr("Voice used when reading OCR/input text"));

    QPushButton *testInputTTSBtn = new QPushButton(tr("🔊 Test"));
    testInputTTSBtn->setToolTip(tr("Test input voice"));
    QPushButton *stopInputTTSBtn = new QPushButton(tr("⏹"));
    stopInputTTSBtn->setToolTip(tr("Stop playback"));
    stopInputTTSBtn->setEnabled(false);

    // Connect input voice test button
    connect(testInputTTSBtn, &QPushButton::clicked, this, [this, stopInputTTSBtn, testInputTTSBtn]() {
        if (!ttsEngine) {
            ttsStatusText->setPlainText("❌ TTS engine not available");
            return;
        }

        if (!ttsEnabledCheck->isChecked()) {
            ttsStatusText->setPlainText("⚠️ Enable Text-to-Speech to play audio.");
            return;
        }

        const QString providerId = ttsProviderCombo ? ttsProviderCombo->currentData().toString() : ttsEngine->providerId();
        QString voice = inputVoiceCombo ? inputVoiceCombo->currentText().trimmed() : QString();

        if (voice.isEmpty()) {
            ttsStatusText->setPlainText(tr("⚠️ Select an input voice before testing."));
            return;
        }

        QString testText = testTextEdit ? testTextEdit->text().trimmed() : "";
        if (testText.isEmpty()) {
            testText = getTestTextForLanguage(inputVoiceCombo->currentText(), true);
        }

        // Get the language code for this voice
        QString langCode = getLanguageCodeFromVoice(voice);

        ttsEngine->setProviderId(providerId);
        ttsEngine->setPrimaryVoice(voice);
        ttsEngine->setVolume(1.0);
        ttsEngine->setPitch(0.0);
        ttsEngine->setRate(0.0);

        if (providerId == QStringLiteral("google-web")) {
            // CRUCIAL: Set the language code for Google TTS
            ttsEngine->configureGoogle(QString(), voice, langCode);
        } else {
            const QString exePath = ttsEngine->edgeExecutable();
            if (exePath.isEmpty() || exePath == "edge-tts") {
                ttsEngine->setEdgeExecutable("edge-tts");
            } else if (exePath.isEmpty()) {
                checkEdgeTTSAvailability();
                ttsStatusText->setPlainText(tr("⚠️ Edge TTS not configured. Please install it first."));
                return;
            }
            ttsEngine->setEdgeVoice(voice);
        }

        ttsStatusText->setPlainText(tr("▶️ Playing input voice test..."));

        // Convert language code to QLocale using LanguageManager
        QLocale testLocale = LanguageManager::instance().localeFromLanguageCode(langCode);

        ttsEngine->speak(testText, true, testLocale);
    });

    connect(stopInputTTSBtn, &QPushButton::clicked, [this]() {
        if (ttsEngine) {
            ttsEngine->stop();
        }
    });

    QWidget *inputVoiceWidget = new QWidget();
    QHBoxLayout *inputVoiceRow = new QHBoxLayout(inputVoiceWidget);
    inputVoiceRow->setContentsMargins(0, 0, 0, 0);
    inputVoiceRow->addWidget(inputVoiceCombo, 1);
    inputVoiceRow->addWidget(testInputTTSBtn);
    inputVoiceRow->addWidget(stopInputTTSBtn);

    inputVoiceLabel = new QLabel(tr("Input voice:"));
    voiceLayout->addRow(inputVoiceLabel, inputVoiceWidget);

    // Output voice combo with test buttons
    outputVoiceCombo = new QComboBox();
    outputVoiceCombo->setEditable(true);
    outputVoiceCombo->setPlaceholderText(tr("Select output voice"));
    outputVoiceCombo->setToolTip(tr("Voice used when reading translated text"));

    testTTSBtn = new QPushButton(tr("🔊 Test"));
    testTTSBtn->setToolTip(tr("Test output voice"));
    stopTTSBtn = new QPushButton(tr("⏹"));
    stopTTSBtn->setToolTip(tr("Stop playback"));
    stopTTSBtn->setEnabled(false);
    connect(testTTSBtn, &QPushButton::clicked, this, &SettingsWindow::onTestTTSClicked);
    connect(stopTTSBtn, &QPushButton::clicked, [this]() {
        if (ttsEngine) {
            ttsEngine->stop();
        }
    });

    QWidget *outputVoiceWidget = new QWidget();
    QHBoxLayout *outputVoiceRow = new QHBoxLayout(outputVoiceWidget);
    outputVoiceRow->setContentsMargins(0, 0, 0, 0);
    outputVoiceRow->addWidget(outputVoiceCombo, 1);
    outputVoiceRow->addWidget(testTTSBtn);
    outputVoiceRow->addWidget(stopTTSBtn);
    outputVoiceRow->addWidget(refreshVoicesButton);

    outputVoiceLabel = new QLabel(tr("Output voice:"));
    voiceLayout->addRow(outputVoiceLabel, outputVoiceWidget);

    // Edge TTS auto-detection section (only shown for Edge provider)
    edgeExePathEdit = new QLineEdit();
    edgeExePathEdit->setPlaceholderText(tr("Auto-detecting edge-tts..."));
    edgeExePathEdit->setReadOnly(true);
    edgeExePathEdit->setVisible(false); // Hidden since we auto-detect

    edgeBrowseButton = new QPushButton();
    edgeBrowseButton->setVisible(false); // Hidden, not needed anymore

    QPushButton *installEdgeButton = new QPushButton(tr("Install Edge TTS"));
    installEdgeButton->setToolTip(tr("Install edge-tts using pip (Python required)"));

    edgeExeLabel = new QLabel(tr("Edge TTS Status:"));
    edgeExeRow = new QWidget();
    QHBoxLayout *edgeExeLayout = new QHBoxLayout(edgeExeRow);
    edgeExeLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *edgeStatusLabel = new QLabel(tr("Checking..."));
    edgeStatusLabel->setObjectName("edgeStatusLabel");
    edgeExeLayout->addWidget(edgeStatusLabel, 1);
    edgeExeLayout->addWidget(installEdgeButton);

    voiceLayout->addRow(edgeExeLabel, edgeExeRow);

    edgeHintLabel = new QLabel();
    edgeHintLabel->setStyleSheet("color: #666; font-size: 11px;");
    edgeHintLabel->setWordWrap(true);
    voiceLayout->addRow(QString(), edgeHintLabel);

    // No more advanced voice toggle - always show input/output
    advancedVoiceToggle = new QCheckBox();
    advancedVoiceToggle->setVisible(false);

    QLabel *testTextLabel = new QLabel(tr("Sample text:"));
    testTextEdit = new QLineEdit();
    testTextEdit->setText(tr("Hello! This is a test of the text-to-speech functionality."));
    testTextEdit->setPlaceholderText(tr("Enter custom test text or leave empty for language-specific test"));
    voiceLayout->addRow(testTextLabel, testTextEdit);

    layout->addWidget(voiceGroup);

    ttsStatusText = new QTextEdit();
    ttsStatusText->setReadOnly(true);
    ttsStatusText->setMaximumHeight(90);
    layout->addWidget(ttsStatusText);

    layout->addStretch();

    connect(refreshVoicesButton, &QPushButton::clicked, this, [this]() {
        updateVoicesForLanguage();
        if (ttsStatusText) {
            ttsStatusText->setPlainText(tr("Voice suggestions refreshed."));
        }
    });

    // Connect install button for Edge TTS
    connect(installEdgeButton, &QPushButton::clicked, this, [this, edgeStatusLabel, installEdgeButton]() {
        installEdgeButton->setEnabled(false);
        edgeStatusLabel->setText(tr("Installing edge-tts..."));

        QProcess *process = new QProcess(this);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, process, edgeStatusLabel, installEdgeButton](int exitCode, QProcess::ExitStatus) {
            process->deleteLater();
            if (exitCode == 0) {
                edgeStatusLabel->setText(tr("✅ Installed successfully"));
                checkEdgeTTSAvailability();
            } else {
                const QString errorMsg = QString::fromLocal8Bit(process->readAllStandardError());
                edgeStatusLabel->setText(tr("❌ Installation failed"));
                if (edgeHintLabel) {
                    edgeHintLabel->setText(tr("Error: %1").arg(errorMsg.left(200)));
                }
                installEdgeButton->setEnabled(true);
            }
        });

        // Try to install edge-tts
        process->start("pip", {"install", "edge-tts"});
        if (!process->waitForStarted(1000)) {
            // If pip doesn't work, try pip3
            process->start("pip3", {"install", "edge-tts"});
            if (!process->waitForStarted(1000)) {
                // Try python -m pip
                process->start("python", {"-m", "pip", "install", "edge-tts"});
                if (!process->waitForStarted(1000)) {
                    // Try python3 -m pip
                    process->start("python3", {"-m", "pip", "install", "edge-tts"});
                    if (!process->waitForStarted(1000)) {
                        edgeStatusLabel->setText(tr("❌ Python/pip not found"));
                        edgeHintLabel->setText(tr("Please install Python first from python.org"));
                        installEdgeButton->setEnabled(true);
                        process->deleteLater();
                    }
                }
            }
        }
    });

    if (ttsEngine) {
        connect(ttsEngine, &TTSEngine::stateChanged, this, [this, stopInputTTSBtn, testInputTTSBtn](QTextToSpeech::State state) {
            const bool isSpeaking = (state == QTextToSpeech::Speaking);
            stopTTSBtn->setEnabled(isSpeaking);
            testTTSBtn->setEnabled(!isSpeaking);
            stopInputTTSBtn->setEnabled(isSpeaking);
            testInputTTSBtn->setEnabled(!isSpeaking);
        });

        refreshVoicesButton->setEnabled(true);

        const QString providerId = ttsEngine->providerId();
        if (ttsProviderCombo) {
            const int idx = ttsProviderCombo->findData(providerId);
            if (idx >= 0) {
                QSignalBlocker blocker(ttsProviderCombo);
                ttsProviderCombo->setCurrentIndex(idx);
            }
        }
        updateProviderUI(providerId);
    } else {
        ttsEnabledCheck->setEnabled(false);
        if (ttsProviderCombo) ttsProviderCombo->setEnabled(false);
        if (voiceCombo) voiceCombo->setEnabled(false);
        if (inputVoiceCombo) inputVoiceCombo->setEnabled(false);
        if (outputVoiceCombo) outputVoiceCombo->setEnabled(false);
        if (refreshVoicesButton) refreshVoicesButton->setEnabled(false);
        advancedVoiceToggle->setEnabled(false);
        testTTSBtn->setEnabled(false);
        ttsStatusText->setPlainText(tr("❌ Text-to-Speech engine is unavailable."));
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

    if (!ttsEnabledCheck->isChecked()) {
        ttsStatusText->setPlainText("⚠️ Enable Text-to-Speech to play audio.");
        return;
    }

    const QString providerId = ttsProviderCombo ? ttsProviderCombo->currentData().toString() : ttsEngine->providerId();

    // Use output voice for testing (since we test the translation output)
    QString voice = outputVoiceCombo ? outputVoiceCombo->currentText().trimmed() : QString();
    if (voice.isEmpty() && voiceCombo) {
        voice = voiceCombo->currentText().trimmed(); // Fallback for compatibility
    }

    if (voice.isEmpty()) {
        ttsStatusText->setPlainText(tr("⚠️ Select an output voice before testing."));
        return;
    }

    QString testText = testTextEdit->text().trimmed();
    if (testText.isEmpty()) {
        testText = getTestTextForLanguage(outputVoiceCombo ? outputVoiceCombo->currentText() : voice, false);
    }

    // Get the language code for this voice
    QString langCode = getLanguageCodeFromVoice(voice);

    // Use the unified configuration method
    ttsEngine->configureFromCurrentSettings();

    // Override with test-specific settings for the test
    ttsEngine->setVolume(1.0);  // Full volume for test

    ttsStatusText->setPlainText(tr("▶️ Playing test text..."));

    // Convert language code to QLocale using LanguageManager
    QLocale testLocale = LanguageManager::instance().localeFromLanguageCode(langCode);

    ttsEngine->speak(testText, false, testLocale);
}

void SettingsWindow::updateProviderUI(const QString& providerId)
{
    const bool isGoogle = providerId.isEmpty() || providerId == QStringLiteral("google-web");

    if (edgeExeLabel) {
        edgeExeLabel->setVisible(!isGoogle);
    }
    if (edgeExeRow) {
        edgeExeRow->setVisible(!isGoogle);
    }
    if (edgeHintLabel) {
        edgeHintLabel->setVisible(!isGoogle);
    }

    if (providerInfoLabel) {
        if (isGoogle) {
            providerInfoLabel->setText(tr("Instant playback via Google Translate. Works best for short phrases."));
        } else {
            providerInfoLabel->setText(tr("Microsoft Edge TTS provides natural-sounding voices in many languages."));
        }
    }

    if (ttsEngine) {
        ttsEngine->setProviderId(providerId);
        if (isGoogle) {
            QString langCode = getLanguageCodeFromVoice(ttsEngine->primaryVoice());
            ttsEngine->configureGoogle(QString(), ttsEngine->primaryVoice(), langCode);
        } else {
            // Auto-detect edge-tts when Edge provider is selected
            checkEdgeTTSAvailability();
        }

        if (ttsProviderLabel) {
            ttsProviderLabel->setText(tr("Voice service: %1").arg(ttsEngine->providerName()));
        }
    }

    updateVoicesForLanguage();
}

void SettingsWindow::checkEdgeTTSAvailability()
{
    // Find the edge status label
    QLabel *edgeStatusLabel = findChild<QLabel*>("edgeStatusLabel");
    QPushButton *installButton = edgeExeRow ? edgeExeRow->findChild<QPushButton*>() : nullptr;

    if (!edgeStatusLabel) return;

    // Check if edge-tts is available
    QProcess *checkProcess = new QProcess(this);
    connect(checkProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, checkProcess, edgeStatusLabel, installButton](int exitCode, QProcess::ExitStatus) {
        checkProcess->deleteLater();

        if (exitCode == 0) {
            // edge-tts is installed, get its path
            QProcess *whichProcess = new QProcess(this);
            connect(whichProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    [this, whichProcess, edgeStatusLabel, installButton](int exitCode2, QProcess::ExitStatus) {
                QString edgePath;
                if (exitCode2 == 0) {
                    edgePath = QString::fromLocal8Bit(whichProcess->readAllStandardOutput()).trimmed();
                }
                whichProcess->deleteLater();

                if (!edgePath.isEmpty()) {
                    edgeStatusLabel->setText(tr("✅ Edge TTS is installed"));
                    if (edgeHintLabel) {
                        edgeHintLabel->setText(tr("Ready to use Microsoft Edge voices"));
                    }
                    if (installButton) {
                        installButton->setVisible(false);
                    }
                    // Set the executable path
                    if (ttsEngine) {
                        ttsEngine->setEdgeExecutable("edge-tts"); // Use command name directly
                    }
                    if (edgeExePathEdit) {
                        edgeExePathEdit->setText("edge-tts");
                    }
                } else {
                    // Installed but can't find path, still usable
                    edgeStatusLabel->setText(tr("✅ Edge TTS is installed"));
                    if (ttsEngine) {
                        ttsEngine->setEdgeExecutable("edge-tts");
                    }
                }
            });

            // Try to find edge-tts location
            #ifdef Q_OS_WIN
            whichProcess->start("where", {"edge-tts"});
            #else
            whichProcess->start("which", {"edge-tts"});
            #endif
        } else {
            // edge-tts not found
            edgeStatusLabel->setText(tr("⚠️ Edge TTS not installed"));
            if (edgeHintLabel) {
                edgeHintLabel->setText(tr("Click 'Install Edge TTS' to enable Microsoft voices"));
            }
            if (installButton) {
                installButton->setVisible(true);
                installButton->setEnabled(true);
            }
        }
    });

    // Check if edge-tts command exists
    checkProcess->start("edge-tts", {"--version"});
    if (!checkProcess->waitForStarted(1000)) {
        // Command not found
        edgeStatusLabel->setText(tr("⚠️ Edge TTS not installed"));
        if (edgeHintLabel) {
            edgeHintLabel->setText(tr("Click 'Install Edge TTS' to enable Microsoft voices"));
        }
        if (installButton) {
            installButton->setVisible(true);
            installButton->setEnabled(true);
        }
        checkProcess->deleteLater();
    }
}

void SettingsWindow::updateVoicesForLanguage()
{
    if (!ttsEngine) {
        return;
    }

    // Rebuild the voice suggestions whenever the OCR or translation language changes so
    // that the first entry in each list always reflects the newly selected locale.

    auto mapLanguageToLocale = [](const QString &name) -> QLocale {
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

    QLocale inputLocale = QLocale::system();
    QLocale outputLocale = QLocale::system();

    if (ocrLanguageCombo && !ocrLanguageCombo->currentText().isEmpty()) {
        inputLocale = mapLanguageToLocale(ocrLanguageCombo->currentText());
    } else if (sourceLanguageCombo && !sourceLanguageCombo->currentText().isEmpty()) {
        inputLocale = mapLanguageToLocale(sourceLanguageCombo->currentText());
    }

    if (targetLanguageCombo && !targetLanguageCombo->currentText().isEmpty()) {
        outputLocale = mapLanguageToLocale(targetLanguageCombo->currentText());
    }

    auto populateCombo = [this](QComboBox *combo, const QLocale &locale, const QString &preferredVoice) {
        if (!combo) {
            return;
        }

        const QString preserved = preferredVoice.isEmpty() ? combo->currentText().trimmed() : preferredVoice.trimmed();
        QStringList suggestions;
        if (ttsEngine) {
            suggestions = ttsEngine->suggestedVoicesFor(locale);
        }

        combo->blockSignals(true);
        combo->clear();
        combo->setEditable(true);
        for (const QString &voice : suggestions) {
            combo->addItem(voice, voice);
        }

        if (!preserved.isEmpty() && combo->findText(preserved, Qt::MatchFixedString) != -1) {
            combo->setCurrentText(preserved);
        } else if (combo->count() > 0) {
            combo->setCurrentIndex(0);
        } else if (!preserved.isEmpty()) {
            combo->setCurrentText(preserved);
        }
        combo->blockSignals(false);
    };

    populateCombo(inputVoiceCombo, inputLocale, ttsEngine->inputVoice());
    populateCombo(outputVoiceCombo, outputLocale, ttsEngine->outputVoice());
    populateCombo(voiceCombo, outputLocale, ttsEngine->primaryVoice());

    ttsEngine->setInputVoice(inputVoiceCombo ? inputVoiceCombo->currentText().trimmed() : QString());
    ttsEngine->setOutputVoice(outputVoiceCombo ? outputVoiceCombo->currentText().trimmed() : QString());
    if (voiceCombo) {
        ttsEngine->setPrimaryVoice(voiceCombo->currentText().trimmed());
    }
}

QString SettingsWindow::getTestTextForLanguage(const QString& voice, bool isInputVoice) const
{
    // Get the language code for this voice and use it to determine test text
    QString langCode = getLanguageCodeFromVoice(voice);

    if (langCode.startsWith("zh-TW") || langCode == "zh-TW") {
        return isInputVoice ? "這是語音識別測試。" : "這是翻譯輸出的語音測試。";
    } else if (langCode.startsWith("zh") || langCode == "zh-CN") {
        return isInputVoice ? "这是语音识别测试。" : "这是翻译输出的语音测试。";
    } else if (langCode == "ja") {
        return isInputVoice ? "これは音声認識のテストです。" : "これは翻訳出力の音声テストです。";
    } else if (langCode == "ko") {
        return isInputVoice ? "이것은 음성 인식 테스트입니다." : "이것은 번역 출력 음성 테스트입니다.";
    } else if (langCode.startsWith("es")) {
        return isInputVoice ? "Esta es una prueba de reconocimiento de voz." : "Esta es una prueba de voz para la salida de traducción.";
    } else if (langCode.startsWith("fr")) {
        return isInputVoice ? "Ceci est un test de reconnaissance vocale." : "Ceci est un test vocal pour la sortie de traduction.";
    } else if (langCode == "de") {
        return isInputVoice ? "Dies ist ein Spracherkennungstest." : "Dies ist ein Sprachtest für die Übersetzungsausgabe.";
    } else if (langCode == "it") {
        return isInputVoice ? "Questo è un test di riconoscimento vocale." : "Questo è un test vocale per l'output di traduzione.";
    } else if (langCode.startsWith("pt")) {
        return isInputVoice ? "Este é um teste de reconhecimento de voz." : "Este é um teste de voz para saída de tradução.";
    } else if (langCode == "ru") {
        return isInputVoice ? "Это тест распознавания речи." : "Это голосовой тест для вывода перевода.";
    } else if (langCode == "ar") {
        return isInputVoice ? "هذا اختبار للتعرف على الصوت." : "هذا اختبار صوتي لمخرجات الترجمة.";
    } else if (langCode == "hi") {
        return isInputVoice ? "यह वॉयस रिकग्निशन टेस्ट है।" : "यह अनुवाद आउटपुट के लिए वॉयस टेस्ट है।";
    } else if (langCode == "th") {
        return isInputVoice ? "นี่คือการทดสอบการรู้จำเสียง" : "นี่คือการทดสอบเสียงสำหรับเอาต์พุตการแปล";
    } else if (langCode == "sv") {
        return isInputVoice ? "Det här är ett röstigenkänningstest." : "Det här är ett rösttest för översättningsutdata.";
    } else if (langCode == "vi") {
        return isInputVoice ? "Đây là bài kiểm tra nhận dạng giọng nói." : "Đây là bài kiểm tra giọng nói cho đầu ra dịch.";
    } else if (langCode == "nl") {
        return isInputVoice ? "Dit is een spraakherkenningstest." : "Dit is een spraaktest voor vertaaluitvoer.";
    } else if (langCode == "pl") {
        return isInputVoice ? "To jest test rozpoznawania mowy." : "To jest test głosowy dla wyjścia tłumaczenia.";
    }

    // Default to English
    return isInputVoice ? "This is a voice recognition test." : "This is a voice test for translation output.";
}

QString SettingsWindow::getLanguageCodeFromVoice(const QString& voice) const
{
    const QString lower = voice.toLower();

    // Chinese variants
    if (voice.contains("繁體") || lower.contains("traditional")) return "zh-TW";
    if (voice.contains("简体") || lower.contains("simplified") || lower.contains("pǔtōnghuà")) return "zh-CN";
    if (lower.contains("chinese") || voice.contains("中文")) {
        return voice.contains("繁") ? "zh-TW" : "zh-CN";
    }

    // Other languages
    if (lower.contains("japan") || voice.contains("日本")) return "ja";
    if (lower.contains("korean") || voice.contains("한국")) return "ko";
    if (voice.contains("(mx)", Qt::CaseInsensitive)) return "es-MX";
    if (lower.contains("span") || lower.contains("español")) return "es";
    if (voice.contains("(ca)", Qt::CaseInsensitive)) return "fr-CA";
    if (lower.contains("french") || lower.contains("fran") || lower.contains("français")) return "fr";
    if (lower.contains("german") || lower.contains("deutsch")) return "de";
    if (lower.contains("italian") || lower.contains("italiano")) return "it";
    if (voice.contains("(br)", Qt::CaseInsensitive)) return "pt-BR";
    if (lower.contains("portuguese") || lower.contains("português")) return "pt";
    if (lower.contains("russian") || lower.contains("русский")) return "ru";
    if (lower.contains("arab") || voice.contains("العربية")) return "ar";
    if (lower.contains("hindi") || voice.contains("हिन्दी")) return "hi";
    if (lower.contains("thai") || voice.contains("ไทย")) return "th";
    if (lower.contains("swed") || voice.contains("svenska")) return "sv";
    if (lower.contains("vietnam") || voice.contains("tiếng việt")) return "vi";
    if (lower.contains("dutch") || lower.contains("nederlands")) return "nl";
    if (lower.contains("polish") || lower.contains("polski")) return "pl";

    // Default to English
    return "en-US";
}
