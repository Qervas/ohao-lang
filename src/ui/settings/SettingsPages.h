#pragma once

#include <QWidget>
#include <QSettings>

class ModernSettingsWindow;
class GlobalShortcutManager;
class SystemTray;
class TTSEngine;

/**
 * Helper namespace for creating settings pages
 * Extracted from ModernSettingsWindow to improve code organization
 */
namespace SettingsPages
{
    QWidget* createGeneralPage(ModernSettingsWindow* parent,
                                QSettings& settings,
                                GlobalShortcutManager* shortcutManager,
                                SystemTray* systemTray);

    QWidget* createOCRPage(ModernSettingsWindow* parent, QSettings& settings);

    QWidget* createTranslationPage(ModernSettingsWindow* parent, QSettings& settings);

    QWidget* createAppearancePage(ModernSettingsWindow* parent, QSettings& settings);

    QWidget* createVoicePage(ModernSettingsWindow* parent,
                              QSettings& settings,
                              TTSEngine* ttsEngine);

    QWidget* createChatPage(ModernSettingsWindow* parent, QSettings& settings);

    QWidget* createAIPage(ModernSettingsWindow* parent, QSettings& settings);

    QWidget* createHelpPage();
}
