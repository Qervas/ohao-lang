#include "SettingsPages.h"
#include "ModernSettingsWindow.h"
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
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace SettingsPages
{

QWidget* createGeneralPage(ModernSettingsWindow* parent,
                            QSettings& settings,
                            GlobalShortcutManager* shortcutManager,
                            SystemTray* systemTray)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    parent->ocrLanguageCombo = new QComboBox();
    // Populate from LanguageManager - the single source of truth
    QStringList ocrLanguages;
    for (const auto& lang : LanguageManager::instance().allLanguages()) {
        ocrLanguages << lang.englishName;  // Use English names for consistency
    }
    parent->ocrLanguageCombo->addItems(ocrLanguages);
    QObject::connect(parent->ocrLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            parent, [parent]() {
                parent->updateVoiceList();  // Update voices when OCR language changes
                parent->onSettingChanged();
            });
    langLayout->addRow("OCR Language:", parent->ocrLanguageCombo);

    parent->targetLanguageCombo = new QComboBox();
    // Populate from LanguageManager - the single source of truth
    QStringList translationLanguages;
    for (const auto& lang : LanguageManager::instance().allLanguages()) {
        translationLanguages << lang.englishName;  // Use English names for consistency
    }
    parent->targetLanguageCombo->addItems(translationLanguages);
    QObject::connect(parent->targetLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            parent, &ModernSettingsWindow::onSettingChanged);
    langLayout->addRow("Translation Target:", parent->targetLanguageCombo);

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

    parent->screenshotDimmingSlider = new QSlider(Qt::Horizontal);
    parent->screenshotDimmingSlider->setRange(30, 220);
    parent->screenshotDimmingSlider->setValue(120);
    parent->dimmingValueLabel = new QLabel("47%");
    parent->dimmingValueLabel->setMinimumWidth(40);
    parent->dimmingValueLabel->setAlignment(Qt::AlignRight);

    QObject::connect(parent->screenshotDimmingSlider, &QSlider::valueChanged, parent, [parent](int value) {
        if (parent->dimmingValueLabel) {
            int percentage = (value * 100) / 255;
            parent->dimmingValueLabel->setText(QString("%1%").arg(percentage));
        }
        parent->saveSettings();
    });

    dimmingLayout->addWidget(parent->screenshotDimmingSlider);
    dimmingLayout->addWidget(parent->dimmingValueLabel);

    screenshotLayout->addRow("Screen Dimming:", dimmingWidget);

    layout->addWidget(screenshotGroup);

    // Shortcuts group
    QGroupBox *shortcutsGroup = new QGroupBox("Global Shortcuts");
    shortcutsGroup->setObjectName("settingsGroup");
    QFormLayout *shortcutsLayout = new QFormLayout(shortcutsGroup);
    shortcutsLayout->setSpacing(12);
    shortcutsLayout->setContentsMargins(0, 0, 0, 0);

    // Helper lambda: creates a shortcut row with a display label and "Set" button.
    // Clicking "Set" opens a modal dialog to capture the new key combination,
    // preventing accidental shortcut changes from stray keypresses.
    auto createShortcutRow = [parent, shortcutsLayout](
            const QString &label, QKeySequenceEdit *&editPtr) {
        editPtr = new QKeySequenceEdit();
        editPtr->setVisible(false); // Hidden - only used as data holder

        QHBoxLayout *row = new QHBoxLayout();
        QLabel *display = new QLabel();
        display->setMinimumWidth(160);
        display->setStyleSheet("padding: 6px 10px; border: 1px solid palette(mid); border-radius: 6px; background: palette(base);");

        QPushButton *setBtn = new QPushButton("Set...");
        setBtn->setMaximumWidth(70);
        setBtn->setCursor(Qt::PointingHandCursor);
        setBtn->setToolTip("Click to set a new shortcut");

        row->addWidget(display, 1);
        row->addWidget(setBtn);

        // Sync display with the hidden QKeySequenceEdit
        auto updateDisplay = [display, editPtr]() {
            QString seq = editPtr->keySequence().toString(QKeySequence::NativeText);
            display->setText(seq.isEmpty() ? "(none)" : seq);
        };

        QObject::connect(editPtr, &QKeySequenceEdit::keySequenceChanged,
                         parent, [updateDisplay, parent]() {
            updateDisplay();
            parent->onSettingChanged();
        });

        // "Set" button opens a modal dialog
        QObject::connect(setBtn, &QPushButton::clicked, parent, [parent, editPtr, label, updateDisplay]() {
            QDialog dialog(parent);
            dialog.setWindowTitle("Set Shortcut");
            dialog.setFixedSize(350, 160);
            dialog.setModal(true);

            QVBoxLayout *dlgLayout = new QVBoxLayout(&dialog);
            dlgLayout->setSpacing(12);
            dlgLayout->setContentsMargins(20, 20, 20, 20);

            QLabel *prompt = new QLabel(QString("Press your desired key combination for:\n<b>%1</b>").arg(label));
            prompt->setTextFormat(Qt::RichText);
            prompt->setWordWrap(true);
            dlgLayout->addWidget(prompt);

            QKeySequenceEdit *capture = new QKeySequenceEdit(&dialog);
            capture->setKeySequence(editPtr->keySequence());
            dlgLayout->addWidget(capture);

            // Auto-focus the capture field
            capture->setFocus();

            QHBoxLayout *btnLayout = new QHBoxLayout();
            btnLayout->addStretch();
            QPushButton *okBtn = new QPushButton("OK");
            QPushButton *cancelBtn = new QPushButton("Cancel");
            okBtn->setDefault(true);
            btnLayout->addWidget(cancelBtn);
            btnLayout->addWidget(okBtn);
            dlgLayout->addLayout(btnLayout);

            QObject::connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
            QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

            if (dialog.exec() == QDialog::Accepted) {
                QKeySequence newSeq = capture->keySequence();
                if (!newSeq.isEmpty()) {
                    editPtr->setKeySequence(newSeq);
                }
            }
        });

        shortcutsLayout->addRow(label + ":", row);
    };

    createShortcutRow("Take Screenshot", parent->screenshotShortcutEdit);
    createShortcutRow("Toggle Widget", parent->toggleShortcutEdit);
    createShortcutRow("Toggle Chat Window", parent->chatWindowShortcutEdit);
    createShortcutRow("Read Selected Text Aloud", parent->readAloudShortcutEdit);

    // Add Reset to Defaults button
    QPushButton *resetShortcutsBtn = new QPushButton("Reset to Defaults");
    resetShortcutsBtn->setMaximumWidth(150);
    resetShortcutsBtn->setToolTip("Reset all shortcuts to their default values");
    QObject::connect(resetShortcutsBtn, &QPushButton::clicked, parent, [parent]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            parent,
            "Reset Shortcuts",
            "Reset all shortcuts to their default values?\n\n"
            "This will change:\n"
            "• Screenshot: Ctrl+Alt+X\n"
            "• Toggle Widget: Ctrl+Alt+H\n"
            "• Chat Window: Ctrl+Alt+C\n"
            "• Read Aloud: Ctrl+Alt+A",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            parent->resetShortcutsToDefaults();
        }
    });
    shortcutsLayout->addRow("", resetShortcutsBtn);

