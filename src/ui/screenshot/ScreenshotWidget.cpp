#include "ScreenshotWidget.h"
#include "../overlays/OverlayManager.h"
#include "TTSManager.h"
#include "TTSEngine.h"
#include "../core/ThemeManager.h"
#include "../core/ThemeColors.h"
#include "../core/LanguageManager.h"
#include <QApplication>
#include <QScreen>
#include <QBrush>
#include <QPen>
#include <QDebug>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QPainterPath>
#include <QTimer>
#include <QStyleHints>
#include <QGuiApplication>
#include <iostream>

ScreenshotWidget::ScreenshotWidget(QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false)
    , showingResults(false), m_isFirstSelection(true)
{
    // Make fullscreen and frameless
    // On macOS, add BypassWindowManagerHint to cover the menu bar completely
    #ifdef Q_OS_MACOS
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::BypassWindowManagerHint);
    #else
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    #endif
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

    // Load dimming opacity from settings (default: 120 out of 255 = ~47%)
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    m_dimmingOpacity = settings.value("screenshot/dimmingOpacity", 120).toInt();

    // Initialize overlay manager
    m_overlayManager = new OverlayManager(this);
    // Note: ESC is now handled by ScreenshotWidget::keyPressEvent to exit screenshot mode
    // Overlay ESC events will be ignored to keep screenshot mode active

    // Capture screen immediately (for backward compatibility)
    captureScreen();

    setupWidget();

    // Connect to theme changes for runtime updates
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        update(); // Redraw the widget with new theme colors
    });
}

ScreenshotWidget::ScreenshotWidget(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false)
    , showingResults(false), m_isFirstSelection(true)
{
    // Make fullscreen and frameless
    // On macOS, add BypassWindowManagerHint to cover the menu bar completely
    #ifdef Q_OS_MACOS
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::BypassWindowManagerHint);
    #else
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    #endif
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

    // Load dimming opacity from settings (default: 120 out of 255 = ~47%)
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    m_dimmingOpacity = settings.value("screenshot/dimmingOpacity", 120).toInt();

    // Initialize overlay manager
    m_overlayManager = new OverlayManager(this);
    // Note: ESC is now handled by ScreenshotWidget::keyPressEvent to exit screenshot mode
    // Overlay ESC events will be ignored to keep screenshot mode active

    // Use the provided screenshot
    this->screenshot = screenshot;
    qDebug() << "Screenshot widget initialized with image:" << screenshot.size();

    setupWidget();

    // Connect to theme changes for runtime updates
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        update(); // Redraw the widget with new theme colors
    });
}

