#include "ModernSettingsWindow.h"
#include "AppSettings.h"
#include "TTSEngine.h"
#include "TTSManager.h"
#include "ModernTTSManager.h"
#include "GlobalShortcutManager.h"
#include "SystemTray.h"
#include "ThemeManager.h"
#include "ThemeColors.h"
#include "FloatingWidget.h"
#include "LanguageManager.h"
#include <QApplication>
#include <QScreen>
#include <QGroupBox>
#include <QDebug>
#include <QCoreApplication>
#include <QMessageBox>
#include <QProcess>

ModernSettingsWindow::ModernSettingsWindow(QWidget *parent)
    : QDialog(parent)
    , settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    qDebug() << "ModernSettingsWindow: Starting initialization...";

    // Get TTS engine
    ttsEngine = TTSManager::instance().ttsEngine();

    // Setup UI
    setupUI();
    createSidebar();
    createContentStack();
    applyMacOSStyle();

    // Load settings (with isInitializing flag set)
    loadSettings();

    // Done initializing
    isInitializing = false;

    // Connect to theme changes for runtime updates
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](ThemeManager::Theme theme) {
        QString themeName = ThemeManager::toString(theme);
        applyTheme(themeName);
    });

    qDebug() << "ModernSettingsWindow: Initialization complete!";
}

ModernSettingsWindow::~ModernSettingsWindow()
{
    qDebug() << "ModernSettingsWindow: Destroyed";
}

void ModernSettingsWindow::setShortcutManager(GlobalShortcutManager *manager)
{
    shortcutManager = manager;
}

void ModernSettingsWindow::setSystemTray(SystemTray *tray)
{
    systemTray = tray;
}

void ModernSettingsWindow::setupUI()
{
    setWindowTitle("Settings");
    setModal(true);
    setMinimumSize(800, 600);
    resize(900, 650);

    // Center on screen
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2,
             (screenGeometry.height() - height()) / 2);
    }

    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
}

void ModernSettingsWindow::createSidebar()
{
    sidebar = new QListWidget(this);
    sidebar->setObjectName("settingsSidebar");
    sidebar->setFixedWidth(160);
    sidebar->setSpacing(4);
    sidebar->setMovement(QListWidget::Static);
    sidebar->setFocusPolicy(Qt::NoFocus);
    sidebar->setUniformItemSizes(true);

    // Add sidebar items - simple text, styling will add proper spacing
    QStringList items = {
        "General",
        "OCR",
        "Translation",
        "Appearance",
        "Voice"
    };

    for (const QString &item : items) {
        QListWidgetItem *listItem = new QListWidgetItem(item);
        listItem->setSizeHint(QSize(160, 36));  // Fixed size for consistency
        sidebar->addItem(listItem);
    }

    sidebar->setCurrentRow(0);

    connect(sidebar, &QListWidget::currentRowChanged,
            this, &ModernSettingsWindow::onSidebarItemChanged);

    mainLayout->addWidget(sidebar);
}

void ModernSettingsWindow::createContentStack()
{
    contentStack = new QStackedWidget(this);
    contentStack->setObjectName("settingsContent");

    // Create all pages
    contentStack->addWidget(createGeneralPage());
    contentStack->addWidget(createOCRPage());
    contentStack->addWidget(createTranslationPage());
    contentStack->addWidget(createAppearancePage());
    contentStack->addWidget(createVoicePage());

    mainLayout->addWidget(contentStack, 1);
}

QWidget* ModernSettingsWindow::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // Page title
    QLabel *title = new QLabel("General Settings");
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Languages group
    QGroupBox *langGroup = new QGroupBox("Languages");
    langGroup->setObjectName("settingsGroup");
    QFormLayout *langLayout = new QFormLayout(langGroup);
    langLayout->setSpacing(12);
    langLayout->setContentsMargins(0, 0, 0, 0);

    ocrLanguageCombo = new QComboBox();
    // Populate from LanguageManager - the single source of truth
    QStringList ocrLanguages;
    for (const auto& lang : LanguageManager::instance().allLanguages()) {
        ocrLanguages << lang.englishName;  // Use English names for consistency
    }
    ocrLanguageCombo->addItems(ocrLanguages);
    connect(ocrLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                updateVoiceList();  // Update voices when OCR language changes
                onSettingChanged();
            });
    langLayout->addRow("OCR Language:", ocrLanguageCombo);

    targetLanguageCombo = new QComboBox();
    // Populate from LanguageManager - the single source of truth
    QStringList translationLanguages;
    for (const auto& lang : LanguageManager::instance().allLanguages()) {
        translationLanguages << lang.englishName;  // Use English names for consistency
    }
    targetLanguageCombo->addItems(translationLanguages);
    connect(targetLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModernSettingsWindow::onSettingChanged);
    langLayout->addRow("Translation Target:", targetLanguageCombo);

    layout->addWidget(langGroup);

    // Screenshot group
    QGroupBox *screenshotGroup = new QGroupBox("Screenshot");
    screenshotGroup->setObjectName("settingsGroup");
    QFormLayout *screenshotLayout = new QFormLayout(screenshotGroup);
    screenshotLayout->setSpacing(12);
    screenshotLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *dimmingWidget = new QWidget();
    QHBoxLayout *dimmingLayout = new QHBoxLayout(dimmingWidget);
    dimmingLayout->setContentsMargins(0, 0, 0, 0);

    screenshotDimmingSlider = new QSlider(Qt::Horizontal);
    screenshotDimmingSlider->setRange(30, 220);
    screenshotDimmingSlider->setValue(120);
    connect(screenshotDimmingSlider, &QSlider::valueChanged, this, [this](int value) {
        if (dimmingValueLabel) {
            int percentage = (value * 100) / 255;
            dimmingValueLabel->setText(QString("%1%").arg(percentage));
        }
        if (!isInitializing) {
            saveSettings();
        }
    });

    dimmingValueLabel = new QLabel("47%");
    dimmingValueLabel->setMinimumWidth(40);
    dimmingValueLabel->setAlignment(Qt::AlignRight);

    dimmingLayout->addWidget(screenshotDimmingSlider);
    dimmingLayout->addWidget(dimmingValueLabel);

    screenshotLayout->addRow("Screen Dimming:", dimmingWidget);

    layout->addWidget(screenshotGroup);

    // Shortcuts group
    QGroupBox *shortcutsGroup = new QGroupBox("Global Shortcuts");
    shortcutsGroup->setObjectName("settingsGroup");
    QFormLayout *shortcutsLayout = new QFormLayout(shortcutsGroup);
    shortcutsLayout->setSpacing(12);
    shortcutsLayout->setContentsMargins(0, 0, 0, 0);

    screenshotShortcutEdit = new QKeySequenceEdit();
    connect(screenshotShortcutEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &ModernSettingsWindow::onSettingChanged);
    shortcutsLayout->addRow("Take Screenshot:", screenshotShortcutEdit);

    toggleShortcutEdit = new QKeySequenceEdit();
    connect(toggleShortcutEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &ModernSettingsWindow::onSettingChanged);
    shortcutsLayout->addRow("Toggle Widget:", toggleShortcutEdit);

