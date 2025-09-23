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

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void registerShortcut();
    void unregisterShortcut();

#ifdef Q_OS_WIN
    int hotkeyId = 1;
    bool registered = false;
#endif
};
