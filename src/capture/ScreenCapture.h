#pragma once

#include <QObject>
#include <QPixmap>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>
#include <QMap>
#include <QVariant>

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

#ifdef Q_OS_LINUX
private slots:
    void handlePortalResponse(uint response, const QMap<QString, QVariant> &results);
#endif

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

    // Image preprocessing for OCR optimization
    QPixmap preprocessForOCR(const QPixmap &originalPixmap);
    QPixmap enhanceContrast(const QPixmap &pixmap);
    QPixmap sharpenImage(const QPixmap &pixmap);

    // Resolution detection and handling
    struct ScreenInfo {
        QSize logicalSize;      // Screen size as reported by Qt
        QSize physicalSize;     // Actual screen resolution
        qreal devicePixelRatio; // DPI scaling factor
        QSize nativeSize;       // Physical pixels (physicalSize * devicePixelRatio)
    };
    ScreenInfo detectScreenResolution();
    QPixmap ensureHighestQuality(const QPixmap &pixmap, const ScreenInfo &screenInfo);

    QEventLoop *m_eventLoop;
    QPixmap m_screenshot;
    QString m_errorMessage;
};