#ifdef Q_OS_LINUX
    // Add button to update GNOME shortcuts
    QPushButton *updateGnomeBtn = new QPushButton("Update GNOME Shortcuts");
    updateGnomeBtn->setToolTip("Apply these shortcuts to GNOME keyboard settings");
    QObject::connect(updateGnomeBtn, &QPushButton::clicked, parent, [parent]() {
        parent->updateGnomeShortcuts();
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
    QObject::connect(alwaysOnTopCheck, &QCheckBox::toggled, parent, [parent, &settings](bool checked) {
        settings.setValue("window/alwaysOnTop", checked);

        // Apply to FloatingWidget immediately
        FloatingWidget *floatingWidget = qobject_cast<FloatingWidget*>(parent->parent());
        if (floatingWidget) {
            floatingWidget->setAlwaysOnTop(checked);
        }

        parent->saveSettings();
    });
    behaviorLayout->addWidget(alwaysOnTopCheck);

    layout->addWidget(behaviorGroup);

    layout->addStretch();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createOCRPage(ModernSettingsWindow* parent, QSettings& settings)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    parent->ocrEngineCombo = new QComboBox();
#ifdef Q_OS_MACOS
    parent->ocrEngineCombo->addItems({"Apple Vision (Recommended)", "Tesseract"});
#else
    // Windows and Linux both use Tesseract
    parent->ocrEngineCombo->addItems({"Tesseract"});
#endif
    QObject::connect(parent->ocrEngineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            parent, &ModernSettingsWindow::onSettingChanged);
    engineLayout->addRow("Engine:", parent->ocrEngineCombo);

    layout->addWidget(engineGroup);
    layout->addStretch();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createTranslationPage(ModernSettingsWindow* parent, QSettings& settings)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    parent->autoTranslateCheck = new QCheckBox("Automatically translate after OCR");
    parent->autoTranslateCheck->setChecked(true);
    parent->autoTranslateCheck->setStyleSheet("padding: 4px 0px;");
    QObject::connect(parent->autoTranslateCheck, &QCheckBox::toggled,
            parent, &ModernSettingsWindow::onSettingChanged);
    optionsLayout->addWidget(parent->autoTranslateCheck);

    layout->addWidget(optionsGroup);

    // Engine group - Only Google Translate (free)
    QGroupBox *engineGroup = new QGroupBox("Translation Engine");
    engineGroup->setObjectName("settingsGroup");
    QFormLayout *engineLayout = new QFormLayout(engineGroup);
    engineLayout->setSpacing(12);
    engineLayout->setContentsMargins(0, 0, 0, 0);

    parent->translationEngineCombo = new QComboBox();
    parent->translationEngineCombo->addItems({"Google Translate (Free)"});
    parent->translationEngineCombo->setEnabled(false); // Only one option, disable selection
    QObject::connect(parent->translationEngineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), parent, [parent](int index) {
        parent->saveSettings();
    });
    engineLayout->addRow("Engine:", parent->translationEngineCombo);

    // API fields removed - Google Translate doesn't need API key or custom URL
    parent->apiUrlEdit = nullptr;
    parent->apiKeyEdit = nullptr;

    layout->addWidget(engineGroup);
    layout->addStretch();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createAppearancePage(ModernSettingsWindow* parent, QSettings& settings)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    parent->themeCombo = new QComboBox();
    parent->themeCombo->addItems({"Auto (System)", "Light", "Dark", "Cyberpunk"});
    QObject::connect(parent->themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), parent, [parent](int) {
        if (parent->themeCombo) {
            QString selectedTheme = parent->themeCombo->currentText();
            parent->applyTheme(selectedTheme);
            ThemeManager::instance().applyTheme(ThemeManager::fromString(selectedTheme));
            parent->saveSettings();
        }
    });
    themeLayout->addRow("Theme:", parent->themeCombo);

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

    parent->widgetWidthSlider = new QSlider(Qt::Horizontal);
    parent->widgetWidthSlider->setRange(100, 250);
    parent->widgetWidthSlider->setValue(140);

    parent->widthValueLabel = new QLabel("140 px");
    parent->widthValueLabel->setMinimumWidth(60);
    parent->widthValueLabel->setAlignment(Qt::AlignRight);

    QObject::connect(parent->widgetWidthSlider, &QSlider::valueChanged, parent, [parent](int width) {
        if (parent->widthValueLabel) {
            parent->widthValueLabel->setText(QString("%1 px").arg(width));
        }

        // Update floating widget size in real-time
        FloatingWidget *floatingWidget = qobject_cast<FloatingWidget*>(parent->parent());
        if (floatingWidget) {
            int height = static_cast<int>(width * 0.43);
            floatingWidget->setFixedSize(width, height);
        }
        parent->saveSettings();
    });

    widthLayout->addWidget(parent->widgetWidthSlider);
    widthLayout->addWidget(parent->widthValueLabel);

    sizeLayout->addRow("Width:", widthWidget);

    layout->addWidget(sizeGroup);
    layout->addStretch();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createVoicePage(ModernSettingsWindow* parent,
                          QSettings& settings,
                          TTSEngine* ttsEngine)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    parent->ttsEnabledCheck = new QCheckBox("Enable text-to-speech");
    parent->ttsEnabledCheck->setChecked(true);
    QObject::connect(parent->ttsEnabledCheck, &QCheckBox::toggled,
            parent, &ModernSettingsWindow::onSettingChanged);
    enableLayout->addWidget(parent->ttsEnabledCheck);

    layout->addWidget(enableGroup);

    // Provider group
    QGroupBox *providerGroup = new QGroupBox("Voice Provider");
    providerGroup->setObjectName("settingsGroup");
    QFormLayout *providerLayout = new QFormLayout(providerGroup);
    providerLayout->setSpacing(12);
    providerLayout->setContentsMargins(0, 0, 0, 0);

    parent->ttsProviderCombo = new QComboBox();
    parent->ttsProviderCombo->addItem("Microsoft Edge TTS (Recommended - Fast & High Quality)", QStringLiteral("edge-free"));
