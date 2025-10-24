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

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void registerShortcuts();
    void unregisterShortcuts();
    
    bool isEnabled = true;

#ifdef Q_OS_WIN
    int screenshotHotkeyId = 1;
    int toggleHotkeyId = 2;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;
#endif

#ifdef Q_OS_MACOS
    EventHotKeyRef screenshotHotKeyRef;
    EventHotKeyRef toggleHotKeyRef;
    EventHandlerUPP eventHandlerUPP = nullptr;
    EventHandlerRef eventHandlerRef = nullptr;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;

    static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
#endif

#ifdef Q_OS_LINUX
    // X11 global shortcut support
    void *display = nullptr;
    unsigned int screenshotKeycode = 0;
    unsigned int toggleKeycode = 0;
    unsigned int screenshotModifiers = 0;
    unsigned int toggleModifiers = 0;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;

    void startX11Monitoring();
    void stopX11Monitoring();
#endif
};
