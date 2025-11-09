#include "GlobalShortcutManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QKeySequence>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#endif

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#endif

GlobalShortcutManager::GlobalShortcutManager(QObject *parent)
    : QObject(parent)
{
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    registerShortcuts();
}

GlobalShortcutManager::~GlobalShortcutManager()
{
    unregisterShortcuts();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

void GlobalShortcutManager::registerShortcuts()
{
    unregisterShortcuts();

#ifdef Q_OS_WIN
    // Use ShortcutConfig for centralized shortcut management
    QSettings settings;
    const auto& config = ShortcutConfig::instance();

    // PHASE 4: Auto-detect and fix duplicate shortcuts on startup
    QMap<QString, QStringList> shortcutUsage;
    QList<ShortcutConfig::ShortcutAction> actions = {
        ShortcutConfig::Screenshot,
        ShortcutConfig::ToggleVisibility,
        ShortcutConfig::ChatWindow,
        ShortcutConfig::ReadAloud
    };

    // Detect duplicates
    for (const auto& action : actions) {
        const auto& def = config.getShortcut(action);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();
        shortcutUsage[shortcut].append(def.name);
    }

    // Fix duplicates by resetting to defaults
    bool hadDuplicates = false;
    for (auto it = shortcutUsage.begin(); it != shortcutUsage.end(); ++it) {
        if (it.value().size() > 1) {
            hadDuplicates = true;
            qWarning() << "════════════════════════════════════════════════════════";
            qWarning() << "DUPLICATE SHORTCUTS DETECTED ON STARTUP!";
            qWarning() << "  Key combination:" << it.key();
            qWarning() << "  Used by:" << it.value().join(", ");
            qWarning() << "  Action: Resetting all conflicting shortcuts to defaults";
            qWarning() << "════════════════════════════════════════════════════════";

            // Reset each conflicting shortcut to its default
            for (const QString& name : it.value()) {
                for (const auto& action : actions) {
                    const auto& def = config.getShortcut(action);
                    if (def.name == name) {
                        settings.setValue(def.settingsKey, def.defaultShortcut);
                        qInfo() << "Reset" << name << "to default:" << def.defaultShortcut;
                    }
                }
            }
        }
    }

    if (hadDuplicates) {
        settings.sync();
        qWarning() << "Duplicate shortcuts have been automatically fixed.";
        qWarning() << "Please check Settings → General → Shortcuts to customize.";
    }

    // Track registered key combinations to detect conflicts
    QMap<QString, QString> registeredCombos;  // key combo -> shortcut name

    // Helper lambda to parse key sequence and register Windows hotkey
    auto registerWindowsHotkey = [&](ShortcutConfig::ShortcutAction action, bool* registered) -> bool {
        const auto& def = config.getShortcut(action);
        QString shortcut = settings.value(def.settingsKey, def.defaultShortcut).toString();

        // CHECK FOR DUPLICATE KEY COMBINATION
        if (registeredCombos.contains(shortcut)) {
            qWarning() << "════════════════════════════════════════════════════════";
            qWarning() << "DUPLICATE SHORTCUT DETECTED!";
            qWarning() << "  Key combination:" << shortcut;
            qWarning() << "  Already registered for:" << registeredCombos[shortcut];
            qWarning() << "  Attempted to register for:" << def.name;
            qWarning() << "  Action: Skipping duplicate registration";
            qWarning() << "════════════════════════════════════════════════════════";
            *registered = false;
            return false;
        }

        QKeySequence seq(shortcut);
        if (seq.count() == 0) {
            qWarning() << "Invalid shortcut for" << def.name << ":" << shortcut;
            *registered = false;
            return false;
        }

        int keyCode = seq[0].toCombined();

        // Extract modifiers
        UINT modifiers = MOD_NOREPEAT;
        if (keyCode & Qt::ControlModifier) modifiers |= MOD_CONTROL;
        if (keyCode & Qt::AltModifier) modifiers |= MOD_ALT;
        if (keyCode & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
        if (keyCode & Qt::MetaModifier) modifiers |= MOD_WIN;

        // Extract base key
        int baseKey = keyCode & ~(Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);

        // Convert Qt key to Windows virtual key code
        UINT vkCode = 0;
        if (baseKey >= Qt::Key_A && baseKey <= Qt::Key_Z) {
            vkCode = 0x41 + (baseKey - Qt::Key_A); // A=0x41, B=0x42, etc.
        } else if (baseKey >= Qt::Key_0 && baseKey <= Qt::Key_9) {
            vkCode = 0x30 + (baseKey - Qt::Key_0); // 0=0x30, 1=0x31, etc.
        } else {
            qWarning() << "Unsupported key code for" << def.name << ":" << baseKey;
            *registered = false;
            return false;
        }

        qDebug() << "Registering" << def.name << ":" << shortcut << "ID:" << def.hotkeyId << "Modifiers:" << modifiers << "VK:" << QString::number(vkCode, 16);

        if (RegisterHotKey(nullptr, def.hotkeyId, modifiers, vkCode)) {
            *registered = true;
            registeredCombos[shortcut] = def.name;  // Track successful registration
            qInfo() << "✓" << def.name << "registered successfully (ID:" << def.hotkeyId << ")";
            return true;
        } else {
            *registered = false;
            DWORD error = GetLastError();
            qWarning() << "✗" << def.name << "registration failed. Error:" << error;
            if (error == 1409) {
                qWarning() << "  → Hotkey already registered by another application";
            }
            return false;
        }
    };

    // Register all shortcuts using ShortcutConfig
    qDebug() << "========================================";
    qDebug() << "Registering global shortcuts...";
    qDebug() << "========================================";

    bool allSucceeded = true;
    allSucceeded &= registerWindowsHotkey(ShortcutConfig::Screenshot, &screenshotRegistered);
    allSucceeded &= registerWindowsHotkey(ShortcutConfig::ToggleVisibility, &toggleRegistered);
    allSucceeded &= registerWindowsHotkey(ShortcutConfig::ChatWindow, &chatWindowRegistered);
    allSucceeded &= registerWindowsHotkey(ShortcutConfig::ReadAloud, &readAloudRegistered);

    qDebug() << "========================================";
    qDebug() << "Registered shortcuts summary:";
    qDebug() << "  Screenshot (enum 0, ID" << config.getShortcut(ShortcutConfig::Screenshot).hotkeyId << "):" << (screenshotRegistered ? "OK" : "FAIL");
    qDebug() << "  Toggle (enum 1, ID" << config.getShortcut(ShortcutConfig::ToggleVisibility).hotkeyId << "):" << (toggleRegistered ? "OK" : "FAIL");
    qDebug() << "  Chat (enum 2, ID" << config.getShortcut(ShortcutConfig::ChatWindow).hotkeyId << "):" << (chatWindowRegistered ? "OK" : "FAIL");
    qDebug() << "  ReadAloud (enum 3, ID" << config.getShortcut(ShortcutConfig::ReadAloud).hotkeyId << "):" << (readAloudRegistered ? "OK" : "FAIL");
    qDebug() << "========================================";

    // Write debug info to file for Windows debugging
    QFile debugFile("C:/Users/djmax/Desktop/shortcut-debug.txt");
    if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&debugFile);
        out << "Shortcut Registration Debug\n";
        out << "============================\n\n";

        // Show actual key combinations registered
        auto screenshotKey = settings.value(config.getShortcut(ShortcutConfig::Screenshot).settingsKey,
                                             config.getShortcut(ShortcutConfig::Screenshot).defaultShortcut).toString();
        auto toggleKey = settings.value(config.getShortcut(ShortcutConfig::ToggleVisibility).settingsKey,
                                         config.getShortcut(ShortcutConfig::ToggleVisibility).defaultShortcut).toString();
        auto chatKey = settings.value(config.getShortcut(ShortcutConfig::ChatWindow).settingsKey,
                                       config.getShortcut(ShortcutConfig::ChatWindow).defaultShortcut).toString();
        auto readAloudKey = settings.value(config.getShortcut(ShortcutConfig::ReadAloud).settingsKey,
                                            config.getShortcut(ShortcutConfig::ReadAloud).defaultShortcut).toString();

        out << "Screenshot (enum 0, ID " << config.getShortcut(ShortcutConfig::Screenshot).hotkeyId << ", Key: " << screenshotKey << "): " << (screenshotRegistered ? "OK" : "FAIL") << "\n";
        out << "Toggle (enum 1, ID " << config.getShortcut(ShortcutConfig::ToggleVisibility).hotkeyId << ", Key: " << toggleKey << "): " << (toggleRegistered ? "OK" : "FAIL") << "\n";
        out << "Chat (enum 2, ID " << config.getShortcut(ShortcutConfig::ChatWindow).hotkeyId << ", Key: " << chatKey << "): " << (chatWindowRegistered ? "OK" : "FAIL") << "\n";
        out << "ReadAloud (enum 3, ID " << config.getShortcut(ShortcutConfig::ReadAloud).hotkeyId << ", Key: " << readAloudKey << "): " << (readAloudRegistered ? "OK" : "FAIL") << "\n";
        debugFile.close();
    }

    if (!allSucceeded) {
        qWarning() << "════════════════════════════════════════════════════════";
        qWarning() << "Some global shortcuts failed to register!";
        qWarning() << "  Screenshot:" << (screenshotRegistered ? "OK" : "FAILED");
        qWarning() << "  Toggle:" << (toggleRegistered ? "OK" : "FAILED");
        qWarning() << "  Chat:" << (chatWindowRegistered ? "OK" : "FAILED");
        qWarning() << "  Read Aloud:" << (readAloudRegistered ? "OK" : "FAILED");
        qWarning() << "════════════════════════════════════════════════════════";
    }
#elif defined(Q_OS_MACOS)
    // Load custom shortcuts from settings
    QSettings settings;
    QString screenshotShortcut = settings.value("shortcuts/screenshot", "Meta+Shift+X").toString();
    QString toggleShortcut = settings.value("shortcuts/toggle", "Meta+Shift+Z").toString();
    QString chatWindowShortcut = settings.value("shortcuts/chatWindow", "Meta+Shift+C").toString();
    QString readAloudShortcut = settings.value("shortcuts/readAloud", "Ctrl+Alt+A").toString();

    QKeySequence screenshotSeq(screenshotShortcut);
    QKeySequence toggleSeq(toggleShortcut);
    QKeySequence chatWindowSeq(chatWindowShortcut);
    QKeySequence readAloudSeq(readAloudShortcut);
    
    // Install event handler for hotkeys
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyPressed;
    eventHandlerUPP = NewEventHandlerUPP(hotKeyHandler);
    InstallApplicationEventHandler(eventHandlerUPP, 1, &eventType, this, &eventHandlerRef);

    // Parse screenshot shortcut
    if (screenshotSeq.count() > 0) {
        int keyCode = 0;
        int modifiers = 0;
        
        int key = screenshotSeq[0].toCombined();
        
        // Extract modifiers
        if (key & Qt::MetaModifier) modifiers |= cmdKey;
        if (key & Qt::ShiftModifier) modifiers |= shiftKey;
        if (key & Qt::ControlModifier) modifiers |= controlKey;
        if (key & Qt::AltModifier) modifiers |= optionKey;
        
        // Extract key code (simplified mapping for common keys)
        int baseKey = key & ~(Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);
        
        // Map Qt keys to Carbon virtual key codes
        switch (baseKey) {
            case Qt::Key_A: keyCode = kVK_ANSI_A; break;
            case Qt::Key_B: keyCode = kVK_ANSI_B; break;
            case Qt::Key_C: keyCode = kVK_ANSI_C; break;
            case Qt::Key_D: keyCode = kVK_ANSI_D; break;
            case Qt::Key_E: keyCode = kVK_ANSI_E; break;
            case Qt::Key_F: keyCode = kVK_ANSI_F; break;
            case Qt::Key_G: keyCode = kVK_ANSI_G; break;
            case Qt::Key_H: keyCode = kVK_ANSI_H; break;
            case Qt::Key_I: keyCode = kVK_ANSI_I; break;
            case Qt::Key_J: keyCode = kVK_ANSI_J; break;
            case Qt::Key_K: keyCode = kVK_ANSI_K; break;
            case Qt::Key_L: keyCode = kVK_ANSI_L; break;
            case Qt::Key_M: keyCode = kVK_ANSI_M; break;
            case Qt::Key_N: keyCode = kVK_ANSI_N; break;
            case Qt::Key_O: keyCode = kVK_ANSI_O; break;
            case Qt::Key_P: keyCode = kVK_ANSI_P; break;
            case Qt::Key_Q: keyCode = kVK_ANSI_Q; break;
            case Qt::Key_R: keyCode = kVK_ANSI_R; break;
            case Qt::Key_S: keyCode = kVK_ANSI_S; break;
            case Qt::Key_T: keyCode = kVK_ANSI_T; break;
            case Qt::Key_U: keyCode = kVK_ANSI_U; break;
            case Qt::Key_V: keyCode = kVK_ANSI_V; break;
            case Qt::Key_W: keyCode = kVK_ANSI_W; break;
            case Qt::Key_X: keyCode = kVK_ANSI_X; break;
            case Qt::Key_Y: keyCode = kVK_ANSI_Y; break;
            case Qt::Key_Z: keyCode = kVK_ANSI_Z; break;
            default:
                qWarning() << "Unsupported key for screenshot shortcut:" << baseKey;
                keyCode = -1;
        }
        
        if (keyCode >= 0) {
            EventHotKeyID screenshotHotKeyID;
            screenshotHotKeyID.signature = 'htk1';
            screenshotHotKeyID.id = 1;
            
            OSStatus status = RegisterEventHotKey(
                keyCode,
                modifiers,
                screenshotHotKeyID,
                GetApplicationEventTarget(),
                0,
                &screenshotHotKeyRef
            );
            
            if (status == noErr) {
                screenshotRegistered = true;
                qInfo() << "Registered global screenshot shortcut:" << screenshotShortcut;
            } else {
                screenshotRegistered = false;
                qWarning() << "Failed to register global shortcut" << screenshotShortcut << "Error:" << status;
            }
        }
    }

    // Parse toggle shortcut (same logic)
    if (toggleSeq.count() > 0) {
        int keyCode = 0;
        int modifiers = 0;
        
        int key = toggleSeq[0].toCombined();
        
        if (key & Qt::MetaModifier) modifiers |= cmdKey;
        if (key & Qt::ShiftModifier) modifiers |= shiftKey;
        if (key & Qt::ControlModifier) modifiers |= controlKey;
        if (key & Qt::AltModifier) modifiers |= optionKey;
        
        int baseKey = key & ~(Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);
        
        switch (baseKey) {
            case Qt::Key_A: keyCode = kVK_ANSI_A; break;
            case Qt::Key_B: keyCode = kVK_ANSI_B; break;
            case Qt::Key_C: keyCode = kVK_ANSI_C; break;
            case Qt::Key_D: keyCode = kVK_ANSI_D; break;
            case Qt::Key_E: keyCode = kVK_ANSI_E; break;
            case Qt::Key_F: keyCode = kVK_ANSI_F; break;
            case Qt::Key_G: keyCode = kVK_ANSI_G; break;
            case Qt::Key_H: keyCode = kVK_ANSI_H; break;
            case Qt::Key_I: keyCode = kVK_ANSI_I; break;
            case Qt::Key_J: keyCode = kVK_ANSI_J; break;
            case Qt::Key_K: keyCode = kVK_ANSI_K; break;
            case Qt::Key_L: keyCode = kVK_ANSI_L; break;
            case Qt::Key_M: keyCode = kVK_ANSI_M; break;
            case Qt::Key_N: keyCode = kVK_ANSI_N; break;
            case Qt::Key_O: keyCode = kVK_ANSI_O; break;
            case Qt::Key_P: keyCode = kVK_ANSI_P; break;
            case Qt::Key_Q: keyCode = kVK_ANSI_Q; break;
            case Qt::Key_R: keyCode = kVK_ANSI_R; break;
            case Qt::Key_S: keyCode = kVK_ANSI_S; break;
            case Qt::Key_T: keyCode = kVK_ANSI_T; break;
            case Qt::Key_U: keyCode = kVK_ANSI_U; break;
            case Qt::Key_V: keyCode = kVK_ANSI_V; break;
            case Qt::Key_W: keyCode = kVK_ANSI_W; break;
            case Qt::Key_X: keyCode = kVK_ANSI_X; break;
            case Qt::Key_Y: keyCode = kVK_ANSI_Y; break;
            case Qt::Key_Z: keyCode = kVK_ANSI_Z; break;
            default:
                qWarning() << "Unsupported key for toggle shortcut:" << baseKey;
                keyCode = -1;
        }
        
        if (keyCode >= 0) {
            EventHotKeyID toggleHotKeyID;
            toggleHotKeyID.signature = 'htk2';
            toggleHotKeyID.id = 2;
            
            OSStatus status = RegisterEventHotKey(
                keyCode,
                modifiers,
                toggleHotKeyID,
                GetApplicationEventTarget(),
                0,
                &toggleHotKeyRef
            );
            
            if (status == noErr) {
                toggleRegistered = true;
                qInfo() << "Registered global toggle shortcut:" << toggleShortcut;
            } else {
                toggleRegistered = false;
                qWarning() << "Failed to register global shortcut" << toggleShortcut << "Error:" << status;
            }
        }
    }

    // Parse chat window shortcut (same logic as above)
    if (chatWindowSeq.count() > 0) {
        int keyCode = 0;
        int modifiers = 0;

        int key = chatWindowSeq[0].toCombined();

        if (key & Qt::MetaModifier) modifiers |= cmdKey;
        if (key & Qt::ShiftModifier) modifiers |= shiftKey;
        if (key & Qt::ControlModifier) modifiers |= controlKey;
        if (key & Qt::AltModifier) modifiers |= optionKey;

        int baseKey = key & ~(Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);

        switch (baseKey) {
            case Qt::Key_A: keyCode = kVK_ANSI_A; break;
            case Qt::Key_B: keyCode = kVK_ANSI_B; break;
            case Qt::Key_C: keyCode = kVK_ANSI_C; break;
            case Qt::Key_D: keyCode = kVK_ANSI_D; break;
            case Qt::Key_E: keyCode = kVK_ANSI_E; break;
            case Qt::Key_F: keyCode = kVK_ANSI_F; break;
            case Qt::Key_G: keyCode = kVK_ANSI_G; break;
            case Qt::Key_H: keyCode = kVK_ANSI_H; break;
            case Qt::Key_I: keyCode = kVK_ANSI_I; break;
            case Qt::Key_J: keyCode = kVK_ANSI_J; break;
            case Qt::Key_K: keyCode = kVK_ANSI_K; break;
            case Qt::Key_L: keyCode = kVK_ANSI_L; break;
            case Qt::Key_M: keyCode = kVK_ANSI_M; break;
            case Qt::Key_N: keyCode = kVK_ANSI_N; break;
            case Qt::Key_O: keyCode = kVK_ANSI_O; break;
            case Qt::Key_P: keyCode = kVK_ANSI_P; break;
            case Qt::Key_Q: keyCode = kVK_ANSI_Q; break;
            case Qt::Key_R: keyCode = kVK_ANSI_R; break;
            case Qt::Key_S: keyCode = kVK_ANSI_S; break;
            case Qt::Key_T: keyCode = kVK_ANSI_T; break;
            case Qt::Key_U: keyCode = kVK_ANSI_U; break;
            case Qt::Key_V: keyCode = kVK_ANSI_V; break;
            case Qt::Key_W: keyCode = kVK_ANSI_W; break;
            case Qt::Key_X: keyCode = kVK_ANSI_X; break;
            case Qt::Key_Y: keyCode = kVK_ANSI_Y; break;
            case Qt::Key_Z: keyCode = kVK_ANSI_Z; break;
            default:
                qWarning() << "Unsupported key for chat window shortcut:" << baseKey;
                keyCode = -1;
        }

        if (keyCode >= 0) {
            EventHotKeyID chatWindowHotKeyID;
            chatWindowHotKeyID.signature = 'htk3';
            chatWindowHotKeyID.id = 3;

            OSStatus status = RegisterEventHotKey(
                keyCode,
                modifiers,
                chatWindowHotKeyID,
                GetApplicationEventTarget(),
                0,
                &chatWindowHotKeyRef
            );

            if (status == noErr) {
                chatWindowRegistered = true;
                qInfo() << "Registered global chat window shortcut:" << chatWindowShortcut;
            } else {
                chatWindowRegistered = false;
                qWarning() << "Failed to register global shortcut" << chatWindowShortcut << "Error:" << status;
            }
        }
    }

    // Parse read aloud shortcut
    if (readAloudSeq.count() > 0) {
        int keyCode = 0;
        int modifiers = 0;

        int key = readAloudSeq[0].toCombined();

        if (key & Qt::MetaModifier) modifiers |= cmdKey;
        if (key & Qt::ShiftModifier) modifiers |= shiftKey;
        if (key & Qt::ControlModifier) modifiers |= controlKey;
        if (key & Qt::AltModifier) modifiers |= optionKey;

        int baseKey = key & ~(Qt::MetaModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);

        switch (baseKey) {
            case Qt::Key_A: keyCode = kVK_ANSI_A; break;
            case Qt::Key_B: keyCode = kVK_ANSI_B; break;
            case Qt::Key_C: keyCode = kVK_ANSI_C; break;
            case Qt::Key_D: keyCode = kVK_ANSI_D; break;
            case Qt::Key_E: keyCode = kVK_ANSI_E; break;
            case Qt::Key_F: keyCode = kVK_ANSI_F; break;
            case Qt::Key_G: keyCode = kVK_ANSI_G; break;
            case Qt::Key_H: keyCode = kVK_ANSI_H; break;
            case Qt::Key_I: keyCode = kVK_ANSI_I; break;
            case Qt::Key_J: keyCode = kVK_ANSI_J; break;
            case Qt::Key_K: keyCode = kVK_ANSI_K; break;
            case Qt::Key_L: keyCode = kVK_ANSI_L; break;
            case Qt::Key_M: keyCode = kVK_ANSI_M; break;
            case Qt::Key_N: keyCode = kVK_ANSI_N; break;
            case Qt::Key_O: keyCode = kVK_ANSI_O; break;
            case Qt::Key_P: keyCode = kVK_ANSI_P; break;
            case Qt::Key_Q: keyCode = kVK_ANSI_Q; break;
            case Qt::Key_R: keyCode = kVK_ANSI_R; break;
            case Qt::Key_S: keyCode = kVK_ANSI_S; break;
            case Qt::Key_T: keyCode = kVK_ANSI_T; break;
            case Qt::Key_U: keyCode = kVK_ANSI_U; break;
            case Qt::Key_V: keyCode = kVK_ANSI_V; break;
            case Qt::Key_W: keyCode = kVK_ANSI_W; break;
            case Qt::Key_X: keyCode = kVK_ANSI_X; break;
            case Qt::Key_Y: keyCode = kVK_ANSI_Y; break;
            case Qt::Key_Z: keyCode = kVK_ANSI_Z; break;
            default:
                qWarning() << "Unsupported key for read aloud shortcut:" << baseKey;
                keyCode = -1;
        }

        if (keyCode >= 0) {
            EventHotKeyID readAloudHotKeyID;
            readAloudHotKeyID.signature = 'htk4';
            readAloudHotKeyID.id = 4;

            OSStatus status = RegisterEventHotKey(
                keyCode,
                modifiers,
                readAloudHotKeyID,
                GetApplicationEventTarget(),
                0,
                &readAloudHotKeyRef
            );

            if (status == noErr) {
                readAloudRegistered = true;
                qInfo() << "Registered global read aloud shortcut:" << readAloudShortcut;
            } else {
                readAloudRegistered = false;
                qWarning() << "Failed to register global shortcut" << readAloudShortcut << "Error:" << status;
            }
        }
    }
#elif defined(Q_OS_LINUX)
    // Linux X11/XWayland support using XGrabKey
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        qWarning() << "============================================================";
        qWarning() << "Failed to open X11 display for global shortcuts";
        qWarning() << "";
        qWarning() << "On GNOME Wayland, please set up keyboard shortcuts manually:";
        qWarning() << "1. Open Settings → Keyboard → View and Customize Shortcuts";
        qWarning() << "2. Scroll to 'Custom Shortcuts' and click '+'";
        qWarning() << "3. Add these shortcuts:";
        qWarning() << "   Name: Ohao Screenshot";
        qWarning() << "   Command:" << QCoreApplication::applicationFilePath() << "--screenshot";
        qWarning() << "   Shortcut: Ctrl+Alt+X";
        qWarning() << "";
        qWarning() << "   Name: Ohao Toggle";
        qWarning() << "   Command:" << QCoreApplication::applicationFilePath() << "--toggle";
        qWarning() << "   Shortcut: Ctrl+Alt+H";
        qWarning() << "";
        qWarning() << "   Name: Ohao Chat Window";
        qWarning() << "   Command:" << QCoreApplication::applicationFilePath() << "--chat";
        qWarning() << "   Shortcut: Ctrl+Alt+C";
        qWarning() << "============================================================";
        return;
    }

    display = dpy;
    Window root = DefaultRootWindow(dpy);

    // Register Ctrl+Alt+X for screenshot
    KeySym screenshotKeySym = XStringToKeysym("x");
    screenshotKeycode = XKeysymToKeycode(dpy, screenshotKeySym);
    screenshotModifiers = ControlMask | Mod1Mask;  // Mod1Mask is Alt

    if (screenshotKeycode != 0) {
        // Grab the key combination
        XGrabKey(dpy, screenshotKeycode, screenshotModifiers, root, True, GrabModeAsync, GrabModeAsync);
        // Also grab with NumLock and other lock modifiers
        XGrabKey(dpy, screenshotKeycode, screenshotModifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, screenshotKeycode, screenshotModifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, screenshotKeycode, screenshotModifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);

        screenshotRegistered = true;
        qInfo() << "Registered global screenshot shortcut Ctrl+Alt+X";
    } else {
        qWarning() << "Failed to get keycode for 'x' key";
    }

    // Register Ctrl+Alt+H for toggle visibility
    KeySym toggleKeySym = XStringToKeysym("h");
    toggleKeycode = XKeysymToKeycode(dpy, toggleKeySym);
    toggleModifiers = ControlMask | Mod1Mask;  // Mod1Mask is Alt

    if (toggleKeycode != 0) {
        XGrabKey(dpy, toggleKeycode, toggleModifiers, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, toggleKeycode, toggleModifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, toggleKeycode, toggleModifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, toggleKeycode, toggleModifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);

        toggleRegistered = true;
        qInfo() << "Registered global toggle shortcut Ctrl+Alt+H";
    } else {
        qWarning() << "Failed to get keycode for 'h' key";
    }

    // Register Ctrl+Alt+C for chat window
    KeySym chatWindowKeySym = XStringToKeysym("c");
    chatWindowKeycode = XKeysymToKeycode(dpy, chatWindowKeySym);
    chatWindowModifiers = ControlMask | Mod1Mask;  // Mod1Mask is Alt

    if (chatWindowKeycode != 0) {
        XGrabKey(dpy, chatWindowKeycode, chatWindowModifiers, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, chatWindowKeycode, chatWindowModifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, chatWindowKeycode, chatWindowModifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, chatWindowKeycode, chatWindowModifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);

        chatWindowRegistered = true;
        qInfo() << "Registered global chat window shortcut Ctrl+Alt+C";
    } else {
        qWarning() << "Failed to get keycode for 'c' key";
    }

    // Register Ctrl+Alt+A for read aloud
    KeySym readAloudKeySym = XStringToKeysym("a");
    readAloudKeycode = XKeysymToKeycode(dpy, readAloudKeySym);
    readAloudModifiers = ControlMask | Mod1Mask;  // Mod1Mask is Alt

    if (readAloudKeycode != 0) {
        XGrabKey(dpy, readAloudKeycode, readAloudModifiers, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, readAloudKeycode, readAloudModifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, readAloudKeycode, readAloudModifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, readAloudKeycode, readAloudModifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);

        readAloudRegistered = true;
        qInfo() << "Registered global read aloud shortcut Ctrl+Alt+A";
    } else {
        qWarning() << "Failed to get keycode for 'a' key";
    }

    XSync(dpy, False);

    if (!screenshotRegistered && !toggleRegistered) {
        qWarning() << "Failed to register any global shortcuts";
        qWarning() << "On Wayland, you may need to use your desktop environment's keyboard settings";
    }