#ifdef Q_OS_LINUX
    // Add button to update GNOME shortcuts
    QPushButton *updateGnomeBtn = new QPushButton("Update GNOME Shortcuts");
    updateGnomeBtn->setToolTip("Apply these shortcuts to GNOME keyboard settings");
    connect(updateGnomeBtn, &QPushButton::clicked, this, [this]() {
        updateGnomeShortcuts();
    });
    shortcutsLayout->addRow("", updateGnomeBtn);

    // Add info label
    QLabel *gnomeInfo = new QLabel("Note: On GNOME Wayland, click 'Update GNOME Shortcuts' after changing shortcuts");
    gnomeInfo->setWordWrap(true);
    gnomeInfo->setStyleSheet("color: #8E8E93; font-size: 11px; padding: 4px 0px;");
    shortcutsLayout->addRow("", gnomeInfo);
#endif

    layout->addWidget(shortcutsGroup);

    // Window Behavior group
    QGroupBox *behaviorGroup = new QGroupBox("Window Behavior");
    behaviorGroup->setObjectName("settingsGroup");
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);

    QCheckBox *alwaysOnTopCheck = new QCheckBox("Keep floating widget always on top");
    alwaysOnTopCheck->setChecked(settings.value("window/alwaysOnTop", true).toBool());
    alwaysOnTopCheck->setStyleSheet("padding: 4px 0px;");
    connect(alwaysOnTopCheck, &QCheckBox::toggled, this, [this](bool checked) {
        settings.setValue("window/alwaysOnTop", checked);

        // Apply to FloatingWidget immediately
        FloatingWidget *floatingWidget = qobject_cast<FloatingWidget*>(parent());
        if (floatingWidget) {
            floatingWidget->setAlwaysOnTop(checked);
        }

        if (!isInitializing) {
            saveSettings();
        }
    });
    behaviorLayout->addWidget(alwaysOnTopCheck);

    layout->addWidget(behaviorGroup);

    layout->addStretch();
    return page;
}

QWidget* ModernSettingsWindow::createOCRPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // Page title
    QLabel *title = new QLabel("OCR Settings");
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // OCR Engine group
    QGroupBox *engineGroup = new QGroupBox("OCR Engine");
    engineGroup->setObjectName("settingsGroup");
    QFormLayout *engineLayout = new QFormLayout(engineGroup);
    engineLayout->setSpacing(12);
    engineLayout->setContentsMargins(0, 0, 0, 0);

    ocrEngineCombo = new QComboBox();
#ifdef Q_OS_MACOS
    ocrEngineCombo->addItems({"Apple Vision (Recommended)", "Tesseract"});
#else
    // Windows and Linux both use Tesseract
    ocrEngineCombo->addItems({"Tesseract"});
#endif
    connect(ocrEngineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModernSettingsWindow::onSettingChanged);
    engineLayout->addRow("Engine:", ocrEngineCombo);

    layout->addWidget(engineGroup);
    layout->addStretch();
    return page;
}

