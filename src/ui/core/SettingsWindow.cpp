#include "SettingsWindow.h"
#include "OCREngine.h"
#include "GlobalShortcutManager.h"
#include "SystemTray.h"
#include "FloatingWidget.h"
#include "TTSEngine.h"
#include "TTSManager.h"
#include "../tts/ModernTTSManager.h"
#include "../tts/EdgeTTSProvider.h"
#include "../tts/GoogleWebTTSProvider.h"
#include "../core/LanguageManager.h"
#include "../core/AppSettings.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QMessageBox>
#include <QHideEvent>
#include <QCoreApplication>
#include <QTimer>
#include <QProcess>
#include <QIcon>
#include <QSignalBlocker>
#include <QtMath>
#include <QPainter>
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

void SettingsWindow::setShortcutManager(GlobalShortcutManager *manager)
{
    shortcutManager = manager;
}

void SettingsWindow::setSystemTray(SystemTray *tray)
{
    systemTray = tray;
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

    QPushButton *closeBtn = new QPushButton("Close", this);
    closeBtn->setObjectName("closeBtn");
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(resetBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);

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
    connect(ocrLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString& language){
        updateVoicesForLanguage();
        // Auto-sync translation source language with OCR input language (unless auto-detect is enabled)
        if (sourceLanguageCombo && autoDetectSourceCheck && !autoDetectSourceCheck->isChecked()) {
            sourceLanguageCombo->setCurrentText(language);
        }
    });

    inputLangLayout->addWidget(ocrLanguageCombo, 1);

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
    connect(targetLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString& language){
        updateVoicesForLanguage();
        // Ensure translation target language stays in sync with general target language
        qDebug() << "Target language changed to:" << language << "- this will be used for translation";
    });

    outputLangLayout->addWidget(targetLanguageCombo, 1);

    langLayout->addRow(outputLabel, outputLangWidget);
    
    // Info text
    QLabel *infoLabel = new QLabel("ℹ️ <i>TTS voice language automatically matches the Output Language setting.<br>"
                                  "TTS will automatically speak OCR results after translation completes.</i>");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;");
    langLayout->addRow(infoLabel);


    layout->addWidget(langGroup);
    
    // Screenshot Settings Group
    QGroupBox *screenshotGroup = new QGroupBox("📸 Screenshot Settings");
    screenshotGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; padding-top: 10px; }");
    QVBoxLayout *screenshotLayout = new QVBoxLayout(screenshotGroup);
    screenshotLayout->setSpacing(15);
    
    // Dimming opacity slider
    QLabel *dimmingLabel = new QLabel("🌑 Screen Dimming Opacity:");
    dimmingLabel->setToolTip("Controls how dark the screen overlay appears during screenshot selection");
    screenshotLayout->addWidget(dimmingLabel);
    
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    
    QLabel *lightLabel = new QLabel("Light");
    lightLabel->setStyleSheet("font-size: 10px; color: #888;");
    sliderLayout->addWidget(lightLabel);
    
    screenshotDimmingSlider = new QSlider(Qt::Horizontal);
    screenshotDimmingSlider->setRange(30, 220);  // 30 (very light) to 220 (very dark)
    screenshotDimmingSlider->setValue(120);      // Default: medium darkness
    screenshotDimmingSlider->setTickPosition(QSlider::TicksBelow);
    screenshotDimmingSlider->setTickInterval(30);
    sliderLayout->addWidget(screenshotDimmingSlider, 1);
    
    QLabel *darkLabel = new QLabel("Dark");
    darkLabel->setStyleSheet("font-size: 10px; color: #888;");
    sliderLayout->addWidget(darkLabel);
    
    screenshotDimmingValue = new QLabel("47%");
    screenshotDimmingValue->setStyleSheet("font-weight: bold; min-width: 40px;");
    screenshotDimmingValue->setAlignment(Qt::AlignCenter);
    sliderLayout->addWidget(screenshotDimmingValue);
    
    screenshotLayout->addLayout(sliderLayout);
    
    // Preview widget (fixed background, not affected by theme)
    screenshotPreview = new QLabel();
    screenshotPreview->setMinimumHeight(80);
    screenshotPreview->setMaximumHeight(80);
    screenshotPreview->setScaledContents(true);
    screenshotPreview->setStyleSheet("QLabel { background-color: #f0f0f0; border: 2px solid #555; border-radius: 4px; }");
    screenshotLayout->addWidget(screenshotPreview);
    
    QLabel *previewLabel = new QLabel("Preview: Selected area will stay clear, background will be dimmed");
    previewLabel->setStyleSheet("font-size: 10px; color: #888; font-style: italic;");
    screenshotLayout->addWidget(previewLabel);
    
    layout->addWidget(screenshotGroup);
    
    // Connect slider to update preview
    connect(screenshotDimmingSlider, &QSlider::valueChanged, this, [this](int value) {
        int percentage = (value * 100) / 255;
        screenshotDimmingValue->setText(QString("%1%").arg(percentage));
        updateScreenshotPreview();
    });
    
    // Global Shortcuts Group
    QGroupBox *shortcutsGroup = new QGroupBox("⌨️ Global Shortcuts");
    shortcutsGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; padding-top: 10px; }");
    QFormLayout *shortcutsLayout = new QFormLayout(shortcutsGroup);
    shortcutsLayout->setSpacing(15);
    
    // Screenshot shortcut
    screenshotShortcutEdit = new QKeySequenceEdit();
    screenshotShortcutEdit->setToolTip("Press the key combination you want to use for taking screenshots");
    shortcutsLayout->addRow("📸 Take Screenshot:", screenshotShortcutEdit);
    
    // Disable global shortcuts when field gets focus, enable when loses focus
    screenshotShortcutEdit->installEventFilter(this);
    connect(screenshotShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &keySequence) {
        // Save immediately when changed
        QSettings settings;
        QString shortcutString = keySequence.toString();
        // On macOS, Qt uses "Ctrl" to represent Cmd, but we need "Meta" for Carbon API
        #ifdef Q_OS_MACOS
        shortcutString.replace("Ctrl", "Meta");
        #endif
        settings.setValue("shortcuts/screenshot", shortcutString);
        qDebug() << "Screenshot shortcut changed to:" << shortcutString;
        if (shortcutManager) shortcutManager->reloadShortcuts();
        if (systemTray) systemTray->updateShortcutLabels();
    });
    
    // Toggle visibility shortcut
    toggleShortcutEdit = new QKeySequenceEdit();
    toggleShortcutEdit->setToolTip("Press the key combination you want to use for showing/hiding the floating widget");
    shortcutsLayout->addRow("👁️ Toggle Widget:", toggleShortcutEdit);
    
    // Disable global shortcuts when field gets focus, enable when loses focus
    toggleShortcutEdit->installEventFilter(this);
    connect(toggleShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &keySequence) {
        // Save immediately when changed
        QSettings settings;
        QString shortcutString = keySequence.toString();
        // On macOS, Qt uses "Ctrl" to represent Cmd, but we need "Meta" for Carbon API
        #ifdef Q_OS_MACOS
        shortcutString.replace("Ctrl", "Meta");
        #endif
        settings.setValue("shortcuts/toggle", shortcutString);
        qDebug() << "Toggle shortcut changed to:" << shortcutString;
        if (shortcutManager) shortcutManager->reloadShortcuts();
        if (systemTray) systemTray->updateShortcutLabels();
    });
    
    // Info label for shortcuts
    QLabel *shortcutInfoLabel = new QLabel("ℹ️ <i>Shortcuts work globally, even when the app is in the background.<br>"
                                          "Avoid system shortcuts like Cmd+Q, Cmd+W, etc.</i>");
    shortcutInfoLabel->setWordWrap(true);
    shortcutInfoLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;");
    shortcutsLayout->addRow(shortcutInfoLabel);
    
    layout->addWidget(shortcutsGroup);
    
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
    
    // Platform-specific OCR engines - clean and simple
