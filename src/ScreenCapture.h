#pragma once

#include <QObject>
#include <QPixmap>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>

class ScreenCapture : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);
    ~ScreenCapture();

    // Main capture method - automatically selects best method for platform
    QPixmap captureScreen();

signals:
    void captureCompleted(const QPixmap &screenshot);
    void captureFailed(const QString &error);

private slots:
    void handlePortalResponse(uint response, const QVariantMap &results);

private:
    // Platform-specific implementations
#ifdef Q_OS_LINUX
    QPixmap captureLinux();
    QPixmap captureWayland();
    QPixmap captureX11();
    bool isWayland() const;
#endif

#ifdef Q_OS_WIN
    QPixmap captureWindows();
#endif

#ifdef Q_OS_MAC
    QPixmap captureMacOS();
#endif

    // DBus portal helpers for Wayland
    bool callScreenshotPortal();
    QPixmap loadScreenshotFromUri(const QString &uri);

    QEventLoop *m_eventLoop;
    QPixmap m_screenshot;
    QString m_errorMessage;
};