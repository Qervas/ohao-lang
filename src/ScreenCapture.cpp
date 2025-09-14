#include "ScreenCapture.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QFile>
#include <QUrl>
#include <QUuid>
#include <QTimer>

ScreenCapture::ScreenCapture(QObject *parent)
    : QObject(parent)
    , m_eventLoop(nullptr)
{
}

ScreenCapture::~ScreenCapture()
{
    if (m_eventLoop && m_eventLoop->isRunning()) {
        m_eventLoop->quit();
    }
    delete m_eventLoop;
}

QPixmap ScreenCapture::captureScreen()
{
    qDebug() << "ScreenCapture: Starting screen capture";

#ifdef Q_OS_LINUX
    return captureLinux();
#elif defined(Q_OS_WIN)
    return captureWindows();
#elif defined(Q_OS_MAC)
    return captureMacOS();
#else
    qWarning() << "ScreenCapture: Unsupported platform";
    return QPixmap();
#endif
}

#ifdef Q_OS_LINUX
QPixmap ScreenCapture::captureLinux()
{
    if (isWayland()) {
        qDebug() << "ScreenCapture: Detected Wayland, using portal";
        return captureWayland();
    } else {
        qDebug() << "ScreenCapture: Detected X11, using native Qt";
        return captureX11();
    }
}

bool ScreenCapture::isWayland() const
{
    // Check if we're running on Wayland
    QString platformName = QGuiApplication::platformName();
    qDebug() << "Platform name:" << platformName;
    return platformName.contains("wayland", Qt::CaseInsensitive);
}

QPixmap ScreenCapture::captureX11()
{
    // X11 can use Qt's native screen grabbing
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "ScreenCapture: No primary screen found";
        return QPixmap();
    }

    QPixmap screenshot = screen->grabWindow(0);
    if (screenshot.isNull()) {
        qWarning() << "ScreenCapture: Failed to grab window on X11";
    } else {
        qDebug() << "ScreenCapture: X11 capture successful, size:" << screenshot.size();
    }
    return screenshot;
}

QPixmap ScreenCapture::captureWayland()
{
    qDebug() << "ScreenCapture: Using xdg-desktop-portal for Wayland";

    // Use xdg-desktop-portal for Wayland
    if (!callScreenshotPortal()) {
        qWarning() << "ScreenCapture: Portal call failed:" << m_errorMessage;
        return QPixmap();
    }

    return m_screenshot;
}

bool ScreenCapture::callScreenshotPortal()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    // Create unique token for this request
    QString token = QUuid::createUuid().toString().mid(1, 36).replace("-", "_");
    QString senderName = QDBusConnection::sessionBus().baseService().mid(1).replace(".", "_");
    QString objectPath = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(senderName, token);

    qDebug() << "ScreenCapture: Request object path:" << objectPath;

    // Watch for the response
    bus.connect("org.freedesktop.portal.Desktop",
                objectPath,
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(handlePortalResponse(uint,QVariantMap)));

    // Call the Screenshot method
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        "Screenshot"
    );

    // Prepare options
    QVariantMap options;
    options["handle_token"] = token;
    options["interactive"] = false;  // Don't show dialog

    message << QString() << options;  // parent_window (empty) and options

    qDebug() << "ScreenCapture: Calling Screenshot portal";
    QDBusPendingCall pendingCall = bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall, this);

    // Create event loop to wait for response
    m_eventLoop = new QEventLoop(this);
    m_screenshot = QPixmap();
    m_errorMessage.clear();

    // Set timeout
    QTimer::singleShot(10000, m_eventLoop, &QEventLoop::quit);

    connect(watcher, &QDBusPendingCallWatcher::finished, [this, watcher]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            m_errorMessage = reply.error().message();
            qWarning() << "ScreenCapture: DBus call error:" << m_errorMessage;
            m_eventLoop->quit();
        } else {
            qDebug() << "ScreenCapture: Portal call succeeded, waiting for response";
        }
        watcher->deleteLater();
    });

    // Wait for response
    m_eventLoop->exec();

    delete m_eventLoop;
    m_eventLoop = nullptr;

    return !m_screenshot.isNull();
}

void ScreenCapture::handlePortalResponse(uint response, const QVariantMap &results)
{
    qDebug() << "ScreenCapture: Portal response received, code:" << response;
    qDebug() << "ScreenCapture: Results:" << results;

    if (response == 0) {  // Success
        if (results.contains("uri")) {
            QString uri = results["uri"].toString();
            qDebug() << "ScreenCapture: Screenshot URI:" << uri;
            m_screenshot = loadScreenshotFromUri(uri);
        } else {
            m_errorMessage = "No URI in portal response";
            qWarning() << "ScreenCapture:" << m_errorMessage;
        }
    } else if (response == 1) {  // User cancelled
        m_errorMessage = "User cancelled screenshot";
        qDebug() << "ScreenCapture:" << m_errorMessage;
    } else {  // Other error
        m_errorMessage = QString("Portal error code: %1").arg(response);
        qWarning() << "ScreenCapture:" << m_errorMessage;
    }

    if (m_eventLoop && m_eventLoop->isRunning()) {
        m_eventLoop->quit();
    }
}

QPixmap ScreenCapture::loadScreenshotFromUri(const QString &uri)
{
    QUrl url(uri);
    QString path = url.toLocalFile();

    if (path.isEmpty()) {
        path = uri;  // Try as direct path if not a file:// URI
    }

    qDebug() << "ScreenCapture: Loading screenshot from:" << path;

    QPixmap pixmap;
    if (pixmap.load(path)) {
        qDebug() << "ScreenCapture: Successfully loaded screenshot, size:" << pixmap.size();

        // Handle DPI scaling - portal captures at native resolution
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->geometry();
            qreal devicePixelRatio = screen->devicePixelRatio();

            qDebug() << "ScreenCapture: Screen geometry:" << screenGeometry;
            qDebug() << "ScreenCapture: Device pixel ratio:" << devicePixelRatio;

            // If captured image is larger than screen geometry, scale it down
            if (pixmap.width() > screenGeometry.width() || pixmap.height() > screenGeometry.height()) {
                qDebug() << "ScreenCapture: Scaling down from" << pixmap.size() << "to" << screenGeometry.size();
                pixmap = pixmap.scaled(screenGeometry.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                qDebug() << "ScreenCapture: Final scaled size:" << pixmap.size();
            }
        }

        // Clean up temporary file if it exists
        if (path.startsWith("/tmp/")) {
            QFile::remove(path);
        }
    } else {
        qWarning() << "ScreenCapture: Failed to load screenshot from" << path;
    }

    return pixmap;
}
#endif // Q_OS_LINUX

#ifdef Q_OS_WIN
QPixmap ScreenCapture::captureWindows()
{
    // Windows implementation - uses native Qt which works on Windows
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "ScreenCapture: No primary screen found";
        return QPixmap();
    }

    QPixmap screenshot = screen->grabWindow(0);
    qDebug() << "ScreenCapture: Windows capture, size:" << screenshot.size();
    return screenshot;
}
#endif // Q_OS_WIN

#ifdef Q_OS_MAC
QPixmap ScreenCapture::captureMacOS()
{
    // macOS implementation - uses native Qt (may need permissions)
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "ScreenCapture: No primary screen found";
        return QPixmap();
    }

    QPixmap screenshot = screen->grabWindow(0);
    qDebug() << "ScreenCapture: macOS capture, size:" << screenshot.size();
    return screenshot;
}
#endif // Q_OS_MAC