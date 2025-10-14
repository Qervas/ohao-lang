#include "GlobalShortcutManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QKeySequence>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
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
    // Register Ctrl+Shift+S for screenshot
    const UINT screenshotModifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
    const UINT screenshotKey = 0x53; // 'S'

    if (RegisterHotKey(nullptr, screenshotHotkeyId, screenshotModifiers, screenshotKey)) {
        screenshotRegistered = true;
        qInfo() << "Registered global screenshot shortcut Ctrl+Shift+S";
    } else {
        screenshotRegistered = false;
        qWarning() << "Failed to register global shortcut Ctrl+Shift+S. Error:" << GetLastError();
    }

    // Register Ctrl+Shift+H for toggle visibility
    const UINT toggleModifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
    const UINT toggleKey = 0x48; // 'H'

    if (RegisterHotKey(nullptr, toggleHotkeyId, toggleModifiers, toggleKey)) {
        toggleRegistered = true;
        qInfo() << "Registered global visibility toggle shortcut Ctrl+Shift+H";
    } else {
        toggleRegistered = false;
        qWarning() << "Failed to register global shortcut Ctrl+Shift+H. Error:" << GetLastError();
    }
#elif defined(Q_OS_MACOS)
    // Load custom shortcuts from settings
    QSettings settings;
    QString screenshotShortcut = settings.value("shortcuts/screenshot", "Meta+Shift+X").toString();
    QString toggleShortcut = settings.value("shortcuts/toggle", "Meta+Shift+Z").toString();
    
    QKeySequence screenshotSeq(screenshotShortcut);
    QKeySequence toggleSeq(toggleShortcut);
    
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
#else
    // For Linux, global shortcuts require special system permissions
    // or desktop environment specific APIs
    qWarning() << "Global shortcuts are currently only fully supported on Windows and macOS.";
    qWarning() << "On Linux, please use your desktop environment's shortcut settings to bind:";
    qWarning() << "  Ctrl+Shift+S -> Launch this application with --screenshot argument";
    qWarning() << "  Ctrl+Shift+H -> Launch this application with --toggle argument";
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
#elif defined(Q_OS_MACOS)
    if (screenshotRegistered) {
        UnregisterEventHotKey(screenshotHotKeyRef);
        screenshotRegistered = false;
    }
    if (toggleRegistered) {
        UnregisterEventHotKey(toggleHotKeyRef);
        toggleRegistered = false;
    }
    if (eventHandlerRef) {
        RemoveEventHandler(eventHandlerRef);
        eventHandlerRef = nullptr;
    }
    if (eventHandlerUPP) {
        DisposeEventHandlerUPP(eventHandlerUPP);
        eventHandlerUPP = nullptr;
    }
#endif
}

bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (!isEnabled) {
        return false;
    }
    
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == screenshotHotkeyId) {
                emit screenshotRequested();
                if (result) {
                    *result = 0;
                }
                return true;
            } else if (msg->wParam == toggleHotkeyId) {
                emit toggleVisibilityRequested();
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