#ifdef QT_TEXTTOSPEECH_AVAILABLE
    parent->ttsProviderCombo->addItem("System Voices (Offline)", QStringLiteral("system"));
#endif
    parent->ttsProviderCombo->addItem("Google Web TTS (Fast but Basic)", QStringLiteral("google-web"));

    QObject::connect(parent->ttsProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            parent, [parent]() {
                // Show loading message
                if (parent->voiceCombo) {
                    parent->voiceCombo->clear();
                    parent->voiceCombo->addItem("Loading voices...");
                    parent->voiceCombo->setEnabled(false);
                }

                // Process events to show the loading message
                QCoreApplication::processEvents();

                // Update voice list (this might take time for detection)
                parent->updateVoiceList();
                parent->onSettingChanged();
            });
    providerLayout->addRow("Provider:", parent->ttsProviderCombo);

    parent->providerInfoLabel = new QLabel();
    parent->providerInfoLabel->setWordWrap(true);
    parent->providerInfoLabel->setStyleSheet("color: #86868B; font-size: 11px;");
    providerLayout->addRow("", parent->providerInfoLabel);

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

    parent->voiceCombo = new QComboBox();
    parent->voiceCombo->setEditable(false);
    QObject::connect(parent->voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            parent, &ModernSettingsWindow::onSettingChanged);
    voiceLayout->addWidget(parent->voiceCombo, 1);

    parent->testVoiceBtn = new QPushButton("Test");
    parent->testVoiceBtn->setFixedWidth(60);
    parent->testVoiceBtn->setEnabled(parent->voiceCombo->count() > 0);
    QObject::connect(parent->testVoiceBtn, &QPushButton::clicked, parent, [parent]() {
        if (parent->voiceCombo->currentIndex() >= 0) {
            auto voiceInfo = parent->voiceCombo->currentData().value<ModernTTSManager::VoiceInfo>();
            qDebug() << "Testing voice:" << voiceInfo.name;
            ModernTTSManager::instance().testVoice(voiceInfo);
        }
    });
    voiceLayout->addWidget(parent->testVoiceBtn);

    voicesLayout->addRow("Voice:", voiceWidget);

    layout->addWidget(voicesGroup);

    // Speech Language group - iOS-style toggle
    QGroupBox *speechGroup = new QGroupBox("Speech Language");
    speechGroup->setObjectName("settingsGroup");
    QVBoxLayout *speechLayout = new QVBoxLayout(speechGroup);
    speechLayout->setSpacing(8);
    speechLayout->setContentsMargins(0, 0, 0, 0);

    parent->ttsSpeakTranslationCheck = new QCheckBox("Speak Translated Text (Output Language)");
    parent->ttsSpeakTranslationCheck->setChecked(false);  // Default: speak original/input language
    parent->ttsSpeakTranslationCheck->setToolTip("When enabled: TTS speaks the translated text in the target language\nWhen disabled: TTS speaks the original text in the source language");
    QObject::connect(parent->ttsSpeakTranslationCheck, &QCheckBox::toggled,
            parent, [parent](bool checked) {
                // Update voice list to show voices for appropriate language
                parent->updateVoiceList();
                parent->onSettingChanged();
            });
    speechLayout->addWidget(parent->ttsSpeakTranslationCheck);

    parent->ttsWordByWordCheck = new QCheckBox("Word-by-Word Reading (Slower)");
    parent->ttsWordByWordCheck->setChecked(false);  // Default: normal reading
    parent->ttsWordByWordCheck->setToolTip("Add extra space between words for slower, clearer pronunciation");
    QObject::connect(parent->ttsWordByWordCheck, &QCheckBox::toggled, parent, &ModernSettingsWindow::onSettingChanged);
    speechLayout->addWidget(parent->ttsWordByWordCheck);

    QLabel *speechInfo = new QLabel("Choose whether TTS speaks the original text or the translation");
    speechInfo->setWordWrap(true);
    speechInfo->setStyleSheet("color: #86868B; font-size: 11px; padding-left: 24px;");
    speechLayout->addWidget(speechInfo);

    layout->addWidget(speechGroup);
    layout->addStretch();

    // Populate voices for the current provider
    parent->updateVoiceList();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createChatPage(ModernSettingsWindow* parent, QSettings& settings)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // Title
    QLabel *titleLabel = new QLabel("Translation Chat Settings");
    titleLabel->setObjectName("pageTitle");
    layout->addWidget(titleLabel);

    // Chat Settings Group
    QGroupBox *chatGroup = new QGroupBox("💬 Chat Window");
    chatGroup->setObjectName("settingsGroup");
    QVBoxLayout *chatLayout = new QVBoxLayout();
    chatLayout->setSpacing(12);

    // Enabled checkbox
    QHBoxLayout *enabledRow = new QHBoxLayout();
    QLabel *enabledLabel = new QLabel("Enable Chat Feature:");
    enabledLabel->setMinimumWidth(140);
    QCheckBox *enabledCheck = new QCheckBox();
    enabledCheck->setChecked(AppSettings::instance().getChatConfig().enabled);
    QObject::connect(enabledCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getChatConfig();
        config.enabled = checked;
        AppSettings::instance().setChatConfig(config);
    });
    enabledRow->addWidget(enabledLabel);
    enabledRow->addWidget(enabledCheck);
    enabledRow->addStretch();
    chatLayout->addLayout(enabledRow);

    // Opacity slider
    QHBoxLayout *opacityRow = new QHBoxLayout();
    QLabel *opacityLabel = new QLabel("Window Opacity:");
    opacityLabel->setMinimumWidth(140);
    QSlider *opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(50, 100);
    opacitySlider->setValue(AppSettings::instance().getChatConfig().opacity);
    QLabel *opacityValue = new QLabel(QString::number(opacitySlider->value()) + "%");
    opacityValue->setMinimumWidth(50);
    QObject::connect(opacitySlider, &QSlider::valueChanged, parent, [opacityValue](int value) {
        opacityValue->setText(QString::number(value) + "%");
        auto config = AppSettings::instance().getChatConfig();
        config.opacity = value;
        AppSettings::instance().setChatConfig(config);
    });
    opacityRow->addWidget(opacityLabel);
    opacityRow->addWidget(opacitySlider);
    opacityRow->addWidget(opacityValue);
    chatLayout->addLayout(opacityRow);

    // Font size slider
    QHBoxLayout *fontRow = new QHBoxLayout();
    QLabel *fontLabel = new QLabel("Font Size:");
    fontLabel->setMinimumWidth(140);
    QSlider *fontSlider = new QSlider(Qt::Horizontal);
    fontSlider->setRange(10, 18);
    fontSlider->setValue(AppSettings::instance().getChatConfig().fontSize);
    QLabel *fontValue = new QLabel(QString::number(fontSlider->value()) + "px");
    fontValue->setMinimumWidth(50);
    QObject::connect(fontSlider, &QSlider::valueChanged, parent, [fontValue](int value) {
        fontValue->setText(QString::number(value) + "px");
        auto config = AppSettings::instance().getChatConfig();
        config.fontSize = value;
        AppSettings::instance().setChatConfig(config);
    });
    fontRow->addWidget(fontLabel);
    fontRow->addWidget(fontSlider);
    fontRow->addWidget(fontValue);
    chatLayout->addLayout(fontRow);

    // Keep on top checkbox
    QHBoxLayout *topRow = new QHBoxLayout();
    QLabel *topLabel = new QLabel("Keep Window On Top:");
    topLabel->setMinimumWidth(140);
    QCheckBox *topCheck = new QCheckBox();
    topCheck->setChecked(AppSettings::instance().getChatConfig().keepOnTop);
    QObject::connect(topCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getChatConfig();
        config.keepOnTop = checked;
        AppSettings::instance().setChatConfig(config);
    });
    topRow->addWidget(topLabel);
    topRow->addWidget(topCheck);
    topRow->addStretch();
    chatLayout->addLayout(topRow);

    // Auto clear history checkbox
    QHBoxLayout *clearRow = new QHBoxLayout();
    QLabel *clearLabel = new QLabel("Auto Clear History:");
    clearLabel->setMinimumWidth(140);
    QCheckBox *clearCheck = new QCheckBox();
    clearCheck->setChecked(AppSettings::instance().getChatConfig().autoClearHistory);
    QObject::connect(clearCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getChatConfig();
        config.autoClearHistory = checked;
        AppSettings::instance().setChatConfig(config);
    });
    clearRow->addWidget(clearLabel);
    clearRow->addWidget(clearCheck);
    clearRow->addStretch();
    chatLayout->addLayout(clearRow);

    chatGroup->setLayout(chatLayout);
    layout->addWidget(chatGroup);

    // Info section
    QGroupBox *infoGroup = new QGroupBox("ℹ️  About Translation Chat");
    infoGroup->setObjectName("settingsGroup");
    QVBoxLayout *infoLayout = new QVBoxLayout();
    QLabel *infoText = new QLabel(
        "Translation Chat provides a quick way to translate between your OCR language and target language.\n\n"
        "• Type in either language and get instant translation\n"
        "• Bidirectional: automatically detects which direction to translate\n"
        "• Keeps conversation history for reference\n"
        "• Draggable window - click and drag to reposition\n\n"
        "Languages used: OCR Language ↔ Translation Target"
    );
    infoText->setWordWrap(true);
    infoLayout->addWidget(infoText);
    infoGroup->setLayout(infoLayout);
    layout->addWidget(infoGroup);

    layout->addStretch();

    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createAIPage(ModernSettingsWindow* parent, QSettings& settings)
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Create content widget
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // Title
    QLabel *titleLabel = new QLabel("AI Assistant Settings");
    titleLabel->setObjectName("pageTitle");
    layout->addWidget(titleLabel);

    // AI Settings Group
    QGroupBox *aiGroup = new QGroupBox("🤖 AI Configuration");
    aiGroup->setObjectName("settingsGroup");
    QFormLayout *aiForm = new QFormLayout(aiGroup);
    aiForm->setSpacing(12);
    aiForm->setContentsMargins(0, 0, 0, 0);

    // Enabled checkbox
    QCheckBox *enabledCheck = new QCheckBox();
    enabledCheck->setChecked(AppSettings::instance().getAIConfig().enabled);
    QObject::connect(enabledCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getAIConfig();
        config.enabled = checked;
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("Enable AI Assistant:", enabledCheck);

    // Provider preset dropdown
    QComboBox *providerCombo = new QComboBox();
    providerCombo->addItem("Groq (Recommended)", "Groq");
    providerCombo->addItem("OpenAI", "OpenAI");
    providerCombo->addItem("Ollama (Local)", "Ollama");
    providerCombo->addItem("LM Studio (Local)", "LM Studio");
    providerCombo->addItem("Custom Endpoint", "Custom");

    // Set current provider
    QString currentProvider = AppSettings::instance().getAIConfig().provider;
    int providerIndex = providerCombo->findData(currentProvider);
    if (providerIndex >= 0) {
        providerCombo->setCurrentIndex(providerIndex);
    }

    // API URL
    QLineEdit *urlEdit = new QLineEdit();
    urlEdit->setText(AppSettings::instance().getAIConfig().apiUrl);
    urlEdit->setPlaceholderText("https://api.groq.com/openai");

    // API Key
    QLineEdit *apiKeyEdit = new QLineEdit();
    apiKeyEdit->setText(AppSettings::instance().getAIConfig().apiKey);
    apiKeyEdit->setPlaceholderText("Enter your API key...");
    apiKeyEdit->setEchoMode(QLineEdit::Password);

    // Show/hide API key toggle
    QHBoxLayout *apiKeyRow = new QHBoxLayout();
    QPushButton *toggleKeyVisibility = new QPushButton("Show");
    toggleKeyVisibility->setMaximumWidth(60);
    toggleKeyVisibility->setCheckable(true);
    QObject::connect(toggleKeyVisibility, &QPushButton::toggled, parent, [apiKeyEdit, toggleKeyVisibility](bool checked) {
        apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        toggleKeyVisibility->setText(checked ? "Hide" : "Show");
    });
    apiKeyRow->addWidget(apiKeyEdit, 1);
    apiKeyRow->addWidget(toggleKeyVisibility);

    // Model - editable combo box so users can type custom model names
    QHBoxLayout *modelRow = new QHBoxLayout();
    QComboBox *modelCombo = new QComboBox();
    modelCombo->setEditable(true);
    modelCombo->setInsertPolicy(QComboBox::NoInsert);

    // Helper to populate models based on provider
    auto populateModels = [modelCombo](const QString &provider) {
        QString currentModel = modelCombo->currentText();
        modelCombo->clear();

        if (provider == "Groq") {
            modelCombo->addItems({
                "llama-3.3-70b-versatile",
                "llama-3.1-8b-instant",
                "llama-guard-3-8b",
                "mixtral-8x7b-32768",
                "gemma2-9b-it"
            });
        } else if (provider == "OpenAI") {
            modelCombo->addItems({
                "gpt-4o",
                "gpt-4o-mini",
                "gpt-4-turbo",
                "gpt-3.5-turbo",
                "o3-mini"
            });
        } else if (provider == "Ollama") {
            modelCombo->addItems({
                "llama3.2",
                "llama3.1",
                "mistral",
                "mixtral",
                "phi3",
                "gemma2",
                "qwen2.5"
            });
        } else if (provider == "LM Studio") {
            // LM Studio models are user-loaded, just provide a placeholder
            modelCombo->addItems({
                "local-model"
            });
        }
        // For Custom or if old model not in list, keep the text editable
        // Try to restore previous model if it exists in new list
        int idx = modelCombo->findText(currentModel);
        if (idx >= 0) {
            modelCombo->setCurrentIndex(idx);
        }
    };

    // Provider change handler - auto-fill URL, models, and key hint
    QObject::connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     parent, [providerCombo, urlEdit, apiKeyEdit, modelCombo, populateModels](int) {
        QString provider = providerCombo->currentData().toString();
        auto config = AppSettings::instance().getAIConfig();
        config.provider = provider;

        if (provider == "Groq") {
            urlEdit->setText("https://api.groq.com/openai");
            urlEdit->setPlaceholderText("https://api.groq.com/openai");
            apiKeyEdit->setPlaceholderText("gsk_...");
            config.apiUrl = "https://api.groq.com/openai";
        } else if (provider == "OpenAI") {
            urlEdit->setText("https://api.openai.com");
            urlEdit->setPlaceholderText("https://api.openai.com");
            apiKeyEdit->setPlaceholderText("sk-...");
            config.apiUrl = "https://api.openai.com";
        } else if (provider == "Ollama") {
            urlEdit->setText("http://localhost:11434");
            urlEdit->setPlaceholderText("http://localhost:11434");
            apiKeyEdit->setPlaceholderText("(not required for Ollama)");
            config.apiUrl = "http://localhost:11434";
        } else if (provider == "LM Studio") {
            urlEdit->setText("http://localhost:1234");
            urlEdit->setPlaceholderText("http://localhost:1234");
            apiKeyEdit->setPlaceholderText("(not required for LM Studio)");
            config.apiUrl = "http://localhost:1234";
        } else {
            urlEdit->setPlaceholderText("https://your-api-endpoint.com");
            apiKeyEdit->setPlaceholderText("Enter your API key...");
        }

        populateModels(provider);
        if (modelCombo->count() > 0) {
            config.model = modelCombo->currentText();
        }

        AppSettings::instance().setAIConfig(config);
    });

    aiForm->addRow("Provider:", providerCombo);

    // API URL row
    QObject::connect(urlEdit, &QLineEdit::editingFinished, parent, [urlEdit]() {
        auto config = AppSettings::instance().getAIConfig();
        config.apiUrl = urlEdit->text();
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("API Endpoint:", urlEdit);

    // API Key row
    QObject::connect(apiKeyEdit, &QLineEdit::editingFinished, parent, [apiKeyEdit]() {
        auto config = AppSettings::instance().getAIConfig();
        config.apiKey = apiKeyEdit->text();
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("API Key:", apiKeyRow);

    // Populate initial models
    populateModels(currentProvider);
    // Set current model text (works because combo is editable)
    modelCombo->setCurrentText(AppSettings::instance().getAIConfig().model);

    QObject::connect(modelCombo, &QComboBox::currentTextChanged, parent, [](const QString &model) {
        if (!model.isEmpty()) {
            auto config = AppSettings::instance().getAIConfig();
            config.model = model;
            AppSettings::instance().setAIConfig(config);
        }
    });

    // Refresh models button - fetches from /v1/models
    QPushButton *refreshModelsBtn = new QPushButton("Refresh");
    refreshModelsBtn->setMaximumWidth(80);
    refreshModelsBtn->setToolTip("Fetch available models from API (GET /v1/models)");
    QObject::connect(refreshModelsBtn, &QPushButton::clicked, parent, [parent, modelCombo]() {
        auto config = AppSettings::instance().getAIConfig();
        QString url = config.apiUrl + "/v1/models";

        QNetworkAccessManager *mgr = new QNetworkAccessManager(parent);
        QUrl reqUrl(url);
        QNetworkRequest req(reqUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!config.apiKey.isEmpty()) {
            req.setRawHeader("Authorization", QString("Bearer %1").arg(config.apiKey).toUtf8());
        }

        QNetworkReply *reply = mgr->get(req);
        QObject::connect(reply, &QNetworkReply::finished, parent, [reply, modelCombo, mgr]() {
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonArray data = doc.object()["data"].toArray();
                if (!data.isEmpty()) {
                    QString current = modelCombo->currentText();
                    modelCombo->clear();
                    for (const QJsonValue &val : data) {
                        QString id = val.toObject()["id"].toString();
                        if (!id.isEmpty()) {
                            modelCombo->addItem(id);
                        }
                    }
                    // Restore selection
                    int idx = modelCombo->findText(current);
                    if (idx >= 0) modelCombo->setCurrentIndex(idx);
                    else if (modelCombo->count() > 0) modelCombo->setCurrentIndex(0);
                }
            } else {
                QMessageBox::warning(nullptr, "Fetch Models",
                    "Failed to fetch models:\n" + reply->errorString() +
                    "\n\nMake sure the endpoint is correct and the service is running.");
            }
            reply->deleteLater();
            mgr->deleteLater();
        });
    });

    modelRow->addWidget(modelCombo, 1);
    modelRow->addWidget(refreshModelsBtn);
    aiForm->addRow("Model:", modelRow);

    // Temperature slider
    QHBoxLayout *tempRow = new QHBoxLayout();
    QSlider *tempSlider = new QSlider(Qt::Horizontal);
    tempSlider->setRange(0, 200);  // 0.0 to 2.0
    tempSlider->setValue(static_cast<int>(AppSettings::instance().getAIConfig().temperature * 100));
    QLabel *tempValue = new QLabel(QString::number(tempSlider->value() / 100.0, 'f', 2));
    tempValue->setFixedWidth(50);
    QObject::connect(tempSlider, &QSlider::valueChanged, parent, [tempValue](int value) {
        tempValue->setText(QString::number(value / 100.0, 'f', 2));
        auto config = AppSettings::instance().getAIConfig();
        config.temperature = value / 100.0f;
        AppSettings::instance().setAIConfig(config);
    });
    tempRow->addWidget(tempSlider, 1);
    tempRow->addWidget(tempValue);
    aiForm->addRow("Temperature:", tempRow);

    // Max tokens
    QSpinBox *tokensSpinBox = new QSpinBox();
    tokensSpinBox->setRange(100, 32000);
    tokensSpinBox->setValue(AppSettings::instance().getAIConfig().maxTokens);
    tokensSpinBox->setSingleStep(100);
    QObject::connect(tokensSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), parent, [](int value) {
        auto config = AppSettings::instance().getAIConfig();
        config.maxTokens = value;
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("Max Tokens:", tokensSpinBox);

    // Show token count checkbox
    QCheckBox *showTokenCheck = new QCheckBox();
    showTokenCheck->setChecked(AppSettings::instance().getAIConfig().showTokenCount);
    QObject::connect(showTokenCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getAIConfig();
        config.showTokenCount = checked;
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("Show Token Count:", showTokenCheck);

    // Auto fallback checkbox
    QCheckBox *fallbackCheck = new QCheckBox("Automatically use translation if AI service is unavailable");
    fallbackCheck->setChecked(AppSettings::instance().getAIConfig().autoFallbackToTranslation);
    QObject::connect(fallbackCheck, &QCheckBox::toggled, parent, [](bool checked) {
        auto config = AppSettings::instance().getAIConfig();
        config.autoFallbackToTranslation = checked;
        AppSettings::instance().setAIConfig(config);
    });
    aiForm->addRow("Auto Fallback:", fallbackCheck);

    // Token usage display
    QHBoxLayout *usageRow = new QHBoxLayout();
    parent->tokenUsageLabel = new QLabel(QString::number(AppSettings::instance().getAIConfig().totalTokensUsed));
    parent->tokenUsageLabel->setMinimumWidth(100);
    QPushButton *resetButton = new QPushButton("Reset");
    resetButton->setMaximumWidth(80);
    QObject::connect(resetButton, &QPushButton::clicked, parent, [parent]() {
        auto config = AppSettings::instance().getAIConfig();
        config.totalTokensUsed = 0;
        AppSettings::instance().setAIConfig(config);
        if (parent->tokenUsageLabel) {
            parent->tokenUsageLabel->setText("0");
        }
    });
    usageRow->addWidget(parent->tokenUsageLabel);
    usageRow->addWidget(resetButton);
    usageRow->addStretch();
    aiForm->addRow("Total Tokens Used:", usageRow);

    layout->addWidget(aiGroup);

    // Quick Start Guide
    QGroupBox *setupGroup = new QGroupBox("📋 Quick Start");
    setupGroup->setObjectName("settingsGroup");
    QVBoxLayout *setupLayout = new QVBoxLayout();

    QLabel *setupText = new QLabel(
        "<b>Bring Your Own Key</b> - Use any OpenAI-compatible API provider:<br><br>"
        "<b>Groq (Recommended - Free Tier Available):</b><br>"
        "1. Sign up at <a href='https://console.groq.com'>console.groq.com</a><br>"
        "2. Create an API key<br>"
        "3. Paste key above - you're done!<br><br>"
        "<b>OpenAI:</b><br>"
        "1. Get API key from <a href='https://platform.openai.com/api-keys'>platform.openai.com</a><br>"
        "2. Select OpenAI provider, paste key<br><br>"
        "<b>Ollama (Local, Free, Private):</b><br>"
        "1. Install Ollama from <a href='https://ollama.com'>ollama.com</a><br>"
        "2. Run: <code>ollama pull llama3.2</code><br>"
        "3. Select Ollama provider - no API key needed<br><br>"
        "<b>LM Studio (Local, Free, Private):</b><br>"
        "1. Install from <a href='https://lmstudio.ai'>lmstudio.ai</a><br>"
        "2. Load a model and start the server<br>"
        "3. Select LM Studio provider - no API key needed<br><br>"
        "<b>Custom Endpoint:</b><br>"
        "Any service with an OpenAI-compatible /v1/chat/completions API works."
    );
    setupText->setWordWrap(true);
    setupText->setTextFormat(Qt::RichText);
    setupText->setOpenExternalLinks(true);
    setupLayout->addWidget(setupText);

    setupGroup->setLayout(setupLayout);
    layout->addWidget(setupGroup);

    // Info section
    QGroupBox *infoGroup = new QGroupBox("ℹ️  About AI Assistant");
    infoGroup->setObjectName("settingsGroup");
    QVBoxLayout *infoLayout = new QVBoxLayout();
    QLabel *infoText = new QLabel(
        "AI Assistant adds intelligent chat capabilities to the Translation Chat window:\n\n"
        "• Context-aware responses with conversation history\n"
        "• Language learning assistance and explanations\n"
        "• Automatic fallback to translation if AI is unavailable\n"
        "• Token usage tracking for transparency\n"
        "• Works with any OpenAI-compatible API\n\n"
        "Your API key is stored locally and never shared."
    );
    infoText->setWordWrap(true);
    infoLayout->addWidget(infoText);
    infoGroup->setLayout(infoLayout);
    layout->addWidget(infoGroup);

    layout->addStretch();

    // Set content widget to scroll area
    scrollArea->setWidget(page);
    return scrollArea;
}

QWidget* createHelpPage()
{
    // Create scroll area for long content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Create content widget
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(20);
    layout->setContentsMargins(24, 24, 24, 24);

    // Title
    QLabel *titleLabel = new QLabel("📖 User Guide");
    titleLabel->setObjectName("pageTitle");
    layout->addWidget(titleLabel);

    // Welcome section
    QGroupBox *welcomeGroup = new QGroupBox("Welcome to OhaoLang!");
    welcomeGroup->setObjectName("settingsGroup");
    QVBoxLayout *welcomeLayout = new QVBoxLayout(welcomeGroup);
    welcomeLayout->setSpacing(10);

    QLabel *welcomeText = new QLabel(
        "OhaoLang is a powerful multilingual translation and OCR tool that helps you "
        "translate text from screenshots, speak translations aloud, and chat with AI. "
        "Below is a guide to help you get started with each feature."
    );
    welcomeText->setWordWrap(true);
    welcomeLayout->addWidget(welcomeText);
    layout->addWidget(welcomeGroup);

    // Screenshot & OCR section
    QGroupBox *screenshotGroup = new QGroupBox("📸 Screenshot & OCR");
    screenshotGroup->setObjectName("settingsGroup");
    QVBoxLayout *screenshotLayout = new QVBoxLayout(screenshotGroup);
    screenshotLayout->setSpacing(8);

    QLabel *screenshotTitle = new QLabel("<b>Taking Screenshots for Translation</b>");
    screenshotLayout->addWidget(screenshotTitle);

    QLabel *screenshotText = new QLabel(
        "1. Press <b>Ctrl+Alt+X</b> (or your custom shortcut) to activate screenshot mode<br>"
        "2. Click and drag to select the area containing text<br>"
        "3. Release to capture - the app will automatically detect and extract text<br>"
        "4. Recognized text will appear in the main widget<br><br>"
        "<b>OCR Settings:</b><br>"
        "• Go to <i>OCR</i> tab to select OCR engine (Tesseract or PaddleOCR)<br>"
        "• Choose the language of the text you want to recognize<br>"
        "• Tesseract: Good for most languages, offline<br>"
        "• PaddleOCR: Better accuracy for Asian languages (Chinese, Japanese, Korean)"
    );
    screenshotText->setWordWrap(true);
    screenshotText->setTextFormat(Qt::RichText);
    screenshotLayout->addWidget(screenshotText);
    layout->addWidget(screenshotGroup);

    // Translation section
    QGroupBox *translationGroup = new QGroupBox("🌐 Translation");
    translationGroup->setObjectName("settingsGroup");
    QVBoxLayout *translationLayout = new QVBoxLayout(translationGroup);
    translationLayout->setSpacing(8);

    QLabel *translationTitle = new QLabel("<b>Translating Text</b>");
    translationLayout->addWidget(translationTitle);

    QLabel *translationText = new QLabel(
        "After OCR extracts text from your screenshot, translation happens automatically if enabled:<br><br>"
        "<b>Setup:</b><br>"
        "1. Go to <i>Translation</i> tab<br>"
        "2. Enable \"Auto-translate after OCR\"<br>"
        "3. Select translation engine (Google, DeepL, or Custom API)<br>"
        "4. Choose your target language<br>"
        "5. For custom API, enter your API URL and key<br><br>"
        "<b>How it works:</b><br>"
        "• Original text appears in the top section<br>"
        "• Translated text appears in the bottom section<br>"
        "• Click text to copy to clipboard"
    );
    translationText->setWordWrap(true);
    translationText->setTextFormat(Qt::RichText);
    translationLayout->addWidget(translationText);
    layout->addWidget(translationGroup);

    // Text-to-Speech section
    QGroupBox *ttsGroup = new QGroupBox("🔊 Text-to-Speech");
    ttsGroup->setObjectName("settingsGroup");
    QVBoxLayout *ttsLayout = new QVBoxLayout(ttsGroup);
    ttsLayout->setSpacing(8);

    QLabel *ttsTitle = new QLabel("<b>Reading Text Aloud</b>");
    ttsLayout->addWidget(ttsTitle);

    QLabel *ttsText = new QLabel(
        "<b>Two ways to use TTS:</b><br><br>"
        "1. <b>Automatic</b> - After translation:<br>"
        "   • Go to <i>Voice</i> tab<br>"
        "   • Enable \"Speak translation automatically\"<br>"
        "   • Select voice provider and voice<br>"
        "   • Translations will be spoken automatically<br><br>"
        "2. <b>Read Selected Text</b> - From anywhere:<br>"
        "   • Select any text in any application<br>"
        "   • Press <b>Ctrl+Alt+A</b> (or your custom shortcut)<br>"
        "   • The selected text will be read aloud<br><br>"
        "<b>Voice Options:</b><br>"
        "• <i>System TTS</i>: Uses your system's built-in voices (offline)<br>"
        "• <i>Edge TTS</i>: Microsoft's high-quality voices (requires internet)<br>"
        "• Word-by-word mode: Highlights and speaks each word individually"
    );
    ttsText->setWordWrap(true);
    ttsText->setTextFormat(Qt::RichText);
    ttsLayout->addWidget(ttsText);
    layout->addWidget(ttsGroup);

    // AI Chat section
    QGroupBox *chatGroup = new QGroupBox("💬 AI Chat Assistant");
    chatGroup->setObjectName("settingsGroup");
    QVBoxLayout *chatLayout = new QVBoxLayout(chatGroup);
    chatLayout->setSpacing(8);

    QLabel *chatTitle = new QLabel("<b>Using the AI Chat Window</b>");
    chatLayout->addWidget(chatTitle);

    QLabel *chatText = new QLabel(
        "Press <b>Ctrl+Alt+C</b> (or your custom shortcut) to open the AI chat window:<br><br>"
        "<b>Features:</b><br>"
        "• Ask questions about translations<br>"
        "• Get explanations of words or phrases<br>"
        "• Request grammar help<br>"
        "• Have conversations in your target language<br><br>"
        "<b>Setup:</b><br>"
        "1. Go to <i>AI</i> tab<br>"
        "2. Enable AI Assistant<br>"
        "3. Choose a provider (Groq, OpenAI, Ollama, LM Studio, or Custom)<br>"
        "4. Enter your API key and select a model<br><br>"
        "<b>Tip:</b> Groq offers a free tier. For fully local/private AI, use Ollama or LM Studio."
    );
    chatText->setWordWrap(true);
    chatText->setTextFormat(Qt::RichText);
    chatLayout->addWidget(chatText);
    layout->addWidget(chatGroup);

    // Shortcuts section
    QGroupBox *shortcutsGroup = new QGroupBox("⌨️ Global Shortcuts");
    shortcutsGroup->setObjectName("settingsGroup");
    QVBoxLayout *shortcutsLayout = new QVBoxLayout(shortcutsGroup);
    shortcutsLayout->setSpacing(8);

    QLabel *shortcutsTitle = new QLabel("<b>Default Keyboard Shortcuts</b>");
    shortcutsLayout->addWidget(shortcutsTitle);

    QLabel *shortcutsText = new QLabel(
        "These shortcuts work globally, even when the app is in the background:<br><br>"
        "• <b>Ctrl+Alt+X</b> - Take Screenshot for OCR<br>"
        "• <b>Ctrl+Alt+H</b> - Toggle Main Widget Visibility<br>"
        "• <b>Ctrl+Alt+C</b> - Open AI Chat Window<br>"
        "• <b>Ctrl+Alt+A</b> - Read Selected Text Aloud<br><br>"
        "<b>Customization:</b><br>"
        "Go to <i>General</i> tab → Shortcuts section to customize these shortcuts.<br>"
        "Click in the shortcut field and press your desired key combination.<br>"
        "Use the \"Reset to Defaults\" button to restore original shortcuts.<br><br>"
        "<b>Note:</b> Avoid conflicts with other applications' shortcuts!"
    );
    shortcutsText->setWordWrap(true);
    shortcutsText->setTextFormat(Qt::RichText);
    shortcutsLayout->addWidget(shortcutsText);
    layout->addWidget(shortcutsGroup);

    // Tips & Tricks section
    QGroupBox *tipsGroup = new QGroupBox("💡 Tips & Tricks");
    tipsGroup->setObjectName("settingsGroup");
    QVBoxLayout *tipsLayout = new QVBoxLayout(tipsGroup);
    tipsLayout->setSpacing(8);

    QLabel *tipsText = new QLabel(
        "• <b>Quick Translation Workflow:</b> Screenshot → Auto-OCR → Auto-Translate → Auto-Speak<br>"
        "• <b>Widget Positioning:</b> Drag the main widget anywhere on screen - position is saved<br>"
        "• <b>Theme:</b> Switch between Light/Dark/Auto in <i>Appearance</i> tab<br>"
        "• <b>Multiple Languages:</b> You can switch OCR and translation languages on the fly<br>"
        "• <b>Clipboard Integration:</b> Click any text in the widget to copy it<br>"
        "• <b>System Tray:</b> App minimizes to system tray - right-click for quick actions<br>"
        "• <b>Linux Users:</b> Use the \"Update GNOME Shortcuts\" button in <i>General</i> to register shortcuts"
    );
    tipsText->setWordWrap(true);
    tipsText->setTextFormat(Qt::RichText);
    tipsLayout->addWidget(tipsText);
    layout->addWidget(tipsGroup);

    // Troubleshooting section
    QGroupBox *troubleshootGroup = new QGroupBox("🔧 Troubleshooting");
    troubleshootGroup->setObjectName("settingsGroup");
    QVBoxLayout *troubleshootLayout = new QVBoxLayout(troubleshootGroup);
    troubleshootLayout->setSpacing(8);

    QLabel *troubleshootText = new QLabel(
        "<b>Common Issues:</b><br><br>"
        "• <b>Shortcuts not working:</b> Check if another app is using the same shortcut<br>"
        "• <b>Poor OCR accuracy:</b> Try switching OCR engine or adjusting screenshot quality<br>"
        "• <b>Translation fails:</b> Check internet connection and API settings<br>"
        "• <b>No TTS voice:</b> Install system TTS voices or enable Edge TTS with internet<br>"
        "• <b>AI Chat not responding:</b> Check API key, endpoint URL, and that the service is running<br><br>"
        "<b>For more help:</b> Visit the project repository or check the documentation."
    );
    troubleshootText->setWordWrap(true);
    troubleshootText->setTextFormat(Qt::RichText);
    troubleshootLayout->addWidget(troubleshootText);
    layout->addWidget(troubleshootGroup);

    layout->addStretch();

    // Set content widget to scroll area
    scrollArea->setWidget(page);
    return scrollArea;
}

} // namespace SettingsPages
