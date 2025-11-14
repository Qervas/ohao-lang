#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QSettings>
#include "../../system/ShortcutConfig.h"

class TTSEngine;
class GlobalShortcutManager;
class SystemTray;

/**
 * Modern macOS-style Settings Window
 *
 * Features:
 * - Sidebar navigation (like macOS System Preferences)
 * - Clean, single-pane content area
 * - Smooth animations
 * - Proper initialization order (no race conditions)
 * - Beautiful Apple-inspired design
 */
class ModernSettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModernSettingsWindow(QWidget *parent = nullptr);
    ~ModernSettingsWindow();

    void setShortcutManager(GlobalShortcutManager *manager);
    void setSystemTray(SystemTray *tray);

    // Public methods needed by SettingsPages
    void applyTheme(const QString &themeName);
    void saveSettings();
    void updateGnomeShortcuts();
    void updateVoiceList();
    void resetShortcutsToDefaults();
    void onSettingChanged();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onSidebarItemChanged(int index);

private:
    void setupUI();
    void createSidebar();
    void createContentStack();
    void applyMacOSStyle();
    void loadSettings();
    QPair<bool, QString> validateShortcuts() const;

    // UI Components
    QHBoxLayout *mainLayout;
    QListWidget *sidebar;
    QStackedWidget *contentStack;

public:
    // Widget pointers accessible by SettingsPages
    // General Page widgets
    QComboBox *ocrLanguageCombo = nullptr;
    QComboBox *targetLanguageCombo = nullptr;
    QSlider *screenshotDimmingSlider = nullptr;
    QLabel *dimmingValueLabel = nullptr;
    QKeySequenceEdit *screenshotShortcutEdit = nullptr;
    QKeySequenceEdit *toggleShortcutEdit = nullptr;
    QKeySequenceEdit *chatWindowShortcutEdit = nullptr;
    QKeySequenceEdit *readAloudShortcutEdit = nullptr;

    // OCR Page widgets
    QComboBox *ocrEngineCombo = nullptr;

    // Translation Page widgets
    QCheckBox *autoTranslateCheck = nullptr;
    QComboBox *translationEngineCombo = nullptr;
    QLineEdit *apiUrlEdit = nullptr;
    QLineEdit *apiKeyEdit = nullptr;

    // Appearance Page widgets
    QComboBox *themeCombo = nullptr;
    QSlider *widgetWidthSlider = nullptr;
    QLabel *widthValueLabel = nullptr;

    // Voice Page widgets
    QCheckBox *ttsEnabledCheck = nullptr;
    QComboBox *ttsProviderCombo = nullptr;
    QLabel *providerInfoLabel = nullptr;
    QComboBox *voiceCombo = nullptr;
    QPushButton *testVoiceBtn = nullptr;
    QCheckBox *ttsSpeakTranslationCheck = nullptr;
    QCheckBox *ttsWordByWordCheck = nullptr;

    // AI Page widgets
    QLabel *tokenUsageLabel = nullptr;

private:

    // Settings storage
    QSettings settings;

    // Dependencies
    TTSEngine *ttsEngine = nullptr;
    GlobalShortcutManager *shortcutManager = nullptr;
    SystemTray *systemTray = nullptr;

    // State
    bool isInitializing = true;
};