#ifdef Q_OS_MACOS
    ocrEngineCombo->addItems({"Apple Vision (Recommended)", "Tesseract"});
#elif defined(Q_OS_WIN)
    ocrEngineCombo->addItems({"Windows OCR (Recommended)", "Tesseract"});
#else
    ocrEngineCombo->addItems({"Tesseract"});
#endif
    
    connect(ocrEngineCombo, &QComboBox::currentTextChanged, this, &SettingsWindow::onOcrEngineChanged);
    engineLayout->addRow("Engine:", ocrEngineCombo);

    layout->addWidget(engineGroup);
    layout->addStretch();
}

void SettingsWindow::setupTranslationTab()
{
    translationTab = new QWidget();
    tabWidget->addTab(translationTab, "🌐 Translation");

    QVBoxLayout *layout = new QVBoxLayout(translationTab);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 20, 20, 20);

    // Auto-Translate Toggle
    QGroupBox *autoGroup = new QGroupBox("Automatic Processing");
    QVBoxLayout *autoLayout = new QVBoxLayout(autoGroup);

    autoTranslateCheck = new QCheckBox("Auto-Translate: Automatically translate after OCR");
    autoTranslateCheck->setChecked(true);
    autoLayout->addWidget(autoTranslateCheck);

    layout->addWidget(autoGroup);

    // Translation Engine
    QGroupBox *engineGroup = new QGroupBox("Translation Engine");
    QFormLayout *engineLayout = new QFormLayout(engineGroup);
    engineLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    engineLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    engineLayout->setSpacing(12);

    translationEngineCombo = new QComboBox();
    translationEngineCombo->addItems({"Google Translate (Free)", "LibreTranslate", "Ollama Local LLM",
                                     "Microsoft Translator", "DeepL (Free)", "Offline Dictionary"});
    connect(translationEngineCombo, &QComboBox::currentTextChanged, this, &SettingsWindow::onTranslationEngineChanged);
    engineLayout->addRow("Engine:", translationEngineCombo);

    layout->addWidget(engineGroup);
    layout->addStretch();
}

