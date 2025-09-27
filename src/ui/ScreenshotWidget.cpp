#include "ScreenshotWidget.h"
#include "SelectionToolbar.h"
#include "TTSManager.h"
#include "TTSEngine.h"
#include "ThemeManager.h"
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
    , ocrEngine(nullptr), showingResults(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

    // Capture screen immediately (for backward compatibility)
    captureScreen();

    setupWidget();
}

ScreenshotWidget::ScreenshotWidget(const QPixmap &screenshot, QWidget *parent)
    : QWidget(parent), selecting(false), hasSelection(false), showingToolbar(false)
    , ocrEngine(nullptr), showingResults(false)
{
    // Make fullscreen and frameless
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Ensure this widget uses the application's theme palette
    setPalette(QApplication::palette());
    qDebug() << "ScreenshotWidget palette set. Window color:" << palette().color(QPalette::Window).name();

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

    // Draw results overlay if showing and in Quick Translation mode
    if (showingResults) {
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        QString overlayMode = settings.value("translation/overlayMode", "Deep Learning Mode").toString();

        if (overlayMode == "Quick Translation") {
            drawResultsOverlay(painter);
        }
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

void ScreenshotWidget::drawResultsOverlay(QPainter &painter)
{
    QRect selection = QRect(startPoint, endPoint).normalized();

    // Calculate overlay position
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->geometry();
    const int margin = 20;
    const int padding = 20;
    const int minWidth = 300;
    const int maxWidth = 500;

    // CRITICAL FIX: Get theme colors directly from the singleton
    // This ensures we always get the current theme's colors
    ThemeManager& themeManager = ThemeManager::instance();
    QPalette appPalette = themeManager.getCurrentPalette();

    qDebug() << "=== OVERLAY COLOR DEBUG ===";
    qDebug() << "Current theme:" << ThemeManager::toString(themeManager.getCurrentTheme());
    qDebug() << "Palette Window:" << appPalette.color(QPalette::Window).name()
             << "RGB(" << appPalette.color(QPalette::Window).red()
             << "," << appPalette.color(QPalette::Window).green()
             << "," << appPalette.color(QPalette::Window).blue() << ")";
    qDebug() << "Palette WindowText:" << appPalette.color(QPalette::WindowText).name();
    qDebug() << "Palette Highlight:" << appPalette.color(QPalette::Highlight).name();

    // Set up content font
    QFont textFont = painter.font();
    textFont.setPointSize(12);
    QFontMetrics textFM(textFont);

    // Progress or results content
    QString content;
    bool hasTranslation = false;

    if (m_currentResult.success) {
        content = QString("Original: %1").arg(m_currentResult.text);
        if (m_currentResult.hasTranslation && !m_currentResult.translatedText.isEmpty()) {
            content += QString("\n\nTranslation: %1").arg(m_currentResult.translatedText);
            hasTranslation = true;
        }
    } else if (!m_progressText.isEmpty()) {
        content = m_progressText;
    } else {
        content = "Processing...";
    }

    // Calculate content area (no title)
    int contentWidth = qBound(minWidth, textFM.horizontalAdvance(content), maxWidth);
    QRect textRect(0, 0, contentWidth - padding * 2, 0);
    QRect boundingRect = textFM.boundingRect(textRect, Qt::TextWordWrap, content);

    int overlayWidth = contentWidth;
    int overlayHeight = boundingRect.height() + padding * 2;

    // Position overlay (prefer right side of selection, fall back to left/below)
    int x = selection.right() + margin;
    int y = selection.top();

    if (x + overlayWidth > screenGeometry.width()) {
        x = selection.left() - overlayWidth - margin;
        if (x < 0) {
            x = selection.left();
            y = selection.bottom() + margin;
            if (y + overlayHeight > screenGeometry.height()) {
                y = selection.top() - overlayHeight - margin;
            }
        }
    }

    // Ensure overlay stays on screen
    x = qBound(10, x, screenGeometry.width() - overlayWidth - 10);
    y = qBound(10, y, screenGeometry.height() - overlayHeight - 10);

    QRect overlayRect(x, y, overlayWidth, overlayHeight);
    m_resultAreaRect = overlayRect;

    // Draw background with theme-aware colors
    QPainterPath backgroundPath;
    backgroundPath.addRoundedRect(overlayRect, 12, 12);

    QColor backgroundColor, borderColor, textColor, progressColor;

    // Use actual theme colors from application palette
    backgroundColor = appPalette.color(QPalette::Window);
    backgroundColor.setAlpha(240);  // Make it semi-transparent for overlay effect

    borderColor = appPalette.color(QPalette::Highlight);
    borderColor.setAlpha(180);  // Semi-transparent border

    textColor = appPalette.color(QPalette::WindowText);

    // Progress color: use a vibrant variant of the highlight color
    progressColor = appPalette.color(QPalette::Highlight);
    // Make it more vibrant by adjusting saturation
    progressColor = progressColor.lighter(120);

    qDebug() << "Using theme colors - Background:" << backgroundColor.name()
             << "Border:" << borderColor.name() << "Text:" << textColor.name()
             << "Progress:" << progressColor.name();
    qDebug() << "Application palette Window:" << appPalette.color(QPalette::Window).name()
             << "WindowText:" << appPalette.color(QPalette::WindowText).name()
             << "Highlight:" << appPalette.color(QPalette::Highlight).name();

    // Fill background
    painter.fillPath(backgroundPath, backgroundColor);

    // Draw border
    QPen borderPen(borderColor, 1);
    painter.setPen(borderPen);
    painter.drawPath(backgroundPath);

    // Draw content (no title)
    painter.setFont(textFont);
    QRect contentRect(x + padding, y + padding, overlayWidth - padding * 2, boundingRect.height());

    // Color based on status
    if (m_currentResult.success) {
        painter.setPen(textColor);
    } else {
        painter.setPen(progressColor);
    }

    painter.drawText(contentRect, Qt::TextWordWrap, content);

    // Add copy hint at screen edge if results are ready
    if (m_currentResult.success) {
        QFont hintFont = textFont;
        hintFont.setPointSize(10);
        painter.setFont(hintFont);

        // Use theme-aware hint color
        QColor hintColor = textColor;
        hintColor.setAlpha(150); // Make it more transparent so it's less intrusive
        painter.setPen(hintColor);

        QFontMetrics hintFM(hintFont);
        QString hint = "Press Ctrl+C to copy • Press Esc to close";
        int hintWidth = hintFM.horizontalAdvance(hint) + 20; // Add some padding
        int hintHeight = hintFM.height() + 10;

        // Smart hint positioning: avoid blocking selection and overlay
        QRect hintRect;
        const int edgeMargin = 10;

        // Check distance from selection to screen edges to choose best position
        int distToTop = selection.top();
        int distToBottom = screenGeometry.height() - selection.bottom();
        int distToLeft = selection.left();
        int distToRight = screenGeometry.width() - selection.right();

        // Don't show hint if selection or overlay is too close to all edges
        bool tooCloseToTop = distToTop < 60;
        bool tooCloseToBottom = distToBottom < 60;
        bool tooCloseToLeft = distToLeft < hintWidth + 20;
        bool tooCloseToRight = distToRight < hintWidth + 20;

        // Check if overlay would conflict with hint positions
        QRect overlayRect(x, y, overlayWidth, overlayHeight);

        if (!tooCloseToTop && (distToTop > distToBottom || tooCloseToBottom)) {
            // Place hint at top of screen
            hintRect = QRect((screenGeometry.width() - hintWidth) / 2, edgeMargin,
                           hintWidth, hintHeight);
            // Hide if overlay extends to top
            if (overlayRect.top() < 80) {
                hintRect = QRect(); // Empty rect = don't draw
            }
        } else if (!tooCloseToBottom && distToBottom > 60) {
            // Place hint at bottom of screen
            hintRect = QRect((screenGeometry.width() - hintWidth) / 2,
                           screenGeometry.height() - hintHeight - edgeMargin,
                           hintWidth, hintHeight);
            // Hide if overlay extends to bottom
            if (overlayRect.bottom() > screenGeometry.height() - 80) {
                hintRect = QRect(); // Empty rect = don't draw
            }
        } else {
            // Too close to edges or conflicts - don't show hint
            hintRect = QRect(); // Empty rect = don't draw
        }

        // Draw hint only if we have a valid position
        if (!hintRect.isEmpty()) {
            // Draw subtle background for better readability
            QColor bgColor = backgroundColor;
            bgColor.setAlpha(120);
            painter.fillRect(hintRect, bgColor);

            // Draw border
            QPen borderPen(borderColor, 1);
            painter.setPen(borderPen);
            painter.drawRect(hintRect);

            // Draw text
            painter.setPen(hintColor);
            painter.drawText(hintRect, Qt::AlignCenter, hint);
        }
    }
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
        emit screenshotFinished();
        // If OCR is running, cancel it safely
        if (ocrEngine && ocrEngine->isBusy()) {
            ocrEngine->cancel();
        }
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

    // Initialize OCR engine if not already done
    if (!ocrEngine) {
        ocrEngine = new OCREngine(this);

        // Connect OCR signals
        connect(ocrEngine, &OCREngine::ocrFinished, this, &ScreenshotWidget::onOCRFinished);
        connect(ocrEngine, &OCREngine::ocrProgress, this, &ScreenshotWidget::onOCRProgress);
        connect(ocrEngine, &OCREngine::ocrError, this, &ScreenshotWidget::onOCRError);
    }

    // Load settings from SettingsWindow (refresh every time to pick up changes)
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    // Configure OCR engine based on current settings
    QString engine = settings.value("ocr/engine", "Tesseract").toString();
    if (engine == "Tesseract") {
        ocrEngine->setEngine(OCREngine::Tesseract);
    } else if (engine == "EasyOCR") {
        ocrEngine->setEngine(OCREngine::EasyOCR);
    } else if (engine == "PaddleOCR") {
        ocrEngine->setEngine(OCREngine::PaddleOCR);
    } else if (engine == "Windows OCR") {
        ocrEngine->setEngine(OCREngine::WindowsOCR);
    }

    ocrEngine->setLanguage(settings.value("ocr/language", "English").toString());
    ocrEngine->setQualityLevel(settings.value("ocr/quality", 3).toInt());
    ocrEngine->setPreprocessing(settings.value("ocr/preprocessing", true).toBool());
    ocrEngine->setAutoDetectOrientation(settings.value("ocr/autoDetect", true).toBool());

    // Configure translation settings (refresh every time to pick up language changes)
    ocrEngine->setAutoTranslate(settings.value("translation/autoTranslate", true).toBool());
    ocrEngine->setTranslationEngine(settings.value("translation/engine", "Google Translate (Free)").toString());
    ocrEngine->setTranslationSourceLanguage(settings.value("translation/sourceLanguage", "Auto-Detect").toString());
    ocrEngine->setTranslationTargetLanguage(settings.value("translation/targetLanguage", "English").toString());
    
    qDebug() << "OCR configured with target language:" << settings.value("translation/targetLanguage", "English").toString();

    // Debounce if already running
    if (m_ocrInProgress || (ocrEngine && ocrEngine->isBusy())) {
        qDebug() << "OCR already in progress - ignoring new request";
        return;
    }

    // Always use unified overlay display for consistent theming (merged OCRResultWindow functionality)
    showingToolbar = false;
    showingResults = true;
    m_progressText = "🔍 Starting OCR processing...";
    m_currentResult = OCRResult(); // Reset result

    if (!m_textOverlay) {
        m_textOverlay = new TextReplacementOverlay(nullptr);
        m_textOverlay->setAttribute(Qt::WA_DeleteOnClose, false);
        m_textOverlay->setDebugBoxes(true); // start with debug boxes; can toggle via setting later
    }
    if (!m_learningOverlay) {
        m_learningOverlay = new LanguageLearningOverlay(nullptr);
        m_learningOverlay->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    if (!m_lastOCRImage.isNull()) {
        m_textOverlay->setSourceImageSize(m_lastOCRImage.size());
    }

    // Position overlay over selection rect in global coordinates
    if (hasSelection) {
        QRect selRect(QPoint(qMin(startPoint.x(), endPoint.x()), qMin(startPoint.y(), endPoint.y())),
                      QPoint(qMax(startPoint.x(), endPoint.x()), qMax(startPoint.y(), endPoint.y())));
        QPoint globalTopLeft = mapToGlobal(selRect.topLeft());
        m_textOverlay->setGeometry(QRect(globalTopLeft, selRect.size()));
        m_textOverlay->show();
        m_textOverlay->raise();
    }
    update(); // Redraw to show overlay

    // Store the selected area for potential retry
    m_lastOCRImage = selectedArea;

    // Start OCR processing
    m_ocrInProgress = true;
    ocrEngine->performOCR(selectedArea);
}

void ScreenshotWidget::handleCancel()
{
    emit screenshotFinished();
    close();
}

void ScreenshotWidget::onOCRFinished(const OCRResult &result)
{
    qDebug() << "OCR finished. Success:" << result.success << "Text:" << result.text;

    m_ocrInProgress = false;

    // === DEBUG: TTS in OCR workflow ===
    qDebug() << "=== OCR TTS DEBUG ===";
    qDebug() << "Result success:" << result.success;
    qDebug() << "Text:" << result.text.left(50);
    qDebug() << "Language:" << result.language;
    qDebug() << "Has translation:" << result.hasTranslation;
    qDebug() << "Translated text:" << result.translatedText.left(50);
    qDebug() << "Target language:" << result.targetLanguage;

    // Use the same TTS engine and approach as Settings window test
    TTSEngine* ttsEngine = TTSManager::instance().ttsEngine();
    if (ttsEngine && result.success) {
        qDebug() << "✓ Using same TTS engine as Settings window";
        qDebug() << "✓ TTS Engine provider:" << ttsEngine->providerId();
        qDebug() << "✓ TTS Input enabled:" << ttsEngine->isTTSInputEnabled();
        qDebug() << "✓ TTS Output enabled:" << ttsEngine->isTTSOutputEnabled();

        // Configure TTS engine exactly like Settings test does
        ttsEngine->configureFromCurrentSettings();

        // Set volume to maximum like Settings test does
        ttsEngine->setVolume(1.0);
        qDebug() << "✓ TTS configured - Provider:" << ttsEngine->providerId() << "Volume:" << ttsEngine->volume();

        // Speak the OCR result if TTS input is enabled and we have text
        if (ttsEngine->isTTSInputEnabled() && !result.text.isEmpty()) {
            qDebug() << "✓ Speaking OCR text:" << result.text.left(50) << "Language:" << result.language;
            QLocale locale = result.language.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(result.language);
            qDebug() << "✓ Using locale:" << locale.name() << "Volume:" << ttsEngine->volume();
            ttsEngine->speak(result.text, true, locale);  // true = isInputText
        } else {
            qDebug() << "✗ OCR TTS skipped - Input enabled:" << ttsEngine->isTTSInputEnabled() << "Text empty:" << result.text.isEmpty();
        }

        // Speak the translation result if TTS output is enabled and we have translation
        if (ttsEngine->isTTSOutputEnabled() && result.hasTranslation && !result.translatedText.isEmpty()) {
            qDebug() << "✓ Speaking translation:" << result.translatedText.left(50) << "Target language:" << result.targetLanguage;
            QLocale locale = result.targetLanguage.isEmpty() ? QLocale() : LanguageManager::instance().localeFromLanguageCode(result.targetLanguage);
            qDebug() << "✓ Using locale:" << locale.name() << "Volume:" << ttsEngine->volume();
            ttsEngine->speak(result.translatedText, false, locale);  // false = isOutputText
        } else {
            qDebug() << "✗ Translation TTS skipped - Output enabled:" << ttsEngine->isTTSOutputEnabled()
                     << "Has translation:" << result.hasTranslation << "Translation empty:" << result.translatedText.isEmpty();
        }
    } else {
        qDebug() << "✗ TTS engine not available or OCR failed - Engine:" << (ttsEngine != nullptr) << "Success:" << result.success;
    }

    qDebug() << "=== END OCR TTS DEBUG ===";
    qDebug();

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (showingResults) {
        // Update overlay with results
        m_currentResult = result;
        m_progressText.clear();

        // Show overlay based on user's overlay mode setting
        if (result.success && !result.text.isEmpty()) {
            QString overlayMode = settings.value("translation/overlayMode", "Deep Learning Mode").toString();

            // Calculate selection rect for positioning
            QRect selRect(QPoint(qMin(startPoint.x(), endPoint.x()), qMin(startPoint.y(), endPoint.y())),
                          QPoint(qMax(startPoint.x(), endPoint.x()), qMax(startPoint.y(), endPoint.y())));
            QRect globalSelRect(mapToGlobal(selRect.topLeft()), selRect.size());

            if (overlayMode == "Deep Learning Mode") {
                // Use enhanced learning overlay
                if (m_learningOverlay) {
                    m_learningOverlay->showLearningContent(result);
                    m_learningOverlay->positionNearSelection(globalSelRect);
                }
                // Hide simple overlay
                if (m_textOverlay) {
                    m_textOverlay->hide();
                }
            } else {
                // Quick Translation mode: use the blue overlay (drawResultsOverlay) in paintEvent
                // Hide both widget overlays and let paintEvent show the blue overlay
                if (m_textOverlay) {
                    m_textOverlay->hide();
                }
                if (m_learningOverlay) {
                    m_learningOverlay->hide();
                }
                // The blue overlay will be shown by paintEvent -> drawResultsOverlay
            }
        }
        update(); // Redraw to show results
    }
}

void ScreenshotWidget::onOCRProgress(const QString &status)
{
    qDebug() << "OCR progress:" << status;

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (showingResults) {
        // Update overlay progress
        m_progressText = status;
        if (m_textOverlay && m_textOverlay->isVisible()) {
            // Keep showing selection area with maybe light translucent background
            // We don't yet have tokens; nothing to update here.
        }
        update(); // Redraw to show progress
    }
}

void ScreenshotWidget::onOCRError(const QString &error)
{
    qDebug() << "OCR error:" << error;

    m_ocrInProgress = false;

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (showingResults) {
        // Update overlay with error
        m_progressText = QString("❌ Error: %1").arg(error);
        OCRResult errorResult;
        errorResult.success = false;
        errorResult.errorMessage = error;
        m_currentResult = errorResult;
        if (m_textOverlay) {
            m_textOverlay->hide();
        }
        update(); // Redraw to show error
    }
}

void ScreenshotWidget::onOCRRetryRequested()
{
    qDebug() << "OCR retry requested";

    if (!m_lastOCRImage.isNull() && ocrEngine) {
        // Show retry progress in overlay
        m_progressText = "🔄 Retrying OCR processing...";
        update();

        // Retry OCR with the same image
        ocrEngine->performOCR(m_lastOCRImage);
    }
}