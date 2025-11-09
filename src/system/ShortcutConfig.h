#pragma once

#include <QString>
#include <QKeySequence>
#include <QMap>

/**
 * Centralized shortcut configuration to prevent conflicts
 * and provide a single source of truth for all global shortcuts
 */
class ShortcutConfig
{
public:
    enum ShortcutAction {
        Screenshot,
        ToggleVisibility,
        ChatWindow,
        ReadAloud
    };

    struct ShortcutDefinition {
        QString name;
        QString defaultShortcut;
        QString settingsKey;
        int hotkeyId;  // Windows hotkey ID

        ShortcutDefinition() : hotkeyId(0) {}

        ShortcutDefinition(const QString &n, const QString &def, const QString &key, int id)
            : name(n), defaultShortcut(def), settingsKey(key), hotkeyId(id) {}
    };

    static ShortcutConfig& instance() {
        static ShortcutConfig instance;
        return instance;
    }

    // Get shortcut definition for an action
    ShortcutDefinition getShortcut(ShortcutAction action) const {
        return shortcuts.value(action);  // Use .value() for const safety
    }

    // Get all shortcuts
    const QMap<ShortcutAction, ShortcutDefinition>& getAllShortcuts() const {
        return shortcuts;
    }

    // Check if hotkey ID is valid and not conflicting
    bool isValidHotkeyId(int id) const {
        for (const auto &shortcut : shortcuts) {
            if (shortcut.hotkeyId == id) {
                return true;
            }
        }
        return false;
    }

    // Get action by hotkey ID
    ShortcutAction getActionByHotkeyId(int id) const {
        // Explicit mapping for safety and clarity
        if (getShortcut(Screenshot).hotkeyId == id) {
            return Screenshot;
        }
        if (getShortcut(ToggleVisibility).hotkeyId == id) {
            return ToggleVisibility;
        }
        if (getShortcut(ChatWindow).hotkeyId == id) {
            return ChatWindow;
        }
        if (getShortcut(ReadAloud).hotkeyId == id) {
            return ReadAloud;
        }

        // Fallback: should never reach here
        return Screenshot;
    }

private:
    ShortcutConfig() {
        // Define all shortcuts in one place with guaranteed unique IDs
#ifdef Q_OS_MACOS
        shortcuts[Screenshot] = ShortcutDefinition(
            "Take Screenshot",
            "Meta+Shift+X",
            "shortcuts/screenshot",
            1
        );
        shortcuts[ToggleVisibility] = ShortcutDefinition(
            "Toggle Widget",
            "Meta+Shift+H",
            "shortcuts/toggle",
            2
        );
        shortcuts[ChatWindow] = ShortcutDefinition(
            "Toggle Chat Window",
            "Meta+Shift+C",
            "shortcuts/chatWindow",
            3
        );
        shortcuts[ReadAloud] = ShortcutDefinition(
            "Read Selected Text Aloud",
            "Ctrl+Alt+A",  // Same on macOS
            "shortcuts/readAloud",
            4
        );
#else
        // Windows and Linux
        shortcuts[Screenshot] = ShortcutDefinition(
            "Take Screenshot",
            "Ctrl+Alt+X",
            "shortcuts/screenshot",
            1
        );
        shortcuts[ToggleVisibility] = ShortcutDefinition(
            "Toggle Widget",
            "Ctrl+Alt+H",
            "shortcuts/toggle",
            2
        );
        shortcuts[ChatWindow] = ShortcutDefinition(
            "Toggle Chat Window",
            "Ctrl+Alt+C",
            "shortcuts/chatWindow",
            3
        );
        shortcuts[ReadAloud] = ShortcutDefinition(
            "Read Selected Text Aloud",
            "Ctrl+Alt+A",
            "shortcuts/readAloud",
            4
        );
#endif
    }

    QMap<ShortcutAction, ShortcutDefinition> shortcuts;
};