QWidget* ModernSettingsWindow::createTranslationPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // Page title
    QLabel *title = new QLabel("Translation Settings");
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Options group
    QGroupBox *optionsGroup = new QGroupBox("Options");
    optionsGroup->setObjectName("settingsGroup");
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

    autoTranslateCheck = new QCheckBox("Automatically translate after OCR");
    autoTranslateCheck->setChecked(true);
    autoTranslateCheck->setStyleSheet("padding: 4px 0px;");
    connect(autoTranslateCheck, &QCheckBox::toggled,
            this, &ModernSettingsWindow::onSettingChanged);
    optionsLayout->addWidget(autoTranslateCheck);

    layout->addWidget(optionsGroup);

    // Engine group - Only Google Translate (free)
    QGroupBox *engineGroup = new QGroupBox("Translation Engine");
    engineGroup->setObjectName("settingsGroup");
    QFormLayout *engineLayout = new QFormLayout(engineGroup);
    engineLayout->setSpacing(12);
    engineLayout->setContentsMargins(0, 0, 0, 0);

    translationEngineCombo = new QComboBox();
    translationEngineCombo->addItems({"Google Translate (Free)"});
    translationEngineCombo->setEnabled(false); // Only one option, disable selection
    connect(translationEngineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!isInitializing) {
            saveSettings();
        }
    });
    engineLayout->addRow("Engine:", translationEngineCombo);

    // API fields removed - Google Translate doesn't need API key or custom URL
    apiUrlEdit = nullptr;
    apiKeyEdit = nullptr;

    layout->addWidget(engineGroup);
    layout->addStretch();
    return page;
}

QWidget* ModernSettingsWindow::createAppearancePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // Page title
    QLabel *title = new QLabel("Appearance");
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Theme group
    QGroupBox *themeGroup = new QGroupBox("Theme");
    themeGroup->setObjectName("settingsGroup");
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    themeLayout->setSpacing(12);
    themeLayout->setContentsMargins(0, 0, 0, 0);

    themeCombo = new QComboBox();
    themeCombo->addItems({"Auto (System)", "Light", "Dark", "Cyberpunk"});
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!isInitializing && themeCombo) {
            QString selectedTheme = themeCombo->currentText();
            applyTheme(selectedTheme);
            ThemeManager::instance().applyTheme(ThemeManager::fromString(selectedTheme));
            saveSettings();
        }
    });
    themeLayout->addRow("Theme:", themeCombo);

    layout->addWidget(themeGroup);

    // Widget size group
    QGroupBox *sizeGroup = new QGroupBox("Floating Widget Size");
    sizeGroup->setObjectName("settingsGroup");
    QFormLayout *sizeLayout = new QFormLayout(sizeGroup);
    sizeLayout->setSpacing(12);
    sizeLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *widthWidget = new QWidget();
    QHBoxLayout *widthLayout = new QHBoxLayout(widthWidget);
    widthLayout->setContentsMargins(0, 0, 0, 0);

    widgetWidthSlider = new QSlider(Qt::Horizontal);
    widgetWidthSlider->setRange(100, 250);
    widgetWidthSlider->setValue(140);
    connect(widgetWidthSlider, &QSlider::valueChanged, this, [this](int width) {
        if (widthValueLabel) {
            widthValueLabel->setText(QString("%1 px").arg(width));
        }

        if (!isInitializing) {
            // Update floating widget size in real-time
            FloatingWidget *floatingWidget = qobject_cast<FloatingWidget*>(parent());
            if (floatingWidget) {
                int height = static_cast<int>(width * 0.43);
                floatingWidget->setFixedSize(width, height);
            }
            saveSettings();
        }
    });

    widthValueLabel = new QLabel("140 px");
    widthValueLabel->setMinimumWidth(60);
    widthValueLabel->setAlignment(Qt::AlignRight);

    widthLayout->addWidget(widgetWidthSlider);
    widthLayout->addWidget(widthValueLabel);

    sizeLayout->addRow("Width:", widthWidget);

    layout->addWidget(sizeGroup);
    layout->addStretch();
    return page;
}

QWidget* ModernSettingsWindow::createVoicePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // Page title
    QLabel *title = new QLabel("Text-to-Speech");
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // TTS Enable group
    QGroupBox *enableGroup = new QGroupBox("Voice Playback");
    enableGroup->setObjectName("settingsGroup");
    QVBoxLayout *enableLayout = new QVBoxLayout(enableGroup);

    ttsEnabledCheck = new QCheckBox("Enable text-to-speech");
    ttsEnabledCheck->setChecked(true);
    connect(ttsEnabledCheck, &QCheckBox::toggled,
            this, &ModernSettingsWindow::onSettingChanged);
    enableLayout->addWidget(ttsEnabledCheck);

    layout->addWidget(enableGroup);

    // Provider group
    QGroupBox *providerGroup = new QGroupBox("Voice Provider");
    providerGroup->setObjectName("settingsGroup");
    QFormLayout *providerLayout = new QFormLayout(providerGroup);
    providerLayout->setSpacing(12);
    providerLayout->setContentsMargins(0, 0, 0, 0);

    ttsProviderCombo = new QComboBox();
    ttsProviderCombo->addItem("Microsoft Edge TTS (Recommended - Fast & High Quality)", QStringLiteral("edge-free"));
#ifdef QT_TEXTTOSPEECH_AVAILABLE
    ttsProviderCombo->addItem("System Voices (Offline)", QStringLiteral("system"));
