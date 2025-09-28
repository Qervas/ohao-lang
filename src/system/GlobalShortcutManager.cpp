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
#else
    // For non-Windows platforms, global shortcuts require special system permissions
    // or desktop environment specific APIs
    qWarning() << "Global shortcuts are currently only fully supported on Windows.";
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