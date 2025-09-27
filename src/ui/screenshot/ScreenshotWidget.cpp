#include "ScreenshotWidget.h"
#include "../overlays/OverlayManager.h"
#include "TTSManager.h"
#include "TTSEngine.h"
#include "../core/ThemeManager.h"
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
    : QWidget(parent), selecting(false), hasSelection(false), showingToolbar(false)
    , showingResults(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

    // Initialize overlay manager
    m_overlayManager = new OverlayManager(this);
    connect(m_overlayManager, &OverlayManager::overlayEscapePressed,
            this, [this]() {
                emit screenshotFinished();
                close();
            });

    // Capture screen immediately (for backward compatibility)
    captureScreen();

    setupWidget();
}

ScreenshotWidget::ScreenshotWidget(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false), showingToolbar(false)
    , showingResults(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

    // Initialize overlay manager
    m_overlayManager = new OverlayManager(this);
    connect(m_overlayManager, &OverlayManager::overlayEscapePressed,
            this, [this]() {
                emit screenshotFinished();
                close();
            });

    // Use the provided screenshot
    this->screenshot = screenshot;
    qDebug() << "Screenshot widget initialized with image:" << screenshot.size();

    setupWidget();
}

void ScreenshotWidget::setupWidget()
{
    // Always full screen so selection can cover the entire display area
    showFullScreen();

    // Enable mouse tracking
    setMouseTracking(true);

    // Set crosshair cursor like Flameshot
    setCursor(Qt::CrossCursor);

    // Take focus for keyboard events
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    // Fullscreen handles sizing; nothing else to do
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

    // If we have a selection, use more sophisticated overlay
    if (hasSelection || selecting) {
        QRect selectionRect = QRect(startPoint, endPoint).normalized();

        // Draw darkened overlay everywhere except selection
        painter.fillRect(rect(), QColor(0, 0, 0, 120));

        // Clear the selection area to show original screenshot
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.fillRect(selectionRect, Qt::transparent);
        // Map logical selection to pixmap device pixels for the source rect
        {
            qreal dpr = screenshot.devicePixelRatio();
            QRect srcRect(
                QPoint(static_cast<int>(selectionRect.x() * dpr), static_cast<int>(selectionRect.y() * dpr)),
                QSize(static_cast<int>(selectionRect.width() * dpr), static_cast<int>(selectionRect.height() * dpr))
            );
            painter.drawPixmap(selectionRect, screenshot, srcRect);
        }

        // Draw modern selection border with theme accent color
        QPalette themePalette = ThemeManager::instance().getCurrentPalette();
        QColor accentColor = themePalette.color(QPalette::Highlight);
        QPen borderPen(accentColor, 3);
        painter.setPen(borderPen);
        painter.drawRect(selectionRect);

        // Draw corner handles for visual feedback
        int handleSize = 8;
        QBrush handleBrush(accentColor);
        painter.setBrush(handleBrush);
        painter.setPen(Qt::NoPen);

        // Draw corner handles
        painter.drawEllipse(selectionRect.topLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.topRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.bottomLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.bottomRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);

        // Show dimensions with modern styling - but only during selection, not when showing results
        if (selectionRect.width() > 30 && selectionRect.height() > 20 && !showingResults) {
            qreal dpr = screenshot.devicePixelRatio();
            int physW = static_cast<int>(selectionRect.width() * dpr);
            int physH = static_cast<int>(selectionRect.height() * dpr);
            QString dimensions = QString("%1 × %2 px").arg(physW).arg(physH);
            QFont font = painter.font();
            font.setPointSize(11);
            font.setWeight(QFont::Bold);
            painter.setFont(font);

            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(dimensions);

            // Position above selection if space, otherwise below
            QPoint textPos;
            if (selectionRect.top() > textRect.height() + 10) {
                textPos = QPoint(selectionRect.left() + (selectionRect.width() - textRect.width()) / 2,
                               selectionRect.top() - 8);
            } else {
                textPos = QPoint(selectionRect.left() + (selectionRect.width() - textRect.width()) / 2,
                               selectionRect.bottom() + textRect.height() + 8);
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
        // Draw full overlay when no selection
        painter.fillRect(rect(), QColor(0, 0, 0, 120));

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

    // Draw integrated toolbar if showing
    if (showingToolbar) {
        drawToolbar(painter);
    }

    // Results overlays are now handled by OverlayManager - no drawing needed here!
}

void ScreenshotWidget::drawToolbar(QPainter &painter)
{
    QRect selection = QRect(startPoint, endPoint).normalized();

    // Calculate toolbar position (same logic as before)
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->geometry();
    const int margin = 8;
    const int buttonSize = 38;
    const int spacing = 4;
    const int padding = 8;

    // Calculate toolbar size
    int toolbarWidth = (buttonSize * 4) + (spacing * 3) + (padding * 2);
    int toolbarHeight = buttonSize + (padding * 2);

    // Position toolbar below selection (or above if no space)
    int x = selection.x() + (selection.width() - toolbarWidth) / 2;
    int y = selection.bottom() + margin;

    if (y + toolbarHeight > screenGeometry.height()) {
        y = selection.top() - toolbarHeight - margin;
    }

    // Constrain to screen
    x = qMax(0, qMin(x, screenGeometry.width() - toolbarWidth));
    y = qMax(0, qMin(y, screenGeometry.height() - toolbarHeight));

    toolbarRect = QRect(x, y, toolbarWidth, toolbarHeight);

    std::cout << "*** Drawing integrated toolbar at: " << x << "," << y << std::endl;

    // Draw toolbar background
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath toolbarPath;
    toolbarPath.addRoundedRect(toolbarRect, 24, 24);

    QBrush toolbarBrush(QColor(30, 30, 35, 240));
    painter.fillPath(toolbarPath, toolbarBrush);

    // Draw toolbar border
    painter.setPen(QPen(QColor(100, 100, 120, 150), 1));
    painter.drawPath(toolbarPath);

    // Draw buttons
    int buttonX = x + padding;
    int buttonY = y + padding;

    // Copy button 📋
    copyButtonRect = QRect(buttonX, buttonY, buttonSize, buttonSize);
    painter.fillRect(copyButtonRect, QColor(70, 70, 80, 200));
    painter.setPen(QPen(QColor(100, 100, 120, 150), 1));
    painter.drawRoundedRect(copyButtonRect, 18, 18);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.drawText(copyButtonRect, Qt::AlignCenter, "📋");

    buttonX += buttonSize + spacing;

    // Save button 💾
    saveButtonRect = QRect(buttonX, buttonY, buttonSize, buttonSize);
    painter.fillRect(saveButtonRect, QColor(70, 70, 80, 200));
    painter.setPen(QPen(QColor(100, 100, 120, 150), 1));
    painter.drawRoundedRect(saveButtonRect, 18, 18);
    painter.setPen(Qt::white);
    painter.drawText(saveButtonRect, Qt::AlignCenter, "💾");

    buttonX += buttonSize + spacing;

    // OCR button 📝
    ocrButtonRect = QRect(buttonX, buttonY, buttonSize, buttonSize);
    painter.fillRect(ocrButtonRect, QColor(70, 70, 80, 200));
    painter.setPen(QPen(QColor(100, 100, 120, 150), 1));
    painter.drawRoundedRect(ocrButtonRect, 18, 18);
    painter.setPen(Qt::white);
    painter.drawText(ocrButtonRect, Qt::AlignCenter, "📝");

    buttonX += buttonSize + spacing;

    // Cancel button ❌
    cancelButtonRect = QRect(buttonX, buttonY, buttonSize, buttonSize);
    painter.fillRect(cancelButtonRect, QColor(70, 70, 80, 200));
    painter.setPen(QPen(QColor(100, 100, 120, 150), 1));
    painter.drawRoundedRect(cancelButtonRect, 18, 18);
    painter.setPen(Qt::white);
    painter.drawText(cancelButtonRect, Qt::AlignCenter, "❌");
}

// Legacy drawResultsOverlay removed - now handled by OverlayManager

ToolbarButton ScreenshotWidget::getButtonAt(QPoint pos)
{
    if (copyButtonRect.contains(pos)) return ToolbarButton::Copy;
    if (saveButtonRect.contains(pos)) return ToolbarButton::Save;
    if (ocrButtonRect.contains(pos)) return ToolbarButton::OCR;
    if (cancelButtonRect.contains(pos)) return ToolbarButton::Cancel;
    return ToolbarButton::None;
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if clicking on toolbar buttons first
        if (showingToolbar) {
            ToolbarButton button = getButtonAt(event->pos());
            if (button != ToolbarButton::None) {
                handleToolbarClick(button);
                return;
            }
        }

        // Start new selection
        startPoint = event->pos();
        endPoint = startPoint;
        selecting = true;
        hasSelection = false;
        showingToolbar = false; // Hide toolbar when starting new selection

        std::cout << "*** Starting new selection, hiding toolbar" << std::endl;

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
            showingToolbar = false;
            setCursor(Qt::CrossCursor);
            update();
        }
    }
}

void ScreenshotWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // If overlays are visible, they will handle escape themselves and signal us to close
        if (m_overlayManager && m_overlayManager->areOverlaysVisible()) {
            // Let overlays handle the escape key - they will signal us to close
            return;
        }

        // No overlays visible, close screenshot widget directly
        emit screenshotFinished();
        close();
    } else if (showingResults && (event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_C) {
        // Copy OCR results when showing overlay
        if (m_currentResult.success && !m_currentResult.text.isEmpty()) {
            QClipboard *clipboard = QApplication::clipboard();
            QString textToCopy = m_currentResult.text;

            // If we have translation, copy both
            if (m_currentResult.hasTranslation && !m_currentResult.translatedText.isEmpty()) {
                textToCopy = QString("Original: %1\n\nTranslation: %2")
                                .arg(m_currentResult.text)
                                .arg(m_currentResult.translatedText);
            }

            clipboard->setText(textToCopy);

            // Update progress to show copy confirmation
            m_progressText = "✅ Text copied to clipboard!";
            update();

            // Clear confirmation after 2 seconds
            QTimer::singleShot(2000, [this]() {
                if (showingResults) {
                    m_progressText.clear();
                    update();
                }
            });
        }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (hasSelection && !showingResults) {
            handleCopy();
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_C && hasSelection && !showingResults) {
            handleCopy();
        } else if (event->key() == Qt::Key_S && hasSelection && !showingResults) {
            handleSave();
        }
    }
}

