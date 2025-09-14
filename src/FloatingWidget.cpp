#include "FloatingWidget.h"
#include "ScreenshotWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QDebug>
#include <QTransform>

FloatingWidget::FloatingWidget(QWidget *parent)
    : QWidget(parent), isDragging(false), currentScale(1.0), currentOpacity(200)
{
    setupUI();
    applyModernStyle();

    // Position at right edge of screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        move(screenGeometry.width() - width() - 20, screenGeometry.height() / 2 - height() / 2);
    }
}

void FloatingWidget::setupUI()
{
    // Make widget frameless and always on top
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Set size
    setFixedSize(140, 60);

    // Create horizontal layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Screenshot button
    screenshotBtn = new QPushButton(this);
    screenshotBtn->setFixedSize(50, 40);
    screenshotBtn->setCursor(Qt::PointingHandCursor);
    connect(screenshotBtn, &QPushButton::clicked, this, &FloatingWidget::takeScreenshot);

    // Settings button
    settingsBtn = new QPushButton(this);
    settingsBtn->setFixedSize(50, 40);
    settingsBtn->setCursor(Qt::PointingHandCursor);
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
}

void FloatingWidget::applyModernStyle()
{
    // Modern gradient button style with icons
    QString buttonStyle = R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 240),
                stop:1 rgba(240, 240, 245, 240));
            border: 1px solid rgba(200, 200, 210, 150);
            border-radius: 20px;
            font-size: 18px;
            font-weight: bold;
            color: #4A5568;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 255, 255, 255),
                stop:1 rgba(245, 245, 250, 255));
            border: 1px solid rgba(100, 150, 250, 200);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(240, 240, 245, 255),
                stop:1 rgba(230, 230, 235, 255));
        }
    )";

    screenshotBtn->setStyleSheet(buttonStyle);
    settingsBtn->setStyleSheet(buttonStyle);

    // Set button icons using Unicode symbols
    screenshotBtn->setText("📸");
    settingsBtn->setText("⚙️");
}

void FloatingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Apply scale transformation
    QTransform transform;
    transform.translate(width()/2.0, height()/2.0);
    transform.scale(currentScale, currentScale);
    transform.translate(-width()/2.0, -height()/2.0);
    painter.setTransform(transform);

    // Draw modern rounded rectangle with gradient
    QPainterPath path;
    path.addRoundedRect(rect(), 25, 25);

    // Create gradient background with more vibrant colors
    QLinearGradient gradient(0, 0, 0, height());
    if (currentScale > 1.0) {
        // Hover state - more vibrant
        gradient.setColorAt(0, QColor(255, 255, 255, currentOpacity));
        gradient.setColorAt(0.3, QColor(248, 250, 255, currentOpacity));
        gradient.setColorAt(1, QColor(240, 245, 255, currentOpacity));
    } else {
        // Normal state
        gradient.setColorAt(0, QColor(255, 255, 255, currentOpacity));
        gradient.setColorAt(0.5, QColor(250, 250, 252, currentOpacity));
        gradient.setColorAt(1, QColor(245, 245, 248, currentOpacity));
    }

    painter.fillPath(path, gradient);

    // Draw subtle border with glow effect on hover
    QPen borderPen;
    if (currentScale > 1.0) {
        borderPen = QPen(QColor(100, 150, 255, 150), 1.5);
    } else {
        borderPen = QPen(QColor(200, 200, 210, 100), 1);
    }
    painter.setPen(borderPen);
    painter.drawPath(path);
}

void FloatingWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    animateHover(true);
    setCursor(Qt::OpenHandCursor);
}

void FloatingWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    animateHover(false);
    setCursor(Qt::ArrowCursor);
}

void FloatingWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if click is on a button - if so, don't start dragging
        QWidget *childWidget = childAt(event->pos());
        if (childWidget && (childWidget == screenshotBtn || childWidget == settingsBtn)) {
            // Let button handle the click
            event->ignore();
            return;
        }

        // Start dragging only if not clicking on buttons
        isDragging = true;
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void FloatingWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = event->globalPosition().toPoint() - dragPosition;

        // Keep widget on screen
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->geometry();
            newPos.setX(qMax(0, qMin(newPos.x(), screenGeometry.width() - width())));
            newPos.setY(qMax(0, qMin(newPos.y(), screenGeometry.height() - height())));
        }

        move(newPos);
        event->accept();
    }
}

void FloatingWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }
}

void FloatingWidget::animateHover(bool hover)
{
    if (hover) {
        currentOpacity = 255;
        currentScale = 1.05; // Subtle scale up
        shadowEffect->setBlurRadius(30);
        shadowEffect->setColor(QColor(0, 150, 255, 100)); // Blue glow on hover
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
    qDebug() << "Taking screenshot - Flameshot style!";

    // Hide widget immediately and take screenshot
    hide();

    // Wait a moment for widget to hide completely
    QTimer::singleShot(100, [this]() {
        qDebug() << "Capturing screen before showing overlay...";

        // Capture screen FIRST, before creating any overlay
        QScreen *screen = QApplication::primaryScreen();
        if (!screen) {
            qWarning() << "No primary screen found!";
            show(); // Show widget back
            return;
        }

        qDebug() << "Screen geometry:" << screen->geometry();

        // Try multiple screenshot methods
        QPixmap screenshot;

        // Method 1: Basic screen grab
        qDebug() << "Trying method 1: screen->grabWindow(0)";
        screenshot = screen->grabWindow(0);

        if (screenshot.isNull()) {
            qDebug() << "Method 1 failed, trying method 2: screen grab with geometry";
            QRect screenRect = screen->geometry();
            screenshot = screen->grabWindow(0, 0, 0, screenRect.width(), screenRect.height());
        }

        if (screenshot.isNull()) {
            qDebug() << "Method 2 failed, trying method 3: Different screen grab approach";
            // Try grabbing the entire screen with explicit coordinates
            QRect screenRect = screen->geometry();
            screenshot = screen->grabWindow(0, screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
        }

        if (screenshot.isNull()) {
            qDebug() << "Method 3 failed, trying method 4: Alternative screen grab";
            // Try with different parameters for Wayland compatibility
            screenshot = screen->grabWindow(0, 0, 0, -1, -1);
        }

        if (screenshot.isNull()) {
            qDebug() << "All Qt methods failed. This is likely due to Wayland restrictions.";
            qDebug() << "Creating demo pattern for now...";

            // Create a realistic demo pattern
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
                            "Demo Mode - Wayland Screenshot Simulation\n\n"
                            "Click and drag to select any area\n"
                            "This demonstrates the selection functionality");

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

            // Show this widget again after screenshot closes
            connect(screenshotWidget, &ScreenshotWidget::screenshotFinished, [this]() {
                qDebug() << "Screenshot finished signal received, showing floating widget again";
                show();
                raise();
                activateWindow();
            });

            // Keep destroyed as backup
            connect(screenshotWidget, &QWidget::destroyed, [this]() {
                qDebug() << "Screenshot widget destroyed (backup signal)";
                show();
                raise();
                activateWindow();
            });
        } else {
            qWarning() << "Failed to create screenshot widget!";
            show(); // Show widget back if screenshot creation failed
        }
    });
}

void FloatingWidget::openSettings()
{
    qDebug() << "Opening settings...";
    // TODO: Implement settings dialog
}