#endif
    ttsProviderCombo->addItem("Google Web TTS (Fast but Basic)", QStringLiteral("google-web"));
    connect(ttsProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                // Show loading message
                if (voiceCombo) {
                    voiceCombo->clear();
                    voiceCombo->addItem("Loading voices...");
                    voiceCombo->setEnabled(false);
                }

                // Process events to show the loading message
                QCoreApplication::processEvents();

                // Update voice list (this might take time for detection)
                updateVoiceList();
                onSettingChanged();
            });
    providerLayout->addRow("Provider:", ttsProviderCombo);

    providerInfoLabel = new QLabel();
    providerInfoLabel->setWordWrap(true);
    providerInfoLabel->setStyleSheet("color: #86868B; font-size: 11px;");
    providerLayout->addRow("", providerInfoLabel);

    layout->addWidget(providerGroup);

    // Voice group - single voice selector
    QGroupBox *voicesGroup = new QGroupBox("Voice");
    voicesGroup->setObjectName("settingsGroup");
    QFormLayout *voicesLayout = new QFormLayout(voicesGroup);
    voicesLayout->setSpacing(12);
    voicesLayout->setContentsMargins(0, 0, 0, 0);

    // Single voice selector with test button
    QWidget *voiceWidget = new QWidget();
    QHBoxLayout *voiceLayout = new QHBoxLayout(voiceWidget);
    voiceLayout->setContentsMargins(0, 0, 0, 0);

    voiceCombo = new QComboBox();
    voiceCombo->setEditable(false);
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModernSettingsWindow::onSettingChanged);
    voiceLayout->addWidget(voiceCombo, 1);

    testVoiceBtn = new QPushButton("Test");
    testVoiceBtn->setFixedWidth(60);
    testVoiceBtn->setEnabled(voiceCombo->count() > 0);
    connect(testVoiceBtn, &QPushButton::clicked, this, [this]() {
        if (voiceCombo->currentIndex() >= 0) {
            auto voiceInfo = voiceCombo->currentData().value<ModernTTSManager::VoiceInfo>();
            qDebug() << "Testing voice:" << voiceInfo.name;
            ModernTTSManager::instance().testVoice(voiceInfo);
        }
    });
    voiceLayout->addWidget(testVoiceBtn);

    voicesLayout->addRow("Voice:", voiceWidget);

    layout->addWidget(voicesGroup);

    // Speech Language group - iOS-style toggle
    QGroupBox *speechGroup = new QGroupBox("Speech Language");
    speechGroup->setObjectName("settingsGroup");
    QVBoxLayout *speechLayout = new QVBoxLayout(speechGroup);
    speechLayout->setSpacing(8);
    speechLayout->setContentsMargins(0, 0, 0, 0);

    ttsSpeakTranslationCheck = new QCheckBox("Speak Translated Text (Output Language)");
    ttsSpeakTranslationCheck->setChecked(false);  // Default: speak original/input language
    ttsSpeakTranslationCheck->setToolTip("When enabled: TTS speaks the translated text in the target language\nWhen disabled: TTS speaks the original text in the source language");
    connect(ttsSpeakTranslationCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                // Update voice list to show voices for appropriate language
                updateVoiceList();
                onSettingChanged();
            });
    speechLayout->addWidget(ttsSpeakTranslationCheck);

    ttsWordByWordCheck = new QCheckBox("Word-by-Word Reading (Slower)");
    ttsWordByWordCheck->setChecked(false);  // Default: normal reading
    ttsWordByWordCheck->setToolTip("Add extra space between words for slower, clearer pronunciation");
    connect(ttsWordByWordCheck, &QCheckBox::toggled, this, &ModernSettingsWindow::onSettingChanged);
    speechLayout->addWidget(ttsWordByWordCheck);

    QLabel *speechInfo = new QLabel("Choose whether TTS speaks the original text or the translation");
    speechInfo->setWordWrap(true);
    speechInfo->setStyleSheet("color: #86868B; font-size: 11px; padding-left: 24px;");
    speechLayout->addWidget(speechInfo);

    layout->addWidget(speechGroup);
    layout->addStretch();

    // Populate voices for the current provider
    updateVoiceList();

    return page;
}

void ModernSettingsWindow::applyMacOSStyle()
{
    // Initial style will be applied by applyTheme
    // Use the CURRENT theme from ThemeManager (not settings, to handle Auto properly)
    ThemeManager::Theme currentTheme = ThemeManager::instance().getCurrentTheme();
    QString themeName = ThemeManager::toString(currentTheme);
    applyTheme(themeName);
}

