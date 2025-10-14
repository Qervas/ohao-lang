#include "FloatingWidget.h"
#include "GlobalShortcutManager.h"
#include "../screenshot/ScreenshotWidget.h"
#include "ScreenCapture.h"
#include "SettingsWindow.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>
#include <QTransform>
#include <QWindow>
#include <QLocalServer>
#include <QLocalSocket>

FloatingWidget::FloatingWidget(QWidget *parent)
    : QWidget(parent), isDragging(false), currentScale(1.0), currentOpacity(200)
{
    qDebug() << "Creating FloatingWidget...";
    setupUI();
    applyModernStyle();

    // Initialize global shortcut manager (Windows and macOS)
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    shortcutManager = new GlobalShortcutManager(this);
    connect(shortcutManager, &GlobalShortcutManager::screenshotRequested, 
            this, &FloatingWidget::takeScreenshot);
    connect(shortcutManager, &GlobalShortcutManager::toggleVisibilityRequested,
            this, &FloatingWidget::toggleVisibility);
    qDebug() << "Global shortcut manager initialized";
#endif

    // Setup local server for single instance support
    localServer = new QLocalServer(this);
    connect(localServer, &QLocalServer::newConnection, this, &FloatingWidget::handleNewConnection);
    if (!localServer->listen("ohao-lang-server")) {
        qDebug() << "Failed to start local server:" << localServer->errorString();
    } else {
        qDebug() << "Local server started successfully";
    }

    // Restore saved position or fall back to default using a timer to ensure it's set after initialization
    QTimer::singleShot(100, [this]() {
        qDebug() << "QSettings file location:" << settings.fileName();

        QPoint savedPos = settings.value("floatingWidget/pos", QPoint(-1, -1)).toPoint();
        qDebug() << "Read saved position from settings:" << savedPos;

        if (savedPos != QPoint(-1, -1)) {
            // Clamp to screen
            if (QScreen *screen = QApplication::primaryScreen()) {
                QRect g = screen->geometry();
                savedPos.setX(qBound(0, savedPos.x(), g.width() - width()));
                savedPos.setY(qBound(0, savedPos.y(), g.height() - height()));
            }
            qDebug() << "Restoring saved position (clamped):" << savedPos;
            move(savedPos);
            qDebug() << "Widget moved to:" << pos();
            return;
        }
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QPoint initialPos(50, 50); // Top-left corner with some margin
            qDebug() << "No saved position. Using initial position:" << initialPos;
            move(initialPos);
            qDebug() << "Widget moved to initial position:" << pos();
        }
    });
    
    qDebug() << "FloatingWidget created successfully";
}

void FloatingWidget::setupUI()
{
    // Make widget frameless and always on top - force it to be a top-level window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("floatingWidget");
    
    // Enable mouse tracking for better drag experience
    setMouseTracking(true);

    // Set size
    setFixedSize(140, 60);
    
    // Styling now provided globally by ThemeManager via #floatingWidget
    
    // Force the widget to accept mouse events
    setAttribute(Qt::WA_NoMouseReplay);
    
    // Force the widget to be a top-level window
    setParent(nullptr);

    // Create horizontal layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Screenshot button
    screenshotBtn = new QPushButton(this);
    screenshotBtn->setFixedSize(50, 40);
    screenshotBtn->setCursor(Qt::PointingHandCursor);
    screenshotBtn->setObjectName("floatingButton");
    connect(screenshotBtn, &QPushButton::clicked, this, &FloatingWidget::takeScreenshot);

    // Settings button
    settingsBtn = new QPushButton(this);
    settingsBtn->setFixedSize(50, 40);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setObjectName("floatingButton");
    connect(settingsBtn, &QPushButton::clicked, this, &FloatingWidget::openSettings);

    layout->addWidget(screenshotBtn);
    layout->addWidget(settingsBtn);

    // Add drop shadow
    shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 80));
    shadowEffect->setOffset(0, 4);
    setGraphicsEffect(shadowEffect);

    // Setup hover animations
    hoverAnimation = new QPropertyAnimation(this, "windowOpacity");
    hoverAnimation->setDuration(200);

    // Setup scale animation for buttons
    scaleAnimation = new QPropertyAnimation();
    scaleAnimation->setDuration(150);

    // Debounced saver for position
    savePosTimer = new QTimer(this);
    savePosTimer->setSingleShot(true);
    savePosTimer->setInterval(150);
    connect(savePosTimer, &QTimer::timeout, this, [this]() {
        QPoint p = this->pos();
        if (QScreen *screen = QApplication::primaryScreen()) {
            QRect g = screen->geometry();
            p.setX(qBound(0, p.x(), g.width() - width()));
            p.setY(qBound(0, p.y(), g.height() - height()));
        }
        settings.setValue("floatingWidget/pos", p);
        settings.sync();
        qDebug() << "Debounced save position:" << p;
    });
}

