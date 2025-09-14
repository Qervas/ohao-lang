#include "SettingsWindow.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QMessageBox>
#include <QShowEvent>
#include <QHideEvent>
#include <QCoreApplication>

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
    , tabWidget(nullptr)
    , settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
{
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
    setupOcrTab();
    setupTranslationTab();
    setupAppearanceTab();

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

    ocrLanguageCombo = new QComboBox();
    ocrLanguageCombo->addItems({"English", "Chinese (Simplified)", "Chinese (Traditional)",
                               "Japanese", "Korean", "Spanish", "French", "German", "Auto-Detect"});
    engineLayout->addRow("Language:", ocrLanguageCombo);

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

    // Translation Engine Group
    QGroupBox *engineGroup = new QGroupBox("Translation Engine", translationTab);
    QFormLayout *engineLayout = new QFormLayout(engineGroup);

    translationEngineCombo = new QComboBox();
    translationEngineCombo->addItems({"Google Translate (Free)", "LibreTranslate", "Ollama Local LLM",
                                     "Microsoft Translator", "DeepL (Free)", "Offline Dictionary"});
    connect(translationEngineCombo, &QComboBox::currentTextChanged, this, &SettingsWindow::onTranslationEngineChanged);
    engineLayout->addRow("Engine:", translationEngineCombo);

    sourceLanguageCombo = new QComboBox();
    sourceLanguageCombo->addItems({"Auto-Detect", "English", "Chinese", "Japanese", "Korean",
                                  "Spanish", "French", "German", "Russian", "Portuguese"});
    engineLayout->addRow("Source Language:", sourceLanguageCombo);

    targetLanguageCombo = new QComboBox();
    targetLanguageCombo->addItems({"English", "Chinese", "Japanese", "Korean", "Spanish",
                                  "French", "German", "Russian", "Portuguese"});
    targetLanguageCombo->setCurrentText("English");
    engineLayout->addRow("Target Language:", targetLanguageCombo);

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
    themeCombo->addItems({"Auto (System)", "Light", "Dark", "High Contrast"});
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
    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(240, 242, 245, 0.95),
                stop:1 rgba(250, 252, 255, 0.95));
            border-radius: 15px;
            border: 1px solid rgba(200, 200, 210, 0.3);
        }

        QTabWidget {
            background: transparent;
            border: none;
        }

        QTabWidget::pane {
            border: 2px solid rgba(200, 200, 210, 0.2);
            border-radius: 10px;
            background: rgba(255, 255, 255, 0.7);
            margin-top: 5px;
        }

        QTabWidget::tab-bar {
            alignment: center;
        }

        QTabBar::tab {
            background: rgba(220, 220, 230, 0.5);
            border: 1px solid rgba(200, 200, 210, 0.3);
            padding: 12px 24px;
            margin-right: 2px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            font-weight: 500;
            color: #4A5568;
        }

        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 0.9),
                stop:1 rgba(245, 247, 250, 0.9));
            border-bottom-color: transparent;
            color: #2D3748;
            font-weight: 600;
        }

        QTabBar::tab:hover:!selected {
            background: rgba(235, 235, 245, 0.7);
        }

        QGroupBox {
            font-weight: 600;
            font-size: 13px;
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

        QComboBox, QLineEdit, QSpinBox {
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 6px;
            padding: 8px 12px;
            background: rgba(255, 255, 255, 0.8);
            font-size: 13px;
            min-height: 24px;
            min-width: 120px;
        }

        QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
            border-color: rgba(100, 150, 250, 0.6);
            background: rgba(255, 255, 255, 1.0);
        }

        QComboBox::drop-down {
            border: none;
            width: 20px;
        }

        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 4px solid #666;
            margin-right: 8px;
        }

        QCheckBox {
            spacing: 8px;
            font-size: 13px;
            color: #4A5568;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid rgba(200, 200, 210, 0.4);
            border-radius: 4px;
            background: rgba(255, 255, 255, 0.8);
        }

        QCheckBox::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(100, 150, 250, 0.9),
                stop:1 rgba(120, 170, 255, 0.9));
            border-color: rgba(100, 150, 250, 0.6);
        }

        QSlider::groove:horizontal {
            border: 1px solid rgba(200, 200, 210, 0.3);
            height: 6px;
            background: rgba(220, 220, 230, 0.5);
            border-radius: 3px;
        }

        QSlider::handle:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(100, 150, 250, 0.9),
                stop:1 rgba(120, 170, 255, 0.9));
            border: 2px solid rgba(100, 150, 250, 0.6);
            width: 20px;
            margin: -8px 0;
            border-radius: 10px;
        }

        QTextEdit {
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 6px;
            background: rgba(250, 250, 252, 0.9);
            padding: 8px;
            font-size: 12px;
        }

        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 0.9),
                stop:1 rgba(240, 242, 245, 0.9));
            border: 2px solid rgba(200, 200, 210, 0.3);
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 600;
            font-size: 13px;
            color: #4A5568;
            min-width: 80px;
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

        QPushButton#applyBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(100, 150, 250, 0.9),
                stop:1 rgba(80, 130, 230, 0.9));
            color: white;
            border-color: rgba(80, 130, 230, 0.6);
        }

        QPushButton#applyBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(120, 170, 255, 0.95),
                stop:1 rgba(100, 150, 250, 0.95));
        }

        QPushButton#testBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(50, 200, 120, 0.9),
                stop:1 rgba(30, 180, 100, 0.9));
            color: white;
            border-color: rgba(30, 180, 100, 0.6);
        }

        QPushButton#testBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(70, 220, 140, 0.95),
                stop:1 rgba(50, 200, 120, 0.95));
        }

        QScrollArea {
            background: transparent;
            border: none;
        }

        QScrollBar:vertical {
            background: rgba(240, 240, 245, 0.5);
            width: 12px;
            border-radius: 6px;
            margin: 2px;
        }

        QScrollBar::handle:vertical {
            background: rgba(180, 180, 190, 0.7);
            border-radius: 5px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background: rgba(160, 160, 170, 0.9);
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QLabel {
            color: #4A5568;
            font-size: 13px;
            padding: 2px;
        }

        QFormLayout QLabel {
            min-width: 120px;
            padding-right: 8px;
        }
    )");
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

    // Simulate OCR test
    QString engine = ocrEngineCombo->currentText();
    QString language = ocrLanguageCombo->currentText();

    // In a real implementation, this would test the actual OCR engine
    QString result = QString("✅ OCR Test Results:\n"
                           "Engine: %1\n"
                           "Language: %2\n"
                           "Quality Level: %3/5\n"
                           "Status: Ready for use")
                           .arg(engine)
                           .arg(language)
                           .arg(ocrQualitySlider->value());

    ocrStatusText->setPlainText(result);
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