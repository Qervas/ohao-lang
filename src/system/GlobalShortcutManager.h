#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QByteArray>

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#endif

class GlobalShortcutManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject *parent = nullptr);
    ~GlobalShortcutManager() override;
    
    void setEnabled(bool enabled);
    void reloadShortcuts();

signals:
    void screenshotRequested();
    void toggleVisibilityRequested();
    void chatWindowRequested();

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void registerShortcuts();
    void unregisterShortcuts();
    
    bool isEnabled = true;

#ifdef Q_OS_WIN
    int screenshotHotkeyId = 1;
    int toggleHotkeyId = 2;
    int chatWindowHotkeyId = 3;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;
    bool chatWindowRegistered = false;
#endif

#ifdef Q_OS_MACOS
    EventHotKeyRef screenshotHotKeyRef;
    EventHotKeyRef toggleHotKeyRef;
    EventHotKeyRef chatWindowHotKeyRef;
    EventHandlerUPP eventHandlerUPP = nullptr;
    EventHandlerRef eventHandlerRef = nullptr;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;
    bool chatWindowRegistered = false;

    static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
#endif

#ifdef Q_OS_LINUX
    // X11 global shortcut support
    void *display = nullptr;
    unsigned int screenshotKeycode = 0;
    unsigned int toggleKeycode = 0;
    unsigned int chatWindowKeycode = 0;
    unsigned int screenshotModifiers = 0;
    unsigned int toggleModifiers = 0;
    unsigned int chatWindowModifiers = 0;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;
    bool chatWindowRegistered = false;

    void startX11Monitoring();
    void stopX11Monitoring();
#endif
};
