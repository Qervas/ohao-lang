#include "ModernSettingsWindow.h"
#include "SettingsPages.h"
#include "../core/AppSettings.h"
#include "../../tts/TTSEngine.h"
#include "../../tts/TTSManager.h"
#include "../../tts/ModernTTSManager.h"
#include "../../system/GlobalShortcutManager.h"
#include "../../system/SystemTray.h"
#include "../core/ThemeManager.h"
#include "../core/ThemeColors.h"
#include "../core/FloatingWidget.h"
#include "../core/LanguageManager.h"
#include <QApplication>
#include <QScreen>
#include <QGroupBox>
#include <QDebug>
#include <QCoreApplication>
#include <QMessageBox>
#include <QProcess>
#include <QClipboard>
#include <QSpinBox>
#include <QScrollArea>

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

    // Connect to AI settings changes to update token count display
    connect(&AppSettings::instance(), &AppSettings::aiSettingsChanged, this, [this]() {
        if (tokenUsageLabel) {
            auto config = AppSettings::instance().getAIConfig();
            tokenUsageLabel->setText(QString::number(config.totalTokensUsed));
        }
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
        "Voice",
        "Chat",
        "AI",
        "Help"
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

    // Create all pages using SettingsPages namespace
    contentStack->addWidget(SettingsPages::createGeneralPage(this, settings, shortcutManager, systemTray));
    contentStack->addWidget(SettingsPages::createOCRPage(this, settings));
    contentStack->addWidget(SettingsPages::createTranslationPage(this, settings));
    contentStack->addWidget(SettingsPages::createAppearancePage(this, settings));
    contentStack->addWidget(SettingsPages::createVoicePage(this, settings, ttsEngine));
    contentStack->addWidget(SettingsPages::createChatPage(this, settings));
    contentStack->addWidget(SettingsPages::createAIPage(this, settings));
    contentStack->addWidget(SettingsPages::createHelpPage());

    mainLayout->addWidget(contentStack, 1);
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

    // General - Shortcuts (use ShortcutConfig as single source of truth)
    const auto& shortcutConfig = ShortcutConfig::instance();

    if (screenshotShortcutEdit) {
        const auto& def = shortcutConfig.getShortcut(ShortcutConfig::Screenshot);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl"); // Qt displays Cmd as Ctrl
#endif
        screenshotShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (toggleShortcutEdit) {
        const auto& def = shortcutConfig.getShortcut(ShortcutConfig::ToggleVisibility);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        toggleShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (chatWindowShortcutEdit) {
        const auto& def = shortcutConfig.getShortcut(ShortcutConfig::ChatWindow);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        chatWindowShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (readAloudShortcutEdit) {
        const auto& def = shortcutConfig.getShortcut(ShortcutConfig::ReadAloud);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        readAloudShortcutEdit->setKeySequence(QKeySequence(shortcut));
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

    // General - Shortcuts - VALIDATE BEFORE SAVING
    auto validation = validateShortcuts();
    if (!validation.first) {
        // Show error dialog
        QMessageBox::critical(const_cast<ModernSettingsWindow*>(this),
                             "Invalid Shortcut Configuration",
                             validation.second);

        qWarning() << "Shortcut validation failed:" << validation.second;
        // Don't save shortcuts if validation fails, but continue saving other settings
    } else {
        // Validation passed - proceed with saving shortcuts
        if (screenshotShortcutEdit) {
            QString shortcut = screenshotShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
            shortcut.replace("Ctrl", "Meta"); // Store as Meta for Carbon API
#endif
            settings.setValue("shortcuts/screenshot", shortcut);
        }

        if (toggleShortcutEdit) {
            QString shortcut = toggleShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
            shortcut.replace("Ctrl", "Meta");
#endif
            settings.setValue("shortcuts/toggle", shortcut);
        }

        if (chatWindowShortcutEdit) {
            QString shortcut = chatWindowShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
            shortcut.replace("Ctrl", "Meta");
#endif
            settings.setValue("shortcuts/chatWindow", shortcut);
        }

        if (readAloudShortcutEdit) {
            QString shortcut = readAloudShortcutEdit->keySequence().toString();
#ifdef Q_OS_MACOS
            shortcut.replace("Ctrl", "Meta");
#endif
            settings.setValue("shortcuts/readAloud", shortcut);
        }

        // Only reload shortcuts if validation passed
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

QPair<bool, QString> ModernSettingsWindow::validateShortcuts() const
{
    if (!screenshotShortcutEdit || !toggleShortcutEdit ||
        !chatWindowShortcutEdit || !readAloudShortcutEdit) {
        return {true, ""};  // Can't validate if widgets don't exist
    }

    // Collect all shortcuts with their names
    QMap<QString, QStringList> shortcutMap;

    auto addShortcut = [&shortcutMap](const QString& key, const QString& name) {
        if (!key.isEmpty()) {
            shortcutMap[key].append(name);
        }
    };

    addShortcut(screenshotShortcutEdit->keySequence().toString(), "Take Screenshot");
    addShortcut(toggleShortcutEdit->keySequence().toString(), "Toggle Widget");
    addShortcut(chatWindowShortcutEdit->keySequence().toString(), "Chat Window");
    addShortcut(readAloudShortcutEdit->keySequence().toString(), "Read Aloud");

    // Check for duplicates
    for (auto it = shortcutMap.begin(); it != shortcutMap.end(); ++it) {
        if (it.value().size() > 1) {
            QString duplicateNames = it.value().join(", ");
            QString errorMsg = QString("Duplicate shortcut detected!\n\n"
                                      "Key combination: %1\n"
                                      "Used by: %2\n\n"
                                      "Each shortcut must have a unique key combination.")
                                .arg(it.key())
                                .arg(duplicateNames);
            return {false, errorMsg};
        }
    }

    return {true, ""};
}

void ModernSettingsWindow::resetShortcutsToDefaults()
{
    const auto& config = ShortcutConfig::instance();

    // Remove all shortcut settings (will use defaults)
    settings.remove("shortcuts/screenshot");
    settings.remove("shortcuts/toggle");
    settings.remove("shortcuts/chatWindow");
    settings.remove("shortcuts/readAloud");
    settings.sync();

    // Update UI with defaults
    if (screenshotShortcutEdit) {
        QString shortcut = config.getShortcut(ShortcutConfig::Screenshot).defaultShortcut;
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        screenshotShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (toggleShortcutEdit) {
        QString shortcut = config.getShortcut(ShortcutConfig::ToggleVisibility).defaultShortcut;
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        toggleShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (chatWindowShortcutEdit) {
        QString shortcut = config.getShortcut(ShortcutConfig::ChatWindow).defaultShortcut;
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        chatWindowShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    if (readAloudShortcutEdit) {
        QString shortcut = config.getShortcut(ShortcutConfig::ReadAloud).defaultShortcut;
#ifdef Q_OS_MACOS
        shortcut.replace("Meta", "Ctrl");
#endif
        readAloudShortcutEdit->setKeySequence(QKeySequence(shortcut));
    }

    // Reload shortcuts in the manager
    if (shortcutManager) {
        shortcutManager->reloadShortcuts();
    }
    if (systemTray) {
        systemTray->updateShortcutLabels();
    }

    qInfo() << "Shortcuts reset to defaults";

    QMessageBox::information(this, "Reset Complete",
        "All shortcuts have been reset to their default values.");
}

void ModernSettingsWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    qDebug() << "ModernSettingsWindow: Window shown";
}

void ModernSettingsWindow::updateGnomeShortcuts()
{
#ifdef Q_OS_LINUX
    // Get the current shortcuts from the UI (use ShortcutConfig for fallback defaults)
    const auto& shortcutConfig = ShortcutConfig::instance();
    QString screenshotShortcut = screenshotShortcutEdit ? screenshotShortcutEdit->keySequence().toString() : shortcutConfig.getShortcut(ShortcutConfig::Screenshot).defaultShortcut;
    QString toggleShortcut = toggleShortcutEdit ? toggleShortcutEdit->keySequence().toString() : shortcutConfig.getShortcut(ShortcutConfig::ToggleVisibility).defaultShortcut;
    QString chatWindowShortcut = chatWindowShortcutEdit ? chatWindowShortcutEdit->keySequence().toString() : shortcutConfig.getShortcut(ShortcutConfig::ChatWindow).defaultShortcut;
    QString readAloudShortcut = readAloudShortcutEdit ? readAloudShortcutEdit->keySequence().toString() : shortcutConfig.getShortcut(ShortcutConfig::ReadAloud).defaultShortcut;

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
    QString gnomeChatWindow = qtToGnome(chatWindowShortcut);
    QString gnomeReadAloud = qtToGnome(readAloudShortcut);

    // Get application path
    QString appPath = QCoreApplication::applicationFilePath();

    // Build gsettings commands
    QStringList commands;

    // Get existing custom keybindings
    QProcess getBindings;
    getBindings.start("gsettings", QStringList() << "get" << "org.gnome.settings-daemon.plugins.media-keys" << "custom-keybindings");
    getBindings.waitForFinished();
    QString existing = QString::fromUtf8(getBindings.readAll()).trimmed();

    // Find or create custom0, custom1, custom2, and custom3
    QString custom0 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/";
    QString custom1 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom1/";
    QString custom2 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom2/";
    QString custom3 = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom3/";

    // Update the custom keybindings list if needed
    if (!existing.contains("custom0") || !existing.contains("custom1") || !existing.contains("custom2") || !existing.contains("custom3")) {
        commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings \"['%1', '%2', '%3', '%4']\"")
                    .arg(custom0).arg(custom1).arg(custom2).arg(custom3);
    }

    // Set screenshot shortcut (custom0)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Screenshot'").arg(custom0);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --screenshot'").arg(custom0).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom0).arg(gnomeScreenshot);

    // Set toggle shortcut (custom1)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Toggle Widget'").arg(custom1);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --toggle'").arg(custom1).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom1).arg(gnomeToggle);

    // Set chat window shortcut (custom2)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Toggle Chat'").arg(custom2);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --chat'").arg(custom2).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom2).arg(gnomeChatWindow);

    // Set read aloud shortcut (custom3)
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 name 'Ohao Read Aloud'").arg(custom3);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 command '%2 --read-aloud'").arg(custom3).arg(appPath);
    commands << QString("gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:%1 binding '%2'").arg(custom3).arg(gnomeReadAloud);

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
                    "Toggle: %2\n"
                    "Chat Window: %3\n"
                    "Read Aloud: %4\n\n"
                    "The shortcuts should work immediately.")
            .arg(screenshotShortcut)
            .arg(toggleShortcut)
            .arg(chatWindowShortcut)
            .arg(readAloudShortcut));
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