void FloatingWidget::applyModernStyle()
{
    // Set button icons (styling handled by ThemeManager)
    screenshotBtn->setText("📸");
    settingsBtn->setText("⚙️");
}

void FloatingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRect r = rect();
    int radius = 18;

    QColor base = palette().window().color();
    if (base.alpha() == 255) base.setAlpha(200); // ensure translucency
    bool hl = property("highlight").toString() == QStringLiteral("true");
    QColor fill = base;
    if (hl) {
        fill = fill.lighter(115);
        fill.setAlpha(qMin(255, fill.alpha() + 25));
    }

    QPainterPath path;
    path.addRoundedRect(r.adjusted(0,0,-1,-1), radius, radius);
    p.fillPath(path, fill);
    if (hl) {
        QPen pen(QColor(255,255,255,90));
        pen.setWidth(2);
        p.setPen(pen);
        p.drawPath(path);
    }
}

void FloatingWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    animateHover(true);
    setCursor(Qt::OpenHandCursor);
    setProperty("highlight", "true");
    style()->unpolish(this); style()->polish(this); update();
}

void FloatingWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    animateHover(false);
    setCursor(Qt::ArrowCursor);
    setProperty("highlight", "false");
    style()->unpolish(this); style()->polish(this); update();
}

void FloatingWidget::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "Mouse press event received at:" << event->pos() << "Button:" << event->button();
    
    if (event->button() == Qt::LeftButton) {
        // Wayland: must request compositor-driven move; manual move() is ignored
        const QString platformName = QGuiApplication::platformName();
        if (platformName.contains("wayland", Qt::CaseInsensitive) && windowHandle()) {
            qDebug() << "Starting system move via compositor (Wayland)";
            isWaylandSystemMove = true;
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }

        // Other platforms: perform manual dragging
        isDragging = true;
        dragPosition = event->pos(); // Use local position instead of global
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        
        // Change opacity to show click is working
        currentOpacity = 100;
        update();
        
        qDebug() << "Started dragging from position:" << dragPosition << "Global pos:" << event->globalPosition().toPoint();
    } else {
        event->ignore();
    }
}

void FloatingWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging) {
        // If on Wayland, system move is already in progress; ignore manual move
        const QString platformName = QGuiApplication::platformName();
        if (platformName.contains("wayland", Qt::CaseInsensitive)) {
            event->accept();
            return;
        }
        QPoint newPos = event->globalPosition().toPoint() - dragPosition;

        // Keep widget on screen
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->geometry();
            newPos.setX(qMax(0, qMin(newPos.x(), screenGeometry.width() - width())));
            newPos.setY(qMax(0, qMin(newPos.y(), screenGeometry.height() - height())));
        }

        move(newPos);
        if (savePosTimer) savePosTimer->start();
        event->accept();
    } else {
        event->ignore();
    }
}

void FloatingWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (isDragging || isWaylandSystemMove)) {
        isDragging = false;
        isWaylandSystemMove = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        
        // Restore opacity
        currentOpacity = 200;
        update();
        
        // Persist current position
        // Clamp to current screen bounds to avoid saving off-screen
        QPoint currentPos = this->pos();
        if (QScreen *screen = QApplication::primaryScreen()) {
            QRect g = screen->geometry();
            currentPos.setX(qBound(0, currentPos.x(), g.width() - width()));
            currentPos.setY(qBound(0, currentPos.y(), g.height() - height()));
        }
        settings.setValue("floatingWidget/pos", currentPos);
        settings.sync(); // Force write to disk

        qDebug() << "Stopped dragging at position:" << currentPos;
        qDebug() << "Saved position to settings file:" << settings.fileName();
        qDebug() << "Verify saved position:" << settings.value("floatingWidget/pos").toPoint();
    } else {
        event->ignore();
    }
}

void FloatingWidget::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (isDragging || isWaylandSystemMove) {
        if (savePosTimer) savePosTimer->start();
    }
}

void FloatingWidget::animateHover(bool hover)
{
    if (hover) {
        currentOpacity = 255;
        currentScale = 1.05; // Subtle scale up
        shadowEffect->setBlurRadius(30);
        QPalette themePalette = ThemeManager::instance().getCurrentPalette();
        QColor accentColor = themePalette.color(QPalette::Highlight);
        accentColor.setAlpha(100);
        shadowEffect->setColor(accentColor); // Theme accent glow on hover
        shadowEffect->setOffset(0, 6);

        // Note: Qt stylesheets don't support CSS transform
        // Animation will be handled through the widget scaling instead
    } else {
        currentOpacity = 200;
        currentScale = 1.0;
        shadowEffect->setBlurRadius(20);
        shadowEffect->setColor(QColor(0, 0, 0, 80));
        shadowEffect->setOffset(0, 4);
    }
    update();
}