#else
    qWarning() << "Global shortcuts are not supported on this platform";
#endif
}

void GlobalShortcutManager::unregisterShortcuts()
{
#ifdef Q_OS_WIN
    if (screenshotRegistered) {
        UnregisterHotKey(nullptr, screenshotHotkeyId);
        screenshotRegistered = false;
    }
    if (toggleRegistered) {
        UnregisterHotKey(nullptr, toggleHotkeyId);
        toggleRegistered = false;
    }
    if (chatWindowRegistered) {
        UnregisterHotKey(nullptr, chatWindowHotkeyId);
        chatWindowRegistered = false;
    }
    if (readAloudRegistered) {
        UnregisterHotKey(nullptr, readAloudHotkeyId);
        readAloudRegistered = false;
    }
#elif defined(Q_OS_MACOS)
    if (screenshotRegistered) {
        UnregisterEventHotKey(screenshotHotKeyRef);
        screenshotRegistered = false;
    }
    if (toggleRegistered) {
        UnregisterEventHotKey(toggleHotKeyRef);
        toggleRegistered = false;
    }
    if (chatWindowRegistered) {
        UnregisterEventHotKey(chatWindowHotKeyRef);
        chatWindowRegistered = false;
    }
    if (readAloudRegistered) {
        UnregisterEventHotKey(readAloudHotKeyRef);
        readAloudRegistered = false;
    }
    if (eventHandlerRef) {
        RemoveEventHandler(eventHandlerRef);
        eventHandlerRef = nullptr;
    }
    if (eventHandlerUPP) {
        DisposeEventHandlerUPP(eventHandlerUPP);
        eventHandlerUPP = nullptr;
    }
#elif defined(Q_OS_LINUX)
    if (display) {
        Display *dpy = static_cast<Display*>(display);
        Window root = DefaultRootWindow(dpy);

        if (screenshotRegistered && screenshotKeycode != 0) {
            XUngrabKey(dpy, screenshotKeycode, screenshotModifiers, root);
            XUngrabKey(dpy, screenshotKeycode, screenshotModifiers | Mod2Mask, root);
            XUngrabKey(dpy, screenshotKeycode, screenshotModifiers | LockMask, root);
            XUngrabKey(dpy, screenshotKeycode, screenshotModifiers | Mod2Mask | LockMask, root);
            screenshotRegistered = false;
        }

        if (toggleRegistered && toggleKeycode != 0) {
            XUngrabKey(dpy, toggleKeycode, toggleModifiers, root);
            XUngrabKey(dpy, toggleKeycode, toggleModifiers | Mod2Mask, root);
            XUngrabKey(dpy, toggleKeycode, toggleModifiers | LockMask, root);
            XUngrabKey(dpy, toggleKeycode, toggleModifiers | Mod2Mask | LockMask, root);
            toggleRegistered = false;
        }

        if (chatWindowRegistered && chatWindowKeycode != 0) {
            XUngrabKey(dpy, chatWindowKeycode, chatWindowModifiers, root);
            XUngrabKey(dpy, chatWindowKeycode, chatWindowModifiers | Mod2Mask, root);
            XUngrabKey(dpy, chatWindowKeycode, chatWindowModifiers | LockMask, root);
            XUngrabKey(dpy, chatWindowKeycode, chatWindowModifiers | Mod2Mask | LockMask, root);
            chatWindowRegistered = false;
        }

        if (readAloudRegistered && readAloudKeycode != 0) {
            XUngrabKey(dpy, readAloudKeycode, readAloudModifiers, root);
            XUngrabKey(dpy, readAloudKeycode, readAloudModifiers | Mod2Mask, root);
            XUngrabKey(dpy, readAloudKeycode, readAloudModifiers | LockMask, root);
            XUngrabKey(dpy, readAloudKeycode, readAloudModifiers | Mod2Mask | LockMask, root);
            readAloudRegistered = false;
        }

        XCloseDisplay(dpy);
        display = nullptr;
    }
#endif
}

bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (!isEnabled) {
        return false;
    }

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            int hotkeyId = static_cast<int>(msg->wParam);

            // Use ShortcutConfig to map hotkey ID to action
            const auto& config = ShortcutConfig::instance();
            ShortcutConfig::ShortcutAction action = config.getActionByHotkeyId(hotkeyId);

            qDebug() << "========================================";
            qDebug() << "HOTKEY PRESSED!";
            qDebug() << "  Received ID:" << hotkeyId;

            // Debug: Check what each action's hotkey ID is
            qDebug() << "  Screenshot should be ID:" << config.getShortcut(ShortcutConfig::Screenshot).hotkeyId;
            qDebug() << "  Toggle should be ID:" << config.getShortcut(ShortcutConfig::ToggleVisibility).hotkeyId;
            qDebug() << "  Chat should be ID:" << config.getShortcut(ShortcutConfig::ChatWindow).hotkeyId;
            qDebug() << "  ReadAloud should be ID:" << config.getShortcut(ShortcutConfig::ReadAloud).hotkeyId;

            qDebug() << "  Mapped to action enum:" << static_cast<int>(action);
            qDebug() << "  (Screenshot=0, Toggle=1, Chat=2, ReadAloud=3)";

            // Log which signal will be emitted
            QString signalName;
            switch (action) {
                case ShortcutConfig::Screenshot: signalName = "screenshotRequested"; break;
                case ShortcutConfig::ToggleVisibility: signalName = "toggleVisibilityRequested"; break;
                case ShortcutConfig::ChatWindow: signalName = "chatWindowRequested"; break;
                case ShortcutConfig::ReadAloud: signalName = "readAloudRequested"; break;
                default: signalName = "UNKNOWN"; break;
            }
            qDebug() << "  Will emit signal:" << signalName;
            qDebug() << "========================================";

            // Write to debug file (use WriteOnly to overwrite each time for simplicity)
            static int pressCount = 0;
            pressCount++;
            QFile debugFile(QString("C:/Users/djmax/Desktop/shortcut-press-%1.txt").arg(pressCount));
            if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&debugFile);
                out << "Hotkey Press #" << pressCount << "\n";
                out << "==================\n\n";
                out << "Received ID: " << hotkeyId << "\n";
                out << "Mapped to action enum: " << static_cast<int>(action) << "\n";
                out << "Signal to emit: " << signalName << "\n\n";
                out << "Expected IDs:\n";
                out << "  Screenshot: " << config.getShortcut(ShortcutConfig::Screenshot).hotkeyId << "\n";
                out << "  Toggle: " << config.getShortcut(ShortcutConfig::ToggleVisibility).hotkeyId << "\n";
                out << "  Chat: " << config.getShortcut(ShortcutConfig::ChatWindow).hotkeyId << "\n";
                out << "  ReadAloud: " << config.getShortcut(ShortcutConfig::ReadAloud).hotkeyId << "\n";
                debugFile.close();
            }

            // Dispatch based on action enum
            switch (action) {
                case ShortcutConfig::Screenshot:
                    qDebug() << "→ Emitting screenshotRequested";
                    emit screenshotRequested();
                    break;
                case ShortcutConfig::ToggleVisibility:
                    qDebug() << "→ Emitting toggleVisibilityRequested";
                    emit toggleVisibilityRequested();
                    break;
                case ShortcutConfig::ChatWindow:
                    qDebug() << "→ Emitting chatWindowRequested";
                    emit chatWindowRequested();
                    break;
                case ShortcutConfig::ReadAloud:
                    qDebug() << "→ Emitting readAloudRequested";
                    emit readAloudRequested();
                    break;
                default:
                    qWarning() << "Unknown action for hotkey ID:" << hotkeyId;
                    return false;
            }

            if (result) {
                *result = 0;
            }
            return true;
        }
    }