void ScreenshotWidget::handleToolbarClick(ToolbarButton button)
{
    std::cout << "*** Toolbar button clicked: ";
    switch (button) {
        case ToolbarButton::Copy:
            std::cout << "Copy" << std::endl;
            handleCopy();
            break;
        case ToolbarButton::Save:
            std::cout << "Save" << std::endl;
            handleSave();
            break;
        case ToolbarButton::OCR:
            std::cout << "OCR" << std::endl;
            handleOCR();
            break;
        case ToolbarButton::Cancel:
            std::cout << "Cancel" << std::endl;
            handleCancel();
            break;
        default:
            std::cout << "None" << std::endl;
            break;
    }
}

// Old showToolbar method removed - now using integrated toolbar

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
    QPixmap selectedArea = screenshot.copy(QRect(
        QPoint(static_cast<int>(selection.x() * screenshot.devicePixelRatio()),
               static_cast<int>(selection.y() * screenshot.devicePixelRatio())),
        QSize(static_cast<int>(selection.width() * screenshot.devicePixelRatio()),
              static_cast<int>(selection.height() * screenshot.devicePixelRatio()))
    ));

    qDebug() << "OCR requested for selection:" << selection;

    if (selectedArea.isNull() || selectedArea.size().isEmpty()) {
        QMessageBox::warning(this, "OCR Error", "Please select a valid area for OCR processing.");
        return;
    }

    // Update UI state for overlay display
    showingToolbar = false;
    showingResults = true;
    m_currentResult = OCRResult(); // Reset result

    // Store the selected area for potential retry
    m_lastOCRImage = selectedArea;

    // Delegate all OCR processing to OverlayManager, pass full screenshot for overlay sizing
    m_overlayManager->performOCR(selectedArea, selection, screenshot);
    update(); // Redraw
}

void ScreenshotWidget::handleCancel()
{
    emit screenshotFinished();
    close();
}