void ModernSettingsWindow::applyTheme(const QString &themeName)
{
    qDebug() << "ModernSettingsWindow::applyTheme called with theme:" << themeName;

    // Use centralized theme colors
    ThemeColors::ThemeColorSet colors = ThemeColors::getColorSet(themeName);

    QString bgColor = colors.window.name();
    QString contentBg = colors.window.name();
    QString sidebarBg = colors.button.name();
    QString sidebarHover = colors.buttonHover.name();
    QString textColor = colors.windowText.name();
    QString borderColor = colors.floatingWidgetBorder.name();
    QString inputBg = colors.base.name();
    QString groupBg = colors.alternateBase.name();
    QString secondaryText = colors.windowText.lighter(150).name();
    QString sliderGroove = colors.floatingWidgetBorder.name();
    QString highlightColor = colors.highlight.name();

    qDebug() << "ModernSettingsWindow colors: bg=" << bgColor << "text=" << textColor << "highlight=" << highlightColor;

    QString style = QString(R"(
        QDialog {
            background-color: %1;
        }

        QListWidget#settingsSidebar {
            background-color: %2;
            border: none;
            border-right: 1px solid %3;
            outline: none;
        }

        QListWidget#settingsSidebar::item {
            height: 36px;
            padding: 8px 16px;
            color: %4;
            font-size: 13px;
            border-radius: 6px;
            margin: 2px 8px;
            text-align: left;
        }

        QListWidget#settingsSidebar::item:hover {
            background-color: %5;
        }

        QListWidget#settingsSidebar::item:selected {
            background-color: %11;
            color: white;
        }

        QStackedWidget#settingsContent {
            background-color: %6;
        }

        QLabel#pageTitle {
            font-size: 22px;
            font-weight: 600;
            color: %7;
            margin-bottom: 16px;
        }

        QGroupBox#settingsGroup {
            font-size: 13px;
            font-weight: 600;
            color: %8;
            border: 1px solid %9;
            border-radius: 8px;
            margin-top: 16px;
            padding: 20px 16px 16px 16px;
            background-color: %10;
        }

        QGroupBox#settingsGroup::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px 0 6px;
            background-color: %11;
        }

        QComboBox {
            padding: 5px 8px;
            border: 1px solid %12;
            border-radius: 6px;
            background-color: %13;
            color: %14;
            min-height: 24px;
        }

        QComboBox:hover {
            border-color: #007AFF;
        }

        QComboBox:focus {
            border-color: #007AFF;
            outline: none;
        }

        QComboBox::drop-down {
            border: none;
            padding-right: 8px;
        }

        QSlider::groove:horizontal {
            height: 4px;
            background: %15;
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            background: %16;
            border: 2px solid %11;
            width: 16px;
            height: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }

        QSlider::handle:horizontal:hover {
            border-color: %11;
        }

        QLabel {
            color: %17;
            background-color: transparent;
            font-size: 13px;
        }

        QCheckBox {
            spacing: 8px;
            color: %18;
            background-color: transparent;
            font-size: 13px;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1.5px solid %19;
            border-radius: 4px;
            background-color: %20;
        }

        QCheckBox::indicator:checked {
            background-color: #007AFF;
            border-color: #007AFF;
        }

        QLineEdit {
            padding: 6px 8px;
            border: 1px solid %21;
            border-radius: 6px;
            background-color: %22;
            color: %23;
            font-size: 13px;
        }

        QLineEdit:focus {
            border-color: %11;
            outline: none;
        }

        QLineEdit::placeholder {
            color: %24;
        }

        QPushButton {
            padding: 6px 16px;
            background-color: %11;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 500;
        }

        QPushButton:hover {
            background-color: %11;
            opacity: 0.8;
        }

        QPushButton:pressed {
            background-color: %11;
            opacity: 0.6;
        }

        QComboBox QAbstractItemView {
            background-color: %25;
            border: 1px solid %26;
            border-radius: 6px;
            selection-background-color: #007AFF;
            selection-color: white;
            color: %27;
        }

        QKeySequenceEdit {
            padding: 6px 8px;
            border: 1px solid %28;
            border-radius: 6px;
            background-color: %29;
            color: %30;
            font-size: 13px;
        }

        QKeySequenceEdit:focus {
            border-color: %11;
            outline: none;
        }
    )")
    .arg(bgColor)           // 1
    .arg(sidebarBg)         // 2
    .arg(borderColor)       // 3
    .arg(textColor)         // 4
    .arg(sidebarHover)      // 5
    .arg(contentBg)         // 6
    .arg(textColor)         // 7
    .arg(textColor)         // 8
    .arg(borderColor)       // 9
    .arg(groupBg)           // 10
    .arg(highlightColor)    // 11 - highlight/accent color for selections and focus
    .arg(borderColor)       // 12
    .arg(inputBg)           // 13
    .arg(textColor)         // 14
    .arg(sliderGroove)      // 15
    .arg(inputBg)           // 16 - slider handle background
    .arg(textColor)         // 17
    .arg(textColor)         // 18
    .arg(borderColor)       // 19
    .arg(inputBg)           // 20
    .arg(borderColor)       // 21
    .arg(inputBg)           // 22
    .arg(textColor)         // 23
    .arg(secondaryText)     // 24
    .arg(inputBg)           // 25
    .arg(borderColor)       // 26
    .arg(textColor)         // 27
    .arg(borderColor)       // 28
    .arg(inputBg)           // 29
    .arg(textColor);        // 30

    setStyleSheet(style);
}

void ModernSettingsWindow::onSidebarItemChanged(int index)
{
    if (contentStack) {
        contentStack->setCurrentIndex(index);
    }
}

void ModernSettingsWindow::onSettingChanged()
{
    if (!isInitializing) {
        saveSettings();
    }
}