void ScreenshotWidget::setupWidget()
{
    // Cover the entire screen without creating a new fullscreen space
    // Use showMaximized() or manual geometry setting instead of showFullScreen()
    // to avoid macOS creating a new desktop/space
    
#ifdef Q_OS_MACOS
    // On macOS, manually set geometry to cover all screens and show normal
    // This prevents the system from creating a new fullscreen space
    QScreen *primaryScreen = QApplication::primaryScreen();
    if (primaryScreen) {
        QRect screenGeometry = primaryScreen->geometry();
        
        // If multiple screens, expand to cover all of them
        const auto screens = QApplication::screens();
        for (QScreen *screen : screens) {
            screenGeometry = screenGeometry.united(screen->geometry());
        }
        
        setGeometry(screenGeometry);
        show();
        raise();
        activateWindow();
    } else {
        show();
    }
#else
    // On other platforms, showFullScreen() works fine
    showFullScreen();
#endif

    // Enable mouse tracking
    setMouseTracking(true);

    // Set crosshair cursor like Flameshot
    setCursor(Qt::CrossCursor);

    // Take focus for keyboard events
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

ScreenshotWidget::~ScreenshotWidget()
{
    // No separate toolbar to clean up anymore
}

void ScreenshotWidget::captureScreen()
{
    qDebug() << "Capturing screen for backward compatibility...";

    // Use central ScreenCapture so Wayland/X11/Win/Mac paths are consistent
    ScreenCapture capture;
    QPixmap captured = capture.captureScreen();

    if (captured.isNull()) {
        qCritical() << "All screenshot methods failed!";
        close();
        return;
    }

    // Keep DPR tagging for correct on-screen mapping during selection
    screenshot = captured;
    qDebug() << "Screenshot captured successfully:" << screenshot.size() << " DPR:" << screenshot.devicePixelRatio();

    // Match widget size to logical size (QPixmap::size is already logical when DPR>1)
    resize(screenshot.size());
}

void ScreenshotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (screenshot.isNull()) {
        qWarning() << "Screenshot is null!";
        return;
    }

    // Draw the screenshot
    painter.drawPixmap(0, 0, screenshot);

    // Draw darkened overlay
    QRect currentSelection = QRect(startPoint, endPoint).normalized();
    bool hasCurrentSelection = (hasSelection || selecting) && currentSelection.width() > 0 && currentSelection.height() > 0;
    
    // Draw dark overlay everywhere except selections (use configurable opacity)
    painter.fillRect(rect(), QColor(0, 0, 0, m_dimmingOpacity));

    QPalette themePalette = ThemeManager::instance().getCurrentPalette();
    QColor accentColor = themePalette.color(QPalette::Highlight);
    
    // Draw all previous OCR selections (keep them visible with frames)
    for (const QRect &ocrRect : m_ocrSelections) {
        // Show the original screenshot in this area
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.fillRect(ocrRect, Qt::transparent);
        qreal dpr = screenshot.devicePixelRatio();
        QRect srcRect(
            QPoint(static_cast<int>(ocrRect.x() * dpr), static_cast<int>(ocrRect.y() * dpr)),
            QSize(static_cast<int>(ocrRect.width() * dpr), static_cast<int>(ocrRect.height() * dpr))
        );
        painter.drawPixmap(ocrRect, screenshot, srcRect);
        
        // Draw frame for OCR'd area (slightly dimmed to differentiate from current selection)
        QPen ocrPen(accentColor.lighter(130), 2, Qt::DashLine);
        painter.setPen(ocrPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(ocrRect);
    }
    
    // If we have a current selection, draw it with full highlight
    if (hasCurrentSelection) {
        // Clear the selection area to show original screenshot
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.fillRect(currentSelection, Qt::transparent);
        // Map logical selection to pixmap device pixels for the source rect
        {
            qreal dpr = screenshot.devicePixelRatio();
            QRect srcRect(
                QPoint(static_cast<int>(currentSelection.x() * dpr), static_cast<int>(currentSelection.y() * dpr)),
                QSize(static_cast<int>(currentSelection.width() * dpr), static_cast<int>(currentSelection.height() * dpr))
            );
            painter.drawPixmap(currentSelection, screenshot, srcRect);
        }

        // Draw modern selection border with theme accent color
        QPen borderPen(accentColor, 3);
        painter.setPen(borderPen);
        painter.drawRect(currentSelection);

        // Draw corner handles for visual feedback
        int handleSize = 8;
        QBrush handleBrush(accentColor);
        painter.setBrush(handleBrush);
        painter.setPen(Qt::NoPen);

        // Draw corner handles
        painter.drawEllipse(currentSelection.topLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(currentSelection.topRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(currentSelection.bottomLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(currentSelection.bottomRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);

        // Show dimensions with modern styling - but only during selection, not when showing results
        if (currentSelection.width() > 30 && currentSelection.height() > 20 && !showingResults) {
            qreal dpr = screenshot.devicePixelRatio();
            int physW = static_cast<int>(currentSelection.width() * dpr);
            int physH = static_cast<int>(currentSelection.height() * dpr);
            QString dimensions = QString("%1 × %2 px").arg(physW).arg(physH);
            QFont font = painter.font();
            font.setPointSize(11);
            font.setWeight(QFont::Bold);
            painter.setFont(font);

            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(dimensions);

            // Position above selection if space, otherwise below
            QPoint textPos;
            if (currentSelection.top() > textRect.height() + 10) {
                textPos = QPoint(currentSelection.left() + (currentSelection.width() - textRect.width()) / 2,
                               currentSelection.top() - 8);
            } else {
                textPos = QPoint(currentSelection.left() + (currentSelection.width() - textRect.width()) / 2,
                               currentSelection.bottom() + textRect.height() + 8);
            }

            textRect.moveTopLeft(textPos);

            // Draw background with rounded corners
            QPainterPath bgPath;
            bgPath.addRoundedRect(textRect.adjusted(-8, -4, 8, 4), 6, 6);
            painter.fillPath(bgPath, QColor(0, 0, 0, 180));

            // Draw text
            painter.setPen(Qt::white);
            painter.drawText(textRect, Qt::AlignCenter, dimensions);
        }
    } else {
        // Draw full overlay when no selection (use configurable opacity)
        painter.fillRect(rect(), QColor(0, 0, 0, m_dimmingOpacity));

        // Show instruction text
        QString instruction = "Click and drag to select area • Press ESC to cancel";
        QFont font = painter.font();
        font.setPointSize(14);
        painter.setFont(font);

        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(instruction);
        textRect.moveCenter(rect().center());
        textRect.moveTop(50); // Position near top

        // Draw background
        QPainterPath bgPath;
        bgPath.addRoundedRect(textRect.adjusted(-15, -8, 15, 8), 8, 8);
        painter.fillPath(bgPath, QColor(0, 0, 0, 200));

        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, instruction);
    }

    // Results overlays are now handled by OverlayManager - no drawing needed here!
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Start new selection
        startPoint = event->pos();
        endPoint = startPoint;
        selecting = true;
        hasSelection = false;

        update();
    }
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (selecting) {
        endPoint = event->pos();
        update();

        // Change cursor to indicate active selection
        setCursor(Qt::CrossCursor);
    } else {
        // Show crosshair when hovering
        setCursor(Qt::CrossCursor);
    }
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && selecting) {
        selecting = false;
        hasSelection = true;

        QRect selection = QRect(startPoint, endPoint).normalized();
        std::cout << "*** Mouse released, selection: " << selection.x() << "," << selection.y()
                  << " " << selection.width() << "x" << selection.height() << std::endl;

        if (selection.width() > 10 && selection.height() > 10) {
            // Always start OCR automatically - no manual mode needed
            qDebug() << "Starting OCR automatically on selection";
            handleOCR();
        } else {
            std::cout << "*** Selection too small, ignoring and staying active" << std::endl;
            // Reset selection state and keep the overlay active
            hasSelection = false;
            selecting = false;
            setCursor(Qt::CrossCursor);
            update();
        }
    }
}

void ScreenshotWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // ESC always exits screenshot mode
        qDebug() << "ESC pressed - exiting screenshot mode";

        // Close any visible overlays first
        if (m_overlayManager) {
            m_overlayManager->hideAllOverlays();
        }

        // Exit screenshot mode
        emit screenshotFinished();
        close();
    }
}