void SettingsWindow::setupAppearanceTab()
{
    appearanceTab = new QWidget();
    tabWidget->addTab(appearanceTab, "🎨 Appearance");

    QVBoxLayout *layout = new QVBoxLayout(appearanceTab);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 20, 20, 20);

    // Theme Group
    QGroupBox *themeGroup = new QGroupBox("Theme");
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    themeLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    themeLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    themeLayout->setSpacing(12);

    themeCombo = new QComboBox();
    themeCombo->addItems({"Auto (System)", "Light", "Dark", "High Contrast", "Cyberpunk"});
    connect(themeCombo, &QComboBox::currentTextChanged, this, [this](const QString &text){
        // Preview theme instantly
        ThemeManager::instance().applyTheme(ThemeManager::fromString(text));
    });
    themeLayout->addRow("Theme:", themeCombo);

    layout->addWidget(themeGroup);

    // Widget Size Group
    QGroupBox *sizeGroup = new QGroupBox("Floating Widget");
    QFormLayout *sizeLayout = new QFormLayout(sizeGroup);
    sizeLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    sizeLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeLayout->setSpacing(12);

    // Widget width slider (height is calculated proportionally)
    QHBoxLayout *widthLayout = new QHBoxLayout();
    widgetWidthSlider = new QSlider(Qt::Horizontal);
    widgetWidthSlider->setRange(100, 250);
    widgetWidthSlider->setValue(140);
    widgetWidthSlider->setTickPosition(QSlider::TicksBelow);
    widgetWidthSlider->setTickInterval(25);
    
    widgetWidthLabel = new QLabel("140 px");
    widgetWidthLabel->setMinimumWidth(60);
    widgetWidthLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    widthLayout->addWidget(widgetWidthSlider);
    widthLayout->addWidget(widgetWidthLabel);
    
    sizeLayout->addRow("Widget Width:", widthLayout);
    
    // Connect slider to update label and widget size in real-time
    connect(widgetWidthSlider, &QSlider::valueChanged, this, [this](int width) {
        widgetWidthLabel->setText(QString("%1 px").arg(width));
        
        // Save immediately
        QSettings settings;
        settings.setValue("appearance/widgetWidth", width);
        
        // Update the actual floating widget size in real-time
        FloatingWidget *floatingWidget = qobject_cast<FloatingWidget*>(parent());
        if (floatingWidget) {
            int height = static_cast<int>(width * 0.43); // 140x60 ratio = 0.43
            floatingWidget->setFixedSize(width, height);
        }
    });
    
    // Deprecated widgets - set to null
    widgetSizeCombo = nullptr;
    opacitySlider = nullptr;
    animationsCheck = nullptr;
    soundsCheck = nullptr;

    layout->addWidget(sizeGroup);
    layout->addStretch();
}

void SettingsWindow::applyModernStyling()
{
    // Now themed globally via ThemeManager
}