void ModernSettingsWindow::loadSettings()
{
    qDebug() << "ModernSettingsWindow: Loading settings...";

    // General - Languages
    if (ocrLanguageCombo) {
        ocrLanguageCombo->setCurrentText(settings.value("ocr/language", "English").toString());
    }
    if (targetLanguageCombo) {
        targetLanguageCombo->setCurrentText(settings.value("translation/targetLanguage", "English").toString());
    }

    // General - Screenshot
    if (screenshotDimmingSlider) {
        int opacity = settings.value("screenshot/dimmingOpacity", 120).toInt();
        screenshotDimmingSlider->setValue(opacity);
    }

    // General - Shortcuts
#ifdef Q_OS_MACOS
    QString defaultScreenshot = "Meta+Shift+X";
    QString defaultToggle = "Meta+Shift+H";
#else
    QString defaultScreenshot = "Ctrl+Alt+X";
    QString defaultToggle = "Ctrl+Alt+H";
#endif

    if (screenshotShortcutEdit) {
        QString shortcut = settings.value("shortcuts/screenshot", defaultScreenshot).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl"); // Qt displays Cmd as Ctrl
#endif
        screenshotShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (toggleShortcutEdit) {
        QString shortcut = settings.value("shortcuts/toggle", defaultToggle).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        toggleShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    // OCR
    if (ocrEngineCombo) {
        QString savedEngine = settings.value("ocr/engine", "").toString();
#ifdef Q_OS_MACOS
        QString display = savedEngine.contains("Apple") ? "Apple Vision (Recommended)" : "Tesseract";
#else
        // Windows and Linux: Always use Tesseract
        QString display = "Tesseract";
#endif
        ocrEngineCombo->setCurrentText(display);
    }

    // Translation
    if (autoTranslateCheck) {
        autoTranslateCheck->setChecked(settings.value("translation/autoTranslate", true).toBool());
    }
    if (translationEngineCombo) {
        translationEngineCombo->setCurrentText(settings.value("translation/engine", "Google Translate (Free)").toString());
    }
    if (apiUrlEdit) {
        apiUrlEdit->setText(settings.value("translation/apiUrl", "").toString());
    }
    if (apiKeyEdit) {
        apiKeyEdit->setText(settings.value("translation/apiKey", "").toString());
    }

    // Appearance
    if (themeCombo) {
        // Get theme from AppSettings (which ThemeManager uses)
        QString savedTheme = AppSettings::instance().theme();

        // Map AppSettings theme names to combo box items
        if (savedTheme == "Auto") {
            savedTheme = "Auto (System)";
        }

        qDebug() << "ModernSettingsWindow: Loading theme setting:" << savedTheme;
        themeCombo->setCurrentText(savedTheme);
    }
    if (widgetWidthSlider) {
        int width = settings.value("appearance/widgetWidth", 140).toInt();
        widgetWidthSlider->setValue(width);
    }

    // TTS
    if (ttsEnabledCheck) {
        ttsEnabledCheck->setChecked(settings.value("tts/enabled", true).toBool());
    }
    if (ttsProviderCombo) {
        QString savedProvider = settings.value("tts/provider", "edge-free").toString();
        int providerIndex = ttsProviderCombo->findData(savedProvider);
        if (providerIndex >= 0) {
            ttsProviderCombo->setCurrentIndex(providerIndex);
        }
    }
    if (ttsSpeakTranslationCheck) {
        ttsSpeakTranslationCheck->setChecked(settings.value("tts/speakTranslation", false).toBool());
    }
    if (ttsWordByWordCheck) {
        ttsWordByWordCheck->setChecked(settings.value("tts/wordByWord", false).toBool());
    }
    // Voice will be restored by updateVoiceList() using the saved voice name

    qDebug() << "ModernSettingsWindow: Settings loaded successfully";
}

void ModernSettingsWindow::saveSettings()
{
    // General - Languages - Save to both QSettings AND AppSettings
    if (ocrLanguageCombo) {
        QString ocrLang = ocrLanguageCombo->currentText();
        settings.setValue("ocr/language", ocrLang);

        // Update AppSettings OCR config
        auto ocrConfig = AppSettings::instance().getOCRConfig();
        ocrConfig.language = ocrLang;
        AppSettings::instance().setOCRConfig(ocrConfig);
    }
    if (targetLanguageCombo) {
        QString targetLang = targetLanguageCombo->currentText();
        settings.setValue("translation/targetLanguage", targetLang);

        // Update AppSettings Translation config
        auto translationConfig = AppSettings::instance().getTranslationConfig();
        translationConfig.targetLanguage = targetLang;
        AppSettings::instance().setTranslationConfig(translationConfig);
    }

    // General - Screenshot
    if (screenshotDimmingSlider) {
        settings.setValue("screenshot/dimmingOpacity", screenshotDimmingSlider->value());
    }

    // General - Shortcuts
    if (screenshotShortcutEdit) {
        QString shortcut = screenshotShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Ctrl", "Meta"); // Store as Meta for Carbon API
#endif
        settings.setValue("shortcuts/screenshot", shortcut);
        if (shortcutManager) shortcutManager->reloadShortcuts();
        if (systemTray) systemTray->updateShortcutLabels();
    }

    if (toggleShortcutEdit) {
        QString shortcut = toggleShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Ctrl", "Meta");
#endif
        settings.setValue("shortcuts/toggle", shortcut);
        if (shortcutManager) shortcutManager->reloadShortcuts();
        if (systemTray) systemTray->updateShortcutLabels();
    }

    // OCR
    if (ocrEngineCombo) {
        QString text = ocrEngineCombo->currentText();
        QString internalName;
        if (text.contains("Apple")) internalName = "AppleVision";
        else internalName = "Tesseract";
        settings.setValue("ocr/engine", internalName);
    }

    // Translation
    if (autoTranslateCheck) {
        settings.setValue("translation/autoTranslate", autoTranslateCheck->isChecked());
    }
    if (translationEngineCombo) {
        settings.setValue("translation/engine", translationEngineCombo->currentText());
    }
    if (apiUrlEdit) {
        settings.setValue("translation/apiUrl", apiUrlEdit->text().trimmed());
    }
    if (apiKeyEdit) {
        settings.setValue("translation/apiKey", apiKeyEdit->text());
    }

    // Appearance
    if (themeCombo) {
        settings.setValue("appearance/theme", themeCombo->currentText());
    }
    if (widgetWidthSlider) {
        settings.setValue("appearance/widgetWidth", widgetWidthSlider->value());
    }

    // TTS - Save to both new QSettings AND AppSettings for compatibility
    if (ttsEnabledCheck) {
        settings.setValue("tts/enabled", ttsEnabledCheck->isChecked());
    }

    // Update AppSettings so ModernTTSManager can load the settings
    AppSettings::TTSConfig ttsConfig = AppSettings::instance().getTTSConfig();

    if (ttsProviderCombo) {
        QString providerStr = ttsProviderCombo->currentData().toString();
        settings.setValue("tts/provider", providerStr);

        // Map to old engine name for AppSettings
        if (providerStr == "edge-free") {
            ttsConfig.engine = "Microsoft Edge TTS";
        } else if (providerStr == "google-web") {
            ttsConfig.engine = "Google Web TTS";
        } else if (providerStr == "system") {
            ttsConfig.engine = "System";
        }
    }

    if (voiceCombo && voiceCombo->isEnabled() && voiceCombo->currentIndex() >= 0) {
        auto voiceData = voiceCombo->currentData();
        if (voiceData.canConvert<ModernTTSManager::VoiceInfo>()) {
            auto voice = voiceData.value<ModernTTSManager::VoiceInfo>();
            settings.setValue("tts/voiceId", voice.id);
            settings.setValue("tts/voiceName", voice.name);
            ttsConfig.voice = voice.id;  // Update AppSettings too
        }
    }

    // Save volume and speed from ModernTTSManager defaults
    auto ttsOptions = ModernTTSManager::instance().getDefaultOptions();
    ttsConfig.volume = ttsOptions.volume;
    ttsConfig.speed = ttsOptions.rate;

    // Save speak translation setting
    if (ttsSpeakTranslationCheck) {
        ttsConfig.speakTranslation = ttsSpeakTranslationCheck->isChecked();
        settings.setValue("tts/speakTranslation", ttsConfig.speakTranslation);
    }

    // Save word-by-word reading setting
    if (ttsWordByWordCheck) {
        ttsConfig.wordByWordReading = ttsWordByWordCheck->isChecked();
        settings.setValue("tts/wordByWord", ttsConfig.wordByWordReading);
    }

    AppSettings::instance().setTTSConfig(ttsConfig);
    qDebug() << "ModernSettingsWindow: Saved TTS config - Engine:" << ttsConfig.engine << "Voice:" << ttsConfig.voice;

    settings.sync();
}

void ModernSettingsWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    qDebug() << "ModernSettingsWindow: Window shown";
}

void ModernSettingsWindow::updateGnomeShortcuts()
{
#ifdef Q_OS_LINUX
    // Get the current shortcuts from the UI
    QString screenshotShortcut = screenshotShortcutEdit ? screenshotShortcutEdit->keySequence().toString() : "Ctrl+Alt+X";
    QString toggleShortcut = toggleShortcutEdit ? toggleShortcutEdit->keySequence().toString() : "Ctrl+Alt+H";

    // Convert Qt key sequence format to GNOME format
    // Qt format: "Ctrl+Alt+X" -> GNOME format: "<Ctrl><Alt>x"
    auto qtToGnome = [](const QString &qtSeq) -> QString {
        QString gnome = qtSeq.toLower();
        gnome.replace("ctrl", "<Ctrl>");
        gnome.replace("alt", "<Alt>");
        gnome.replace("shift", "<Shift>");
        gnome.replace("meta", "<Super>");
        gnome.remove("+");
        return gnome;
    };

    QString gnomeScreenshot = qtToGnome(screenshotShortcut);
    QString gnomeToggle = qtToGnome(toggleShortcut);

    // Get application path
    QString appPath = QCoreApplication::applicationFilePath();

    // Build gsettings commands
    QStringList commands;

    // Get existing custom keybindings
    QProcess getBindings;
    getBindings.start("gsettings", QStringList() << "get" << "org.gnome.settings-daemon.plugins.media-keys" << "custom-keybindings");
    getBindings.waitForFinished();
    QString existing = QString::fromUtf8(getBindings.readAll()).trimmed();

    // Find or create custom0 and custom1
    QString custom0 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/";
    QString custom1 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom1/";

    // Update the custom keybindings list if needed
    if (!existing.contains("custom0") || !existing.contains("custom1")) {
        commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings \"['%1', '%2']\"")
                    .arg(custom0).arg(custom1);
    }

    // Set screenshot shortcut (custom0)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Screenshot'").arg(custom0);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --screenshot'").arg(custom0).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom0).arg(gnomeScreenshot);

    // Set toggle shortcut (custom1)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Toggle Widget'").arg(custom1);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --toggle'").arg(custom1).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom1).arg(gnomeToggle);

    // Execute all commands
    bool success = true;
    for (const QString &cmd : commands) {
        int result = QProcess::execute("sh", QStringList() << "-c" << cmd);
        if (result != 0) {
            success = false;
            qWarning() << "Failed to execute:" << cmd;
        }
    }

    // Show feedback to user
    if (success) {
        QMessageBox::information(this, "Shortcuts Updated",
            QString("GNOME keyboard shortcuts have been updated:\n\n"
                    "Screenshot: %1\n"
                    "Toggle: %2\n\n"
                    "The shortcuts should work immediately.")
            .arg(screenshotShortcut)
            .arg(toggleShortcut));
    } else {
        QMessageBox::warning(this, "Update Failed",
            "Failed to update GNOME shortcuts. Please check the terminal for errors.\n\n"
            "You may need to configure them manually in GNOME Settings.");
    }
#endif
}