void FloatingWidget::takeScreenshot()
{
    qDebug() << "Taking screenshot using ScreenCapture!";

    // Remember if widget was visible before hiding
    wasVisibleBeforeScreenshot = isVisible();
    qDebug() << "Widget was visible before screenshot:" << wasVisibleBeforeScreenshot;

    // Hide widget immediately and take screenshot
    hide();

    // Wait a moment for widget to hide completely
    QTimer::singleShot(100, [this]() {
        qDebug() << "Capturing screen with cross-platform ScreenCapture...";

        // Use the new ScreenCapture class for real cross-platform capture
        ScreenCapture *capture = new ScreenCapture(this);
        QPixmap screenshot = capture->captureScreen();
        capture->deleteLater();

        qDebug() << "Screenshot capture completed, size:" << screenshot.size();

        // If still no screenshot after all methods, create demo pattern
        if (screenshot.isNull()) {
            qDebug() << "Screenshot capture failed, creating demo pattern as fallback";
            // Create a realistic demo pattern
            QScreen *screen = QApplication::primaryScreen();
            if (!screen) {
                qWarning() << "No primary screen found!";
                show();
                return;
            }
            QRect screenRect = screen->geometry();
            screenshot = QPixmap(screenRect.size());

            // Create a gradient background like a desktop
            QPainter painter(&screenshot);
            QLinearGradient gradient(0, 0, 0, screenRect.height());
            gradient.setColorAt(0, QColor(135, 206, 235)); // Sky blue
            gradient.setColorAt(1, QColor(25, 25, 112));   // Midnight blue
            painter.fillRect(screenshot.rect(), gradient);

            // Add some "windows" to make it look realistic
            painter.fillRect(100, 100, 400, 300, QColor(240, 240, 240, 230));
            painter.setPen(QPen(Qt::darkGray, 2));
            painter.drawRect(100, 100, 400, 300);

            painter.fillRect(600, 200, 300, 200, QColor(255, 255, 255, 230));
            painter.drawRect(600, 200, 300, 200);

            // Add title bars
            painter.fillRect(100, 100, 400, 30, QColor(70, 130, 180));
            painter.fillRect(600, 200, 300, 30, QColor(70, 130, 180));

            // Add instructional text
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 16, QFont::Bold));
            painter.drawText(screenRect, Qt::AlignCenter,
                            "OHAO Screenshot Tool - Demo Mode\n\n"
                            "Click and drag to select any area\n"
                            "Screenshot capture restricted by Wayland security");

            painter.setPen(Qt::yellow);
            painter.setFont(QFont("Arial", 12));
            painter.drawText(20, screenRect.height() - 40,
                            "Note: Real screenshot requires special permissions on Wayland");

            qDebug() << "Created Wayland demo screenshot:" << screenshot.size();
        }

        if (screenshot.isNull()) {
            qWarning() << "All screenshot methods failed!";
            show(); // Show widget back
            return;
        }

        qDebug() << "Screenshot captured successfully:" << screenshot.size();

        // Now create screenshot widget with the captured image
        ScreenshotWidget *screenshotWidget = new ScreenshotWidget(screenshot);

        if (screenshotWidget) {
            qDebug() << "Screenshot widget created with image, showing...";
            screenshotWidget->show();
            screenshotWidget->raise();
            screenshotWidget->activateWindow();

            // Show this widget again after screenshot closes (only if it was visible before)
            connect(screenshotWidget, &ScreenshotWidget::screenshotFinished, [this]() {
                qDebug() << "Screenshot finished signal received";
                // Only show widget if it was visible before screenshot
                if (wasVisibleBeforeScreenshot) {
                    qDebug() << "Restoring widget visibility";
                    show();
                    raise();
                    activateWindow();
                } else {
                    qDebug() << "Widget was hidden before screenshot, keeping it hidden";
                }
            });

            // Keep destroyed as backup
            connect(screenshotWidget, &QWidget::destroyed, [this]() {
                qDebug() << "Screenshot widget destroyed (backup signal)";
                // Only show widget if it was visible before screenshot
                if (wasVisibleBeforeScreenshot) {
                    qDebug() << "Restoring widget visibility";
                    show();
                    raise();
                    activateWindow();
                } else {
                    qDebug() << "Widget was hidden before screenshot, keeping it hidden";
                }
            });
        } else {
            qWarning() << "Failed to create screenshot widget!";
            show(); // Show widget back if screenshot creation failed
        }
    });
}

void FloatingWidget::openSettings()
{
    qDebug() << "Opening settings window...";

    if (!settingsWindow) {
        settingsWindow = new SettingsWindow(this);
    }

    settingsWindow->show();
    settingsWindow->raise();
    settingsWindow->activateWindow();
}

void FloatingWidget::toggleVisibility()
{
    if (isVisible()) {
        qDebug() << "Hiding FloatingWidget via global shortcut";
        hide();
    } else {
        qDebug() << "Showing FloatingWidget via global shortcut";
        show();
        raise();
        activateWindow();
    }
}

void FloatingWidget::activateWindow()
{
    qDebug() << "Activating FloatingWidget window";
    show();
    raise();
    QWidget::activateWindow();

    // Force focus and bring to front
    setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);

    // Flash the window to get user attention
    QApplication::alert(this, 1000);
}

void FloatingWidget::handleNewConnection()
{
    qDebug() << "Handling new connection from another instance";
    QLocalSocket *socket = localServer->nextPendingConnection();

    if (socket) {
        connect(socket, &QLocalSocket::readyRead, [this, socket]() {
            QByteArray data = socket->readAll();
            QString command = QString::fromUtf8(data);
            qDebug() << "Received command from another instance:" << command;

            if (command == "activate") {
                activateWindow();
            }

            socket->deleteLater();
        });

        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
}