void SettingsWindow::loadSettings()
{
    // OCR Settings - map old settings to new display names
    QString savedEngine = settings->value("ocr/engine", "").toString();
    QString displayEngine;
    
    // Map internal engine names to display names
    if (savedEngine == "AppleVision" || savedEngine == "Apple Vision") {
        displayEngine = "Apple Vision (Recommended)";
    } else if (savedEngine == "WindowsOCR" || savedEngine == "Windows OCR") {
        displayEngine = "Windows OCR (Recommended)";
    } else if (savedEngine == "Tesseract") {
        displayEngine = "Tesseract";
    } else {
        // Default based on platform if no saved setting
#ifdef Q_OS_MACOS
        displayEngine = "Apple Vision (Recommended)";
#elif defined(Q_OS_WIN)
        displayEngine = "Windows OCR (Recommended)";
#else
        displayEngine = "Tesseract";
#endif
    }
    
    ocrEngineCombo->setCurrentText(displayEngine);
    ocrLanguageCombo->setCurrentText(settings->value("ocr/language", "English").toString());
    
    // Screenshot Settings
    int dimmingOpacity = settings->value("screenshot/dimmingOpacity", 120).toInt();
    screenshotDimmingSlider->setValue(dimmingOpacity);
    updateScreenshotPreview();  // Update preview with loaded value
    
    // Load Global Shortcuts - default to Cmd+Shift+X on Mac, Ctrl+Shift+X on others
#ifdef Q_OS_MACOS
    QString defaultScreenshotShortcut = "Meta+Shift+X";  // Cmd+Shift+X on macOS
    QString defaultToggleShortcut = "Meta+Shift+Z";      // Cmd+Shift+Z on macOS
#else
    QString defaultScreenshotShortcut = "Ctrl+Shift+X";
    QString defaultToggleShortcut = "Ctrl+Shift+Z";
#endif
    
    QString screenshotShortcut = settings->value("shortcuts/screenshot", defaultScreenshotShortcut).toString();
    QString toggleShortcut = settings->value("shortcuts/toggle", defaultToggleShortcut).toString();
    
    // On macOS, we store as "Meta" for Carbon API, but QKeySequenceEdit needs "Ctrl" (which Qt maps to Cmd)
    #ifdef Q_OS_MACOS
    QString screenshotDisplay = screenshotShortcut;
    screenshotDisplay.replace("Meta", "Ctrl");
    QString toggleDisplay = toggleShortcut;
    toggleDisplay.replace("Meta", "Ctrl");
    screenshotShortcutEdit->setKeySequence(QKeySequence(screenshotDisplay));
    toggleShortcutEdit->setKeySequence(QKeySequence(toggleDisplay));
    #else
    screenshotShortcutEdit->setKeySequence(QKeySequence(screenshotShortcut));
    toggleShortcutEdit->setKeySequence(QKeySequence(toggleShortcut));
    #endif

    // Translation Settings
    autoTranslateCheck->setChecked(settings->value("translation/autoTranslate", true).toBool());
    if (overlayModeCombo) overlayModeCombo->setCurrentText(settings->value("translation/overlayMode", "Deep Learning Mode").toString());
    translationEngineCombo->setCurrentText(settings->value("translation/engine", "Google Translate (Free)").toString());

    // Load legacy settings if needed
    if (autoDetectSourceCheck) {
        bool autoDetectEnabled = settings->value("translation/autoDetectSource", false).toBool();
        autoDetectSourceCheck->setChecked(autoDetectEnabled);
    }
    
    if (sourceLanguageCombo) {
        QString defaultSourceLang = settings->value("general/ocrLanguage", "English").toString();
        sourceLanguageCombo->setCurrentText(settings->value("translation/sourceLanguage", defaultSourceLang).toString());
    }

    if (targetLanguageCombo) targetLanguageCombo->setCurrentText(settings->value("translation/targetLanguage", "English").toString());
    if (apiUrlEdit) apiUrlEdit->setText(settings->value("translation/apiUrl", "").toString());
    if (apiKeyEdit) apiKeyEdit->setText(settings->value("translation/apiKey", "").toString());

    // Appearance Settings
    themeCombo->setCurrentText(settings->value("appearance/theme", "Auto (System)").toString());
    if (widgetWidthSlider) {
        int width = settings->value("appearance/widgetWidth", 140).toInt();
        widgetWidthSlider->setValue(width);
        widgetWidthLabel->setText(QString("%1 px").arg(width));
    }
    if (opacitySlider) opacitySlider->setValue(settings->value("appearance/opacity", 90).toInt());
    if (animationsCheck) animationsCheck->setChecked(settings->value("appearance/animations", true).toBool());
    if (soundsCheck) soundsCheck->setChecked(settings->value("appearance/sounds", false).toBool());

    // TTS is now always enabled - no checkbox controls

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
            // Use target language from General settings for Google TTS
            QString targetLang = settings->value("translation/targetLanguage", "English").toString();
            QLocale locale = languageNameToLocale(targetLang);
            ttsEngine->configureGoogle(QString(), primaryVoice, locale.name());
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

    // OCR Settings - map display names to internal engine names
    if (ocrEngineCombo) {
        QString displayEngine = ocrEngineCombo->currentText();
        QString internalEngine;
        
        // Map display names to internal names for OCREngine
        if (displayEngine.contains("Apple Vision")) {
            internalEngine = "AppleVision";
        } else if (displayEngine.contains("Windows OCR")) {
            internalEngine = "WindowsOCR";
        } else if (displayEngine == "Tesseract") {
            internalEngine = "Tesseract";
        } else {
            internalEngine = displayEngine; // fallback
        }
        
        settings->setValue("ocr/engine", internalEngine);
    }
    if (ocrLanguageCombo) settings->setValue("ocr/language", ocrLanguageCombo->currentText());
    
    // Screenshot Settings
    if (screenshotDimmingSlider) settings->setValue("screenshot/dimmingOpacity", screenshotDimmingSlider->value());
    
    // Note: Global Shortcuts are saved immediately when changed, no need to save here

    // Translation Settings
    if (autoTranslateCheck) settings->setValue("translation/autoTranslate", autoTranslateCheck->isChecked());
    if (overlayModeCombo) settings->setValue("translation/overlayMode", overlayModeCombo->currentText());
    if (translationEngineCombo) settings->setValue("translation/engine", translationEngineCombo->currentText());
    if (autoDetectSourceCheck) settings->setValue("translation/autoDetectSource", autoDetectSourceCheck->isChecked());
    
    // Source language always matches OCR language (simplified design)
    if (ocrLanguageCombo) {
        settings->setValue("translation/sourceLanguage", ocrLanguageCombo->currentText());
    }
    
    if (targetLanguageCombo) settings->setValue("translation/targetLanguage", targetLanguageCombo->currentText());
    if (apiUrlEdit) settings->setValue("translation/apiUrl", apiUrlEdit->text().trimmed());
    if (apiKeyEdit) settings->setValue("translation/apiKey", apiKeyEdit->text());

    // Appearance Settings
    if (themeCombo) settings->setValue("appearance/theme", themeCombo->currentText());
    // Note: widgetWidth is saved immediately when slider changes
    if (opacitySlider) settings->setValue("appearance/opacity", opacitySlider->value());
    if (animationsCheck) settings->setValue("appearance/animations", animationsCheck->isChecked());
    if (soundsCheck) settings->setValue("appearance/sounds", soundsCheck->isChecked());

    // TTS is now always enabled - no settings to save

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
        // TTS is now always enabled
        ttsEngine->setTTSInputEnabled(true);
        ttsEngine->setTTSOutputEnabled(true);

        if (providerId == QStringLiteral("edge-free")) {
            ttsEngine->setEdgeExecutable(edgeExePath);
            ttsEngine->setEdgeVoice(primaryVoice);
        } else if (providerId == QStringLiteral("google-web")) {
            // Use target language from General settings for Google TTS
            QString targetLang = targetLanguageCombo ? targetLanguageCombo->currentText() : "English";
            QLocale locale = languageNameToLocale(targetLang);
            ttsEngine->configureGoogle(QString(), primaryVoice, locale.name());
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

    // Reset Translation settings
    autoTranslateCheck->setChecked(true);
    translationEngineCombo->setCurrentText("Google Translate (Free)");
    if (autoDetectSourceCheck) autoDetectSourceCheck->setChecked(false);
    if (sourceLanguageCombo) {
        sourceLanguageCombo->clear();
        sourceLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)",
                                      "Japanese", "Korean", "Spanish", "French", "German", "Russian",
                                      "Portuguese", "Italian", "Dutch", "Polish", "Swedish", "Arabic", "Hindi", "Thai", "Vietnamese"});
        sourceLanguageCombo->setCurrentText("English");
    }
    if (targetLanguageCombo) targetLanguageCombo->setCurrentText("English");
    if (apiUrlEdit) apiUrlEdit->clear();
    if (apiKeyEdit) apiKeyEdit->clear();

    // Reset Appearance settings
    themeCombo->setCurrentText("Auto (System)");
    if (widgetWidthSlider) {
        widgetWidthSlider->setValue(140);
        QSettings settings;
        settings.setValue("appearance/widgetWidth", 140);
    }
    if (opacitySlider) opacitySlider->setValue(90);
    if (animationsCheck) animationsCheck->setChecked(true);
    if (soundsCheck) soundsCheck->setChecked(false);
    
    // Reset Shortcuts
    if (screenshotShortcutEdit) {
        QSettings settings;
        #ifdef Q_OS_MACOS
        // Store as Meta for Carbon API, display as Ctrl for QKeySequenceEdit (Qt maps Ctrl to Cmd on macOS)
        screenshotShortcutEdit->setKeySequence(QKeySequence("Ctrl+Shift+X"));
        settings.setValue("shortcuts/screenshot", "Meta+Shift+X");
        #else
        screenshotShortcutEdit->setKeySequence(QKeySequence("Ctrl+Shift+X"));
        settings.setValue("shortcuts/screenshot", "Ctrl+Shift+X");
        #endif
    }
    if (toggleShortcutEdit) {
        QSettings settings;
        #ifdef Q_OS_MACOS
        toggleShortcutEdit->setKeySequence(QKeySequence("Ctrl+Shift+Z"));
        settings.setValue("shortcuts/toggle", "Meta+Shift+Z");
        #else
        toggleShortcutEdit->setKeySequence(QKeySequence("Ctrl+Shift+Z"));
        settings.setValue("shortcuts/toggle", "Ctrl+Shift+Z");
        #endif
    }
    
    // Reload shortcuts and update tray
    if (shortcutManager) shortcutManager->reloadShortcuts();
    if (systemTray) systemTray->updateShortcutLabels();
    
    // TTS is now always enabled - no checkboxes to reset
    if (ttsProviderCombo) {
        const int idx = ttsProviderCombo->findData(QStringLiteral("system"));
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

bool SettingsWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Disable global shortcuts when QKeySequenceEdit gets focus
    if ((obj == screenshotShortcutEdit || obj == toggleShortcutEdit)) {
        if (event->type() == QEvent::FocusIn) {
            if (shortcutManager) {
                shortcutManager->setEnabled(false);
                qDebug() << "QKeySequenceEdit focused - shortcuts disabled";
            }
        } else if (event->type() == QEvent::FocusOut) {
            if (shortcutManager) {
                shortcutManager->setEnabled(true);
                qDebug() << "QKeySequenceEdit unfocused - shortcuts enabled";
            }
        }
    }
    return QDialog::eventFilter(obj, event);
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
    // OCR engine changed - no validation needed for simplified design
}