void ModernSettingsWindow::updateVoiceList()
{
    if (!voiceCombo || !ttsProviderCombo || !ocrLanguageCombo || !targetLanguageCombo) {
        return;
    }

    // Get selected provider
    QString providerStr = ttsProviderCombo->currentData().toString();

    // Map provider string to enum
    ModernTTSManager::TTSProvider selectedProvider = ModernTTSManager::TTSProvider::EdgeTTS;  // default to Edge TTS
    if (providerStr == "google-web") {
        selectedProvider = ModernTTSManager::TTSProvider::GoogleWeb;
    } else if (providerStr == "edge-free") {
        selectedProvider = ModernTTSManager::TTSProvider::EdgeTTS;
    } else if (providerStr == "system") {
        selectedProvider = ModernTTSManager::TTSProvider::SystemTTS;
    }

    // Determine which language to use based on toggle
    QString languageToUse;
    if (ttsSpeakTranslationCheck && ttsSpeakTranslationCheck->isChecked()) {
        // Speak translation - use target/output language
        languageToUse = targetLanguageCombo->currentText();
        qDebug() << "Voice list: Using OUTPUT language (translation):" << languageToUse;
    } else {
        // Speak original - use OCR/input language
        languageToUse = ocrLanguageCombo->currentText();
        qDebug() << "Voice list: Using INPUT language (original):" << languageToUse;
    }

    // Map language to QLocale
    QLocale::Language targetLang = QLocale::English;  // default

    if (languageToUse.contains("Chinese (Simplified)")) targetLang = QLocale::Chinese;
    else if (languageToUse.contains("Chinese (Traditional)")) targetLang = QLocale::Chinese;
    else if (languageToUse.contains("Japanese")) targetLang = QLocale::Japanese;
    else if (languageToUse.contains("Korean")) targetLang = QLocale::Korean;
    else if (languageToUse.contains("Spanish")) targetLang = QLocale::Spanish;
    else if (languageToUse.contains("French")) targetLang = QLocale::French;
    else if (languageToUse.contains("German")) targetLang = QLocale::German;
    else if (languageToUse.contains("Russian")) targetLang = QLocale::Russian;
    else if (languageToUse.contains("Portuguese")) targetLang = QLocale::Portuguese;
    else if (languageToUse.contains("Italian")) targetLang = QLocale::Italian;
    else if (languageToUse.contains("Dutch")) targetLang = QLocale::Dutch;
    else if (languageToUse.contains("Polish")) targetLang = QLocale::Polish;
    else if (languageToUse.contains("Swedish")) targetLang = QLocale::Swedish;
    else if (languageToUse.contains("Arabic")) targetLang = QLocale::Arabic;
    else if (languageToUse.contains("Hindi")) targetLang = QLocale::Hindi;
    else if (languageToUse.contains("Thai")) targetLang = QLocale::Thai;
    else if (languageToUse.contains("Vietnamese")) targetLang = QLocale::Vietnamese;

    // Save current selection (prefer saved voice ID from settings)
    QString savedVoiceId = settings.value("tts/voiceId", "").toString();
    QString currentVoiceName;
    if (voiceCombo->currentIndex() >= 0) {
        currentVoiceName = voiceCombo->currentText();
    }

    // Clear and repopulate
    voiceCombo->clear();

    // Get all voices and filter by provider AND language
    auto allVoices = ModernTTSManager::instance().availableVoices();
    int addedCount = 0;
    int restoreIndex = -1;

    for (const auto& voice : allVoices) {
        // Filter by provider and language
        if (voice.provider == selectedProvider && voice.locale.language() == targetLang) {
            voiceCombo->addItem(voice.name, QVariant::fromValue(voice));
            // Try to restore by ID first (more reliable), then by name
            if (!savedVoiceId.isEmpty() && voice.id == savedVoiceId) {
                restoreIndex = addedCount;
            } else if (savedVoiceId.isEmpty() && voice.name == currentVoiceName) {
                restoreIndex = addedCount;
            }
            addedCount++;
        }
    }

    if (addedCount == 0) {
        QString providerName;
        if (providerStr == "edge-free") {
            providerName = "Edge TTS";
        } else if (providerStr == "google-web") {
            providerName = "Google";
        } else {
            providerName = "System";
        }

        QString msg = QString("No %1 voices for %2").arg(providerName).arg(languageToUse);
        voiceCombo->addItem(msg);
        voiceCombo->setEnabled(false);
        if (testVoiceBtn) {
            testVoiceBtn->setEnabled(false);
        }
    } else {
        voiceCombo->setEnabled(true);
        if (testVoiceBtn) {
            testVoiceBtn->setEnabled(true);
        }
        // Restore previous selection if it exists, otherwise select first
        if (restoreIndex >= 0) {
            voiceCombo->setCurrentIndex(restoreIndex);
        } else if (addedCount > 0) {
            voiceCombo->setCurrentIndex(0);  // Auto-select first voice
        }
    }
}