void ScreenshotWidget::handleCopy()
{
    QRect selection = QRect(startPoint, endPoint).normalized();
    QPixmap selectedArea = screenshot.copy(QRect(
        QPoint(static_cast<int>(selection.x() * screenshot.devicePixelRatio()),
               static_cast<int>(selection.y() * screenshot.devicePixelRatio())),
        QSize(static_cast<int>(selection.width() * screenshot.devicePixelRatio()),
              static_cast<int>(selection.height() * screenshot.devicePixelRatio()))
    ));

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(selectedArea);

    qDebug() << "Copied selection to clipboard";
    emit screenshotFinished();
    close();
}

void ScreenshotWidget::handleSave()
{
    QRect selection = QRect(startPoint, endPoint).normalized();
    QPixmap selectedArea = screenshot.copy(QRect(
        QPoint(static_cast<int>(selection.x() * screenshot.devicePixelRatio()),
               static_cast<int>(selection.y() * screenshot.devicePixelRatio())),
        QSize(static_cast<int>(selection.width() * screenshot.devicePixelRatio()),
              static_cast<int>(selection.height() * screenshot.devicePixelRatio()))
    ));

    QString fileName = QFileDialog::getSaveFileName(this, "Save Screenshot",
        QDir::homePath() + "/screenshot.png", "Images (*.png *.jpg)");

    if (!fileName.isEmpty()) {
        selectedArea.save(fileName);
        qDebug() << "Saved screenshot to" << fileName;
    }

    emit screenshotFinished();
    close();
}

void ScreenshotWidget::handleOCR()
{
    QRect selection = QRect(startPoint, endPoint).normalized();
    
    #ifdef Q_OS_MACOS
    // Workaround for macOS menu bar offset on first selection
    // Process first selection but automatically retry with same coordinates
    if (m_isFirstSelection) {
        m_isFirstSelection = false;
        m_firstSelectionRect = selection;
        
        // Reset selection state
        hasSelection = false;
        selecting = false;
        startPoint = QPoint();
        endPoint = QPoint();
        
        update();
        
        // Automatically trigger second OCR with same coordinates after 200ms delay
        QTimer::singleShot(200, this, [this]() {
            // Restore selection coordinates
            startPoint = m_firstSelectionRect.topLeft();
            endPoint = m_firstSelectionRect.bottomRight();
            hasSelection = true;
            
            // Call handleOCR again - this time m_isFirstSelection is false
            handleOCR();
        });
        
        return;
    }
    #endif
    
    // Calculate physical coordinates
    QRect physicalRect(
        static_cast<int>(selection.x() * screenshot.devicePixelRatio()),
        static_cast<int>(selection.y() * screenshot.devicePixelRatio()),
        static_cast<int>(selection.width() * screenshot.devicePixelRatio()),
        static_cast<int>(selection.height() * screenshot.devicePixelRatio())
    );
    
    QPixmap selectedArea = screenshot.copy(physicalRect);

    if (selectedArea.isNull() || selectedArea.size().isEmpty()) {
        QMessageBox::warning(this, "OCR Error", "Please select a valid area for OCR processing.");
        return;
    }

    // Update UI state for overlay display
    showingResults = true;
    m_currentResult = OCRResult(); // Reset result

    // Store the selected area for potential retry
    m_lastOCRImage = selectedArea;

    // Store this selection in the list of OCR'd areas to keep it visible
    m_ocrSelections.append(selection);
    
    // Delegate all OCR processing to OverlayManager, pass full screenshot and existing selections
    m_overlayManager->performOCR(selectedArea, selection, screenshot, m_ocrSelections);
    
    // Reset selection state so user can make a new selection
    hasSelection = false;
    selecting = false;
    startPoint = QPoint();
    endPoint = QPoint();
    
    update();
}

void ScreenshotWidget::handleCancel()
{
    emit screenshotFinished();
    close();
}