void SettingsWindow::onTranslationEngineChanged()
{
    // Translation engine changed - simplified design, no API config needed
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





void SettingsWindow::setupTTSTab()
{
    ttsTab = new QWidget();
    tabWidget->addTab(ttsTab, "🔊 Text-to-Speech");
    
    QVBoxLayout *layout = new QVBoxLayout(ttsTab);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Main TTS Group
    QGroupBox *ttsGroup = new QGroupBox("Text-to-Speech");
    QFormLayout *ttsLayout = new QFormLayout(ttsGroup);
    ttsLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    ttsLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ttsLayout->setSpacing(12);
    
    // Enable checkbox
    ttsEnabledCheck = new QCheckBox("Enable voice playback");
    ttsEnabledCheck->setChecked(true);
    ttsLayout->addRow("", ttsEnabledCheck);
    
    // Provider dropdown
    ttsProviderCombo = new QComboBox();
    ttsProviderCombo->addItem("System Voices (Recommended)", QStringLiteral("system"));
    ttsProviderCombo->addItem("Google Web TTS", QStringLiteral("google-web"));
    ttsProviderCombo->addItem("Microsoft Edge TTS", QStringLiteral("edge-free"));
    ttsLayout->addRow("Voice Provider:", ttsProviderCombo);
    
    // Contextual hint label
    providerInfoLabel = new QLabel();
    providerInfoLabel->setWordWrap(true);
    providerInfoLabel->setStyleSheet("color: #666; font-size: 11px; font-style: italic;");
    ttsLayout->addRow("", providerInfoLabel);
    
    // Input voice
    QWidget *inputWidget = new QWidget();
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(8);
    
    inputVoiceCombo = new QComboBox();
    inputVoiceCombo->setEditable(true);
    inputVoiceCombo->setPlaceholderText("Select input voice");
    inputLayout->addWidget(inputVoiceCombo, 1);
    
    QPushButton *testInputBtn = new QPushButton("🔊 Test");
    inputLayout->addWidget(testInputBtn);
    
    ttsLayout->addRow("Input Voice:", inputWidget);
    
    // Output voice
    QWidget *outputWidget = new QWidget();
    QHBoxLayout *outputLayout = new QHBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(8);
    
    outputVoiceCombo = new QComboBox();
    outputVoiceCombo->setEditable(true);
    outputVoiceCombo->setPlaceholderText("Select output voice");
    outputLayout->addWidget(outputVoiceCombo, 1);
    
    testTTSBtn = new QPushButton("🔊 Test");
    outputLayout->addWidget(testTTSBtn);
    
    ttsLayout->addRow("Output Voice:", outputWidget);
    
    layout->addWidget(ttsGroup);
    layout->addStretch();
    
    // Set deprecated widgets to null
    voiceCombo = nullptr;
    refreshVoicesButton = nullptr;
    inputVoiceLabel = nullptr;
    outputVoiceLabel = nullptr;
    testTextEdit = nullptr;
    stopTTSBtn = nullptr;
    ttsStatusText = nullptr;
    edgeExeLabel = nullptr;
    edgeExeRow = nullptr;
    edgeExePathEdit = nullptr;
    ttsProviderLabel = nullptr;
    edgeHintLabel = nullptr;
    advancedVoiceToggle = nullptr;
    edgeBrowseButton = nullptr;
    
    // Connect provider change to update hint
    connect(ttsProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString providerId = ttsProviderCombo->currentData().toString();
        updateProviderUI(providerId);
    });
    
    // Connect test buttons
    connect(testInputBtn, &QPushButton::clicked, this, [this]() {
        if (!ttsEngine || !ttsEnabledCheck->isChecked()) return;
        
        QString voice = inputVoiceCombo->currentText().trimmed();
        if (voice.isEmpty()) return;
        
        // Use OCR language from General settings for input voice
        QString languageName = ocrLanguageCombo ? ocrLanguageCombo->currentText() : "English";
        QLocale locale = languageNameToLocale(languageName);
        QString testText = getTestTextForLanguage(languageName, true);
        
        ttsEngine->setInputVoice(voice);
        ttsEngine->speak(testText, true, locale);
    });
    
    connect(testTTSBtn, &QPushButton::clicked, this, &SettingsWindow::onTestTTSClicked);
    
    // Initialize if engine available
    if (ttsEngine) {
        updateProviderUI(ttsEngine->providerId());
        updateVoicesForLanguage();
    }
}


