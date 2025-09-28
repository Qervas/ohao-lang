#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QByteArray>

class GlobalShortcutManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject *parent = nullptr);
    ~GlobalShortcutManager() override;

signals:
    void screenshotRequested();
    void toggleVisibilityRequested();

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void registerShortcuts();
    void unregisterShortcuts();

#ifdef Q_OS_WIN
    int screenshotHotkeyId = 1;
    int toggleHotkeyId = 2;
    bool screenshotRegistered = false;
    bool toggleRegistered = false;
#endif
};
