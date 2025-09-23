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
    registerShortcut();
#else
    qWarning() << "Global shortcuts are only implemented on Windows for now.";
#endif
}

GlobalShortcutManager::~GlobalShortcutManager()
{
#ifdef Q_OS_WIN
    unregisterShortcut();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
#endif
}

void GlobalShortcutManager::registerShortcut()
{
#ifdef Q_OS_WIN
    unregisterShortcut();

    const UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
    const UINT virtualKey = 0x53; // 'S'

    if (RegisterHotKey(nullptr, hotkeyId, modifiers, virtualKey)) {
        registered = true;
        qInfo() << "Registered global screenshot shortcut Ctrl+Shift+S";
    } else {
        registered = false;
        qWarning() << "Failed to register global shortcut Ctrl+Shift+S. Error:" << GetLastError();
    }
#endif
}

void GlobalShortcutManager::unregisterShortcut()
{
#ifdef Q_OS_WIN
    if (registered) {
        UnregisterHotKey(nullptr, hotkeyId);
        registered = false;
    }
#endif
}

bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == hotkeyId) {
            emit screenshotRequested();
            if (result) {
                *result = 0;
            }
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