void SettingsWindow::onVoiceChanged()
{
    // Voice selection will be handled when applying settings
}

void SettingsWindow::onTestTTSClicked()
{
    if (!ttsEngine || !ttsEnabledCheck->isChecked()) return;

    QString voice = outputVoiceCombo ? outputVoiceCombo->currentText().trimmed() : QString();
    if (voice.isEmpty()) return;

    // Use target language from General settings for output voice
    QString languageName = targetLanguageCombo ? targetLanguageCombo->currentText() : "English";
    QLocale locale = languageNameToLocale(languageName);
    QString testText = getTestTextForLanguage(languageName, false);

    ttsEngine->setOutputVoice(voice);
    ttsEngine->speak(testText, false, locale);
}

void SettingsWindow::updateProviderUI(const QString& providerId)
{
    // Update contextual hint based on provider
    if (providerInfoLabel) {
        if (providerId == QStringLiteral("system")) {
            providerInfoLabel->setText("Download voices: Settings → Accessibility → Spoken Content");
        } else if (providerId == QStringLiteral("google-web")) {
            providerInfoLabel->setText("Works out of the box, no installation required");
        } else if (providerId == QStringLiteral("edge-free")) {
            providerInfoLabel->setText("Install: pip install edge-tts");
        }
    }

    // Update TTS engine provider
    if (ttsEngine) {
        ttsEngine->setProviderId(providerId);
        updateVoicesForLanguage();
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

QLocale SettingsWindow::languageNameToLocale(const QString& languageName) const
{
    if (languageName.contains("English", Qt::CaseInsensitive)) return QLocale(QLocale::English, QLocale::UnitedStates);
    if (languageName.contains("Chinese") && languageName.contains("Simplified")) return QLocale(QLocale::Chinese, QLocale::China);
    if (languageName.contains("Chinese") && languageName.contains("Traditional")) return QLocale(QLocale::Chinese, QLocale::Taiwan);
    if (languageName.contains("Japanese", Qt::CaseInsensitive)) return QLocale(QLocale::Japanese, QLocale::Japan);
    if (languageName.contains("Korean", Qt::CaseInsensitive)) return QLocale(QLocale::Korean, QLocale::SouthKorea);
    if (languageName.contains("Spanish", Qt::CaseInsensitive)) return QLocale(QLocale::Spanish, QLocale::Spain);
    if (languageName.contains("French", Qt::CaseInsensitive)) return QLocale(QLocale::French, QLocale::France);
    if (languageName.contains("German", Qt::CaseInsensitive)) return QLocale(QLocale::German, QLocale::Germany);
    if (languageName.contains("Russian", Qt::CaseInsensitive)) return QLocale(QLocale::Russian, QLocale::Russia);
    if (languageName.contains("Portuguese", Qt::CaseInsensitive)) return QLocale(QLocale::Portuguese, QLocale::Portugal);
    if (languageName.contains("Italian", Qt::CaseInsensitive)) return QLocale(QLocale::Italian, QLocale::Italy);
    if (languageName.contains("Dutch", Qt::CaseInsensitive)) return QLocale(QLocale::Dutch, QLocale::Netherlands);
    if (languageName.contains("Polish", Qt::CaseInsensitive)) return QLocale(QLocale::Polish, QLocale::Poland);
    if (languageName.contains("Swedish", Qt::CaseInsensitive)) return QLocale(QLocale::Swedish, QLocale::Sweden);
    if (languageName.contains("Arabic", Qt::CaseInsensitive)) return QLocale(QLocale::Arabic, QLocale::SaudiArabia);
    if (languageName.contains("Hindi", Qt::CaseInsensitive)) return QLocale(QLocale::Hindi, QLocale::India);
    if (languageName.contains("Thai", Qt::CaseInsensitive)) return QLocale(QLocale::Thai, QLocale::Thailand);
    if (languageName.contains("Vietnamese", Qt::CaseInsensitive)) return QLocale(QLocale::Vietnamese, QLocale::Vietnam);
    return QLocale::system();
}

QString SettingsWindow::getTestTextForLanguage(const QString& languageName, bool isInputVoice) const
{
    if (languageName.contains("Chinese") && languageName.contains("Traditional")) {
        return isInputVoice ? "這是語音識別測試。" : "這是翻譯輸出的語音測試。";
    } else if (languageName.contains("Chinese") && languageName.contains("Simplified")) {
        return isInputVoice ? "这是语音识别测试。" : "这是翻译输出的语音测试。";
    } else if (languageName.contains("Japanese")) {
        return isInputVoice ? "これは音声認識のテストです。" : "これは翻訳出力の音声テストです。";
    } else if (languageName.contains("Korean")) {
        return isInputVoice ? "이것은 음성 인식 테스트입니다." : "이것은 번역 출력 음성 테스트입니다.";
    } else if (languageName.contains("Spanish")) {
        return isInputVoice ? "Esta es una prueba de reconocimiento de voz." : "Esta es una prueba de voz para la salida de traducción.";
    } else if (languageName.contains("French")) {
        return isInputVoice ? "Ceci est un test de reconnaissance vocale." : "Ceci est un test vocal pour la sortie de traduction.";
    } else if (languageName.contains("German")) {
        return isInputVoice ? "Dies ist ein Spracherkennungstest." : "Dies ist ein Sprachtest für die Übersetzungsausgabe.";
    } else if (languageName.contains("Italian")) {
        return isInputVoice ? "Questo è un test di riconoscimento vocale." : "Questo è un test vocale per l'output di traduzione.";
    } else if (languageName.contains("Portuguese")) {
        return isInputVoice ? "Este é um teste de reconhecimento de voz." : "Este é um teste de voz para saída de tradução.";
    } else if (languageName.contains("Russian")) {
        return isInputVoice ? "Это тест распознавания речи." : "Это голосовой тест для вывода перевода.";
    } else if (languageName.contains("Arabic")) {
        return isInputVoice ? "هذا اختبار للتعرف على الصوت." : "هذا اختبار صوتي لمخرجات الترجمة.";
    } else if (languageName.contains("Hindi")) {
        return isInputVoice ? "यह वॉयस रिकग्निशन टेस्ट है।" : "यह अनुवाद आउटपुट के लिए वॉयस टेस्ट है।";
    } else if (languageName.contains("Thai")) {
        return isInputVoice ? "นี่คือการทดสอบการรู้จำเสียง" : "นี่คือการทดสอบเสียงสำหรับเอาต์พุตการแปล";
    } else if (languageName.contains("Vietnamese")) {
        return isInputVoice ? "Đây là bài kiểm tra nhận dạng giọng nói." : "Đây là bài kiểm tra giọng nói cho đầu ra dịch.";
    } else if (languageName.contains("Dutch")) {
        return isInputVoice ? "Dit is een spraakherkenningstest." : "Dit is een spraaktest voor vertaaluitvoer.";
    } else if (languageName.contains("Polish")) {
        return isInputVoice ? "To jest test rozpoznawania mowy." : "To jest test głosowy dla wyjścia tłumaczenia.";
    } else if (languageName.contains("Swedish")) {
        return isInputVoice ? "Det här är ett röstigenkänningstest." : "Det här är ett rösttest för översättningsutdata.";
    }

    // Default to English
    return isInputVoice ? "This is a voice recognition test." : "This is a voice test for translation output.";
}

void SettingsWindow::updateScreenshotPreview()
{
    if (!screenshotPreview) return;
    
    // Get current dimming opacity from slider
    int opacity = screenshotDimmingSlider->value();
    
    // Use fixed size for preview
    int previewWidth = qMax(screenshotPreview->width(), 400);
    int previewHeight = 80;
    
    // Create a pixmap to draw the preview
    QPixmap previewPixmap(previewWidth, previewHeight);
    previewPixmap.fill(Qt::white);
    
    QPainter painter(&previewPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw a sample "screenshot" background (gradient to simulate content)
    QLinearGradient bgGradient(0, 0, previewPixmap.width(), previewPixmap.height());
    bgGradient.setColorAt(0, QColor(100, 120, 200));
    bgGradient.setColorAt(1, QColor(200, 150, 100));
    painter.fillRect(previewPixmap.rect(), bgGradient);
    
    // Add some text to simulate content
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(previewPixmap.rect(), Qt::AlignCenter, "Sample Screenshot Content");
    
    // Draw the dimming overlay everywhere
    painter.fillRect(previewPixmap.rect(), QColor(0, 0, 0, opacity));
    
    // Draw a "selection" area in the center that stays clear
    QRect selectionRect(previewPixmap.width() * 0.3, previewPixmap.height() * 0.3,
                       previewPixmap.width() * 0.4, previewPixmap.height() * 0.4);
    
    // Clear the dimming in the selection area by redrawing the background
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    QLinearGradient selGradient(selectionRect.left(), selectionRect.top(),
                                selectionRect.right(), selectionRect.bottom());
    selGradient.setColorAt(0, QColor(100, 120, 200));
    selGradient.setColorAt(1, QColor(200, 150, 100));
    painter.fillRect(selectionRect, selGradient);
    
    // Draw text in selection area
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(selectionRect, Qt::AlignCenter, "Selected\nArea");
    
    // Draw selection border
    QPalette themePalette = ThemeManager::instance().getCurrentPalette();
    QColor accentColor = themePalette.color(QPalette::Highlight);
    painter.setPen(QPen(accentColor, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selectionRect);
    
    painter.end();
    
    // Set the pixmap directly to the QLabel
    screenshotPreview->setPixmap(previewPixmap);
}
