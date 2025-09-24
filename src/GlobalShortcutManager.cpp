#include "GlobalShortcutManager.h"

#include <QCoreApplication>
#include <QDebug>

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
#ifdef Q_OS_WIN
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    registerShortcuts();
#else
    qWarning() << "Global shortcuts are only implemented on Windows for now.";
#endif
}

GlobalShortcutManager::~GlobalShortcutManager()
{
#ifdef Q_OS_WIN
    unregisterShortcuts();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
#endif
}

void GlobalShortcutManager::registerShortcuts()
{
#ifdef Q_OS_WIN
    unregisterShortcuts();

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
#endif
}

bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
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