#elif defined(Q_OS_LINUX)
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *ev = static_cast<xcb_generic_event_t *>(message);

        if ((ev->response_type & ~0x80) == XCB_KEY_PRESS) {
            xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t*>(ev);

            unsigned int cleanState = keyEvent->state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

            // Check for screenshot shortcut (Ctrl+Shift+S)
            if (keyEvent->detail == screenshotKeycode && cleanState == screenshotModifiers) {
                emit screenshotRequested();
                if (result) {
                    *result = 0;
                }
                return true;
            }

            // Check for toggle shortcut (Ctrl+Shift+H)
            if (keyEvent->detail == toggleKeycode && cleanState == toggleModifiers) {
                emit toggleVisibilityRequested();
                if (result) {
                    *result = 0;
                }
                return true;
            }

            // Check for chat window shortcut (Ctrl+Alt+C)
            if (keyEvent->detail == chatWindowKeycode && cleanState == chatWindowModifiers) {
                emit chatWindowRequested();
                if (result) {
                    *result = 0;
                }
                return true;
            }

            // Check for read aloud shortcut (Ctrl+Alt+A)
            if (keyEvent->detail == readAloudKeycode && cleanState == readAloudModifiers) {
                emit readAloudRequested();
                if (result) {
                    *result = 0;
                }
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}

#ifdef Q_OS_MACOS
OSStatus GlobalShortcutManager::hotKeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    Q_UNUSED(nextHandler);
    
    GlobalShortcutManager *manager = static_cast<GlobalShortcutManager*>(userData);
    
    // Check if shortcuts are enabled
    if (!manager->isEnabled) {
        return eventNotHandledErr;
    }
    
    EventHotKeyID hotKeyID;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hotKeyID), nullptr, &hotKeyID);

    if (hotKeyID.signature == 'htk1' && hotKeyID.id == 1) {
        // Screenshot hotkey (Cmd+Shift+S)
        emit manager->screenshotRequested();
        return noErr;
    } else if (hotKeyID.signature == 'htk2' && hotKeyID.id == 2) {
        // Toggle visibility hotkey (Cmd+Shift+H)
        emit manager->toggleVisibilityRequested();
        return noErr;
    } else if (hotKeyID.signature == 'htk3' && hotKeyID.id == 3) {
        // Chat window hotkey (Cmd+Shift+C)
        emit manager->chatWindowRequested();
        return noErr;
    } else if (hotKeyID.signature == 'htk4' && hotKeyID.id == 4) {
        // Read aloud hotkey (Ctrl+Alt+A)
        emit manager->readAloudRequested();
        return noErr;
    }

    return eventNotHandledErr;
}
#endif

void GlobalShortcutManager::setEnabled(bool enabled)
{
    isEnabled = enabled;
    qDebug() << "Global shortcuts" << (enabled ? "enabled" : "disabled");
}

void GlobalShortcutManager::reloadShortcuts()
{
    qDebug() << "Reloading global shortcuts...";
    registerShortcuts();
}