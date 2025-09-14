#include "ScreenshotWidget.h"
#include "SelectionToolbar.h"
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
#include <iostream>

ScreenshotWidget::ScreenshotWidget(QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false), showingToolbar(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Capture screen immediately (for backward compatibility)
    captureScreen();

    setupWidget();
}

ScreenshotWidget::ScreenshotWidget(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false), showingToolbar(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Use the provided screenshot
    this->screenshot = screenshot;
    qDebug() << "Screenshot widget initialized with image:" << screenshot.size();

    setupWidget();
}

void ScreenshotWidget::setupWidget()
{
    // Make fullscreen
    showFullScreen();

    // Enable mouse tracking
    setMouseTracking(true);

    // Set crosshair cursor like Flameshot
    setCursor(Qt::CrossCursor);

    // Take focus for keyboard events
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    if (!screenshot.isNull()) {
        resize(screenshot.size());
    }
}

ScreenshotWidget::~ScreenshotWidget()
{
    // No separate toolbar to clean up anymore
}

void ScreenshotWidget::captureScreen()
{
    qDebug() << "Capturing screen for backward compatibility...";

    QScreen *screen = QApplication::primaryScreen();
    if (!screen) {
        qWarning() << "No primary screen found!";
        return;
    }

    qDebug() << "Screen geometry:" << screen->geometry();
    screenshot = screen->grabWindow(0);

    if (screenshot.isNull()) {
        qWarning() << "Screenshot capture failed! Trying alternative method...";

        // Try alternative screen capture using screen directly
        QRect screenRect = screen->geometry();
        QPixmap desktopPixmap = screen->grabWindow(0, 0, 0, screenRect.width(), screenRect.height());

        if (!desktopPixmap.isNull()) {
            screenshot = desktopPixmap;
            qDebug() << "Alternative screenshot method worked:" << screenshot.size();
        }
    } else {
        qDebug() << "Screenshot captured successfully:" << screenshot.size();
    }

    if (!screenshot.isNull()) {
        resize(screenshot.size());
    } else {
        qCritical() << "All screenshot methods failed!";
        close();
    }
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
        painter.drawPixmap(selectionRect, screenshot, selectionRect);

        // Draw modern selection border with blue accent
        QPen borderPen(QColor(0, 150, 255), 3);
        painter.setPen(borderPen);
        painter.drawRect(selectionRect);

        // Draw corner handles for visual feedback
        int handleSize = 8;
        QBrush handleBrush(QColor(0, 150, 255));
        painter.setBrush(handleBrush);
        painter.setPen(Qt::NoPen);

        // Draw corner handles
        painter.drawEllipse(selectionRect.topLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.topRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.bottomLeft() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);
        painter.drawEllipse(selectionRect.bottomRight() - QPoint(handleSize/2, handleSize/2), handleSize, handleSize);

        // Show dimensions with modern styling
        if (selectionRect.width() > 30 && selectionRect.height() > 20) {
            QString dimensions = QString("%1 × %2").arg(selectionRect.width()).arg(selectionRect.height());
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
            std::cout << "*** Valid selection, showing integrated toolbar" << std::endl;
            showingToolbar = true;
            update(); // Redraw to show toolbar
        } else {
            std::cout << "*** Selection too small, closing" << std::endl;
            close();
        }
    }
}

void ScreenshotWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit screenshotFinished();
        close();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (hasSelection) {
            handleCopy();
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_C && hasSelection) {
            handleCopy();
        } else if (event->key() == Qt::Key_S && hasSelection) {
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
    QPixmap selectedArea = screenshot.copy(selection);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(selectedArea);

    qDebug() << "Copied selection to clipboard";
    emit screenshotFinished();
    close();
}

void ScreenshotWidget::handleSave()
{
    QRect selection = QRect(startPoint, endPoint).normalized();
    QPixmap selectedArea = screenshot.copy(selection);

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
    QPixmap selectedArea = screenshot.copy(selection);

    // TODO: Implement OCR functionality
    qDebug() << "OCR requested for selection";

    QMessageBox::information(this, "OCR", "OCR functionality coming soon!");
    emit screenshotFinished();
    close();
}

void ScreenshotWidget::handleCancel()
{
    emit screenshotFinished();
    close();
}