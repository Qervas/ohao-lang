#include "QuickTranslationOverlay.h"
#include "../core/ThemeManager.h"
#include "../core/AppSettings.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QtMath>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>

QuickTranslationOverlay::QuickTranslationOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setFocusPolicy(Qt::StrongFocus);

    // Initialize fonts
    m_titleFont = font();
    m_titleFont.setPointSize(10);
    m_titleFont.setBold(true);

    m_textFont = font();
    m_textFont.setPointSize(12);

    // Initialize colors from theme
    updateThemeColors();

    hide();
}

void QuickTranslationOverlay::setContent(const QString &originalText, const QString &translatedText)
{
    m_originalText = originalText;
    m_translatedText = translatedText;
    calculatePanelSize();
    update();
}

void QuickTranslationOverlay::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    calculatePanelSize();
    update();
}

void QuickTranslationOverlay::setPositionNearRect(const QRect &selectionRect, const QSize &screenSize, const QList<QRect> &avoidRects)
{
    calculateOptimalPosition(selectionRect, screenSize, avoidRects);
    setGeometry(m_panelPosition.x(), m_panelPosition.y(), m_panelSize.width(), m_panelSize.height());
}

void QuickTranslationOverlay::setFontScaling(float factor)
{
    m_fontScale = factor;

    // Update font sizes
    m_titleFont.setPointSize(qMax(8, int(10 * factor)));
    m_textFont.setPointSize(qMax(10, int(12 * factor)));

    calculatePanelSize();
    update();
}

void QuickTranslationOverlay::updateThemeColors()
{
    // Use centralized theming system from AppSettings
    m_backgroundColor = AppSettings::instance().getThemeColor("background");
    m_backgroundColor.setAlpha(240); // Semi-transparent

    m_textColor = AppSettings::instance().getThemeColor("text");
    m_borderColor = AppSettings::instance().getThemeColor("border");
    m_shadowColor = QColor(0, 0, 0, 80);
}

void QuickTranslationOverlay::calculatePanelSize()
{
    const int maxWidth = 400; // Maximum panel width
    int totalHeight = m_padding * 2; // Top and bottom padding

    // Calculate content height based on mode
    if (m_mode == ShowOriginal || m_mode == ShowBoth) {
        if (!m_originalText.isEmpty()) {
            totalHeight += QFontMetrics(m_titleFont).height(); // "Original:" label
            totalHeight += m_spacing / 2;
            QRect textRect = getTextRect(m_originalText, m_textFont, maxWidth - 2 * m_padding);
            totalHeight += textRect.height();
            if (m_mode == ShowBoth) totalHeight += m_spacing;
        }
    }

    if (m_mode == ShowTranslated || m_mode == ShowBoth) {
        if (!m_translatedText.isEmpty()) {
            totalHeight += QFontMetrics(m_titleFont).height(); // "Translation:" label
            totalHeight += m_spacing / 2;
            QRect textRect = getTextRect(m_translatedText, m_textFont, maxWidth - 2 * m_padding);
            totalHeight += textRect.height();
        }
    }

    // Calculate optimal width (minimum for readability, maximum for constraints)
    int optimalWidth = maxWidth;
    QString textToMeasure = (m_mode == ShowTranslated || m_translatedText.isEmpty()) ? m_translatedText : m_originalText;
    if (!textToMeasure.isEmpty()) {
        QFontMetrics fm(m_textFont);
        int textWidth = fm.horizontalAdvance(textToMeasure);
        int lines = (textWidth / (maxWidth - 2 * m_padding)) + 1;
        if (lines <= 2) {
            optimalWidth = qMin(maxWidth, textWidth + 2 * m_padding + 20); // Some extra space
        }
    }

    m_panelSize = QSize(qMax(200, optimalWidth), qMax(80, totalHeight));
}

void QuickTranslationOverlay::calculateOptimalPosition(const QRect &selectionRect, const QSize &screenSize, const QList<QRect> &avoidRects)
{
    // Store selection rect for drawing arrow
    m_selectionRect = selectionRect;
    
    const int margin = 10; // Distance from selection (closer for natural comic bubble feel)

    // Calculate available space in each direction
    int spaceAbove = selectionRect.top();
    int spaceBelow = screenSize.height() - selectionRect.bottom();
    int spaceLeft = selectionRect.left();
    int spaceRight = screenSize.width() - selectionRect.right();

    // Try positions in order of preference: below, above, right, left
    QList<QRect> candidatePositions;
    
    // Position below
    if (spaceBelow >= m_panelSize.height() + margin) {
        int x = qMax(margin, qMin(screenSize.width() - m_panelSize.width() - margin,
                                  selectionRect.center().x() - m_panelSize.width() / 2));
        int y = selectionRect.bottom() + margin;
        candidatePositions.append(QRect(x, y, m_panelSize.width(), m_panelSize.height()));
    }
    
    // Position above
    if (spaceAbove >= m_panelSize.height() + margin) {
        int x = qMax(margin, qMin(screenSize.width() - m_panelSize.width() - margin,
                                  selectionRect.center().x() - m_panelSize.width() / 2));
        int y = selectionRect.top() - m_panelSize.height() - margin;
        candidatePositions.append(QRect(x, y, m_panelSize.width(), m_panelSize.height()));
    }
    
    // Position to the right
    if (spaceRight >= m_panelSize.width() + margin) {
        int x = selectionRect.right() + margin;
        int y = qMax(margin, qMin(screenSize.height() - m_panelSize.height() - margin,
                                  selectionRect.center().y() - m_panelSize.height() / 2));
        candidatePositions.append(QRect(x, y, m_panelSize.width(), m_panelSize.height()));
    }
    
    // Position to the left
    if (spaceLeft >= m_panelSize.width() + margin) {
        int x = selectionRect.left() - m_panelSize.width() - margin;
        int y = qMax(margin, qMin(screenSize.height() - m_panelSize.height() - margin,
                                  selectionRect.center().y() - m_panelSize.height() / 2));
        candidatePositions.append(QRect(x, y, m_panelSize.width(), m_panelSize.height()));
    }

    // Find the first position that doesn't overlap with any existing selections
    QPoint position;
    bool foundPosition = false;
    int chosenIndex = -1;
    
    // Check if selection rect is in avoidRects to prevent blocking it
    QList<QRect> allAvoidRects = avoidRects;
    allAvoidRects.append(selectionRect);  // Always avoid the current selection
    
    for (int i = 0; i < candidatePositions.size(); ++i) {
        const QRect &candidate = candidatePositions[i];
        bool overlaps = false;
        
        for (const QRect &avoid : allAvoidRects) {
            if (rectsOverlap(candidate, avoid, margin)) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            position = candidate.topLeft();
            foundPosition = true;
            chosenIndex = i;
            break;
        }
    }
    
    // If all positions overlap, try with reduced margin
    if (!foundPosition) {
        for (int i = 0; i < candidatePositions.size(); ++i) {
            const QRect &candidate = candidatePositions[i];
            bool overlaps = false;
            
            for (const QRect &avoid : allAvoidRects) {
                if (rectsOverlap(candidate, avoid, 10)) {  // Reduced margin
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                position = candidate.topLeft();
                foundPosition = true;
                chosenIndex = i;
                break;
            }
        }
    }
    
    // If still overlapping, use the first candidate or fallback to center
    if (!foundPosition) {
        if (!candidatePositions.isEmpty()) {
            position = candidatePositions.first().topLeft();
            chosenIndex = 0;
        } else {
            // Fallback: center on screen
            position.setX((screenSize.width() - m_panelSize.width()) / 2);
            position.setY((screenSize.height() - m_panelSize.height()) / 2);
            m_arrowDirection = NoArrow;
            m_panelPosition = position;
            return;
        }
    }

    m_panelPosition = position;
    
    // Determine arrow direction based on which position was chosen
    // The order is: below (0), above (1), right (2), left (3)
    if (chosenIndex == 0 || (chosenIndex == -1 && spaceBelow >= spaceAbove)) {
        // Positioned below selection - arrow points up
        m_arrowDirection = ArrowUp;
        m_arrowTipPosition = QPoint(selectionRect.center().x(), selectionRect.bottom());
    } else if (chosenIndex == 1 || (chosenIndex == -1 && spaceAbove > spaceBelow)) {
        // Positioned above selection - arrow points down
        m_arrowDirection = ArrowDown;
        m_arrowTipPosition = QPoint(selectionRect.center().x(), selectionRect.top());
    } else if (chosenIndex == 2 || (chosenIndex == -1 && spaceRight >= spaceLeft)) {
        // Positioned to the right - arrow points left
        m_arrowDirection = ArrowLeft;
        m_arrowTipPosition = QPoint(selectionRect.right(), selectionRect.center().y());
    } else {
        // Positioned to the left - arrow points right
        m_arrowDirection = ArrowRight;
        m_arrowTipPosition = QPoint(selectionRect.left(), selectionRect.center().y());
    }
}

bool QuickTranslationOverlay::rectsOverlap(const QRect &rect1, const QRect &rect2, int margin) const
{
    // Expand rect2 by margin to create a buffer zone
    QRect expandedRect = rect2.adjusted(-margin, -margin, margin, margin);
    return rect1.intersects(expandedRect);
}

QRect QuickTranslationOverlay::getTextRect(const QString &text, const QFont &font, int maxWidth) const
{
    QFontMetrics fm(font);
    return fm.boundingRect(QRect(0, 0, maxWidth, 0), Qt::TextWordWrap, text);
}

void QuickTranslationOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Update colors in case theme changed
    updateThemeColors();

    // Draw the elegant panel
    drawPanel(painter);
    drawContent(painter);
}

void QuickTranslationOverlay::drawPanel(QPainter &painter)
{
    QRect panelRect = rect().adjusted(0, 0, -3, -3);
    
    // Create path for panel with arrow (comic bubble style)
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(panelRect, m_cornerRadius, m_cornerRadius);
    
    // Create arrow polygon (will be drawn separately for reliability)
    QPolygonF arrow;
    
    // Add comic-style arrow/tail pointing to selection
    if (m_arrowDirection != NoArrow) {
        const int arrowWidth = 30;   // Width of arrow base (even wider)
        const int arrowHeight = 25;  // How far arrow extends (longer)
        
        QPoint bubbleEdgeCenter;
        
        switch (m_arrowDirection) {
        case ArrowUp: {
            // Arrow pointing up (bubble is below selection)
            bubbleEdgeCenter = QPoint(panelRect.center().x(), panelRect.top());
            // Clamp to panel width
            int centerX = qBound(panelRect.left() + m_cornerRadius + 20, bubbleEdgeCenter.x(), 
                                panelRect.right() - m_cornerRadius - 20);
            arrow << QPointF(centerX - arrowWidth/2, panelRect.top())
                  << QPointF(centerX, panelRect.top() - arrowHeight)
                  << QPointF(centerX + arrowWidth/2, panelRect.top());
            break;
        }
        case ArrowDown: {
            // Arrow pointing down (bubble is above selection)
            bubbleEdgeCenter = QPoint(panelRect.center().x(), panelRect.bottom());
            int centerX = qBound(panelRect.left() + m_cornerRadius + 20, bubbleEdgeCenter.x(), 
                                panelRect.right() - m_cornerRadius - 20);
            arrow << QPointF(centerX - arrowWidth/2, panelRect.bottom())
                  << QPointF(centerX, panelRect.bottom() + arrowHeight)
                  << QPointF(centerX + arrowWidth/2, panelRect.bottom());
            break;
        }
        case ArrowLeft: {
            // Arrow pointing left (bubble is to the right of selection)
            bubbleEdgeCenter = QPoint(panelRect.left(), panelRect.center().y());
            int centerY = qBound(panelRect.top() + m_cornerRadius + 20, bubbleEdgeCenter.y(), 
                                panelRect.bottom() - m_cornerRadius - 20);
            arrow << QPointF(panelRect.left(), centerY - arrowWidth/2)
                  << QPointF(panelRect.left() - arrowHeight, centerY)
                  << QPointF(panelRect.left(), centerY + arrowWidth/2);
            break;
        }
        case ArrowRight: {
            // Arrow pointing right (bubble is to the left of selection)
            bubbleEdgeCenter = QPoint(panelRect.right(), panelRect.center().y());
            int centerY = qBound(panelRect.top() + m_cornerRadius + 20, bubbleEdgeCenter.y(), 
                                panelRect.bottom() - m_cornerRadius - 20);
            arrow << QPointF(panelRect.right(), centerY - arrowWidth/2)
                  << QPointF(panelRect.right() + arrowHeight, centerY)
                  << QPointF(panelRect.right(), centerY + arrowWidth/2);
            break;
        }
        default:
            break;
        }
    }
    
    // Draw shadow for panel
    painter.save();
    painter.translate(3, 3);
    painter.setBrush(m_shadowColor);
    painter.setPen(Qt::NoPen);
    painter.drawPath(bubblePath);
    
    // Draw shadow for arrow
    if (!arrow.isEmpty()) {
        painter.drawPolygon(arrow);
    }
    painter.restore();

    // Draw main panel (rounded rectangle)
    painter.setBrush(m_backgroundColor);
    painter.setPen(QPen(m_borderColor, 2));
    painter.drawPath(bubblePath);
    
    // Draw arrow separately (more reliable than path union)
    if (!arrow.isEmpty()) {
        painter.setBrush(m_backgroundColor);
        painter.setPen(QPen(m_borderColor, 2));
        painter.drawPolygon(arrow);
    }
}

void QuickTranslationOverlay::drawContent(QPainter &painter)
{
    const QRect contentRect = rect().adjusted(m_padding, m_padding, -m_padding - 3, -m_padding - 3);
    int currentY = contentRect.top();

    painter.setPen(m_textColor);

    // Draw original text if needed
    if ((m_mode == ShowOriginal || m_mode == ShowBoth) && !m_originalText.isEmpty()) {
        // Draw "Original:" label
        painter.setFont(m_titleFont);
        painter.drawText(contentRect.left(), currentY, contentRect.width(), QFontMetrics(m_titleFont).height(),
                        Qt::AlignLeft | Qt::AlignTop, "Original:");
        currentY += QFontMetrics(m_titleFont).height() + m_spacing / 2;

        // Draw original text
        painter.setFont(m_textFont);
        QRect textRect = getTextRect(m_originalText, m_textFont, contentRect.width());
        painter.drawText(contentRect.left(), currentY, contentRect.width(), textRect.height(),
                        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_originalText);
        currentY += textRect.height();

        if (m_mode == ShowBoth) {
            currentY += m_spacing;
        }
    }

    // Draw translated text if needed
    if ((m_mode == ShowTranslated || m_mode == ShowBoth) && !m_translatedText.isEmpty()) {
        // Draw "Translation:" label if showing both
        if (m_mode == ShowBoth) {
            painter.setFont(m_titleFont);
            painter.drawText(contentRect.left(), currentY, contentRect.width(), QFontMetrics(m_titleFont).height(),
                            Qt::AlignLeft | Qt::AlignTop, "Translation:");
            currentY += QFontMetrics(m_titleFont).height() + m_spacing / 2;
        }

        // Draw translated text
        painter.setFont(m_textFont);
        QRect textRect = getTextRect(m_translatedText, m_textFont, contentRect.width());
        painter.drawText(contentRect.left(), currentY, contentRect.width(), textRect.height(),
                        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_translatedText);
    }
}

void QuickTranslationOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Close on click
        hide();
    }
    QWidget::mousePressEvent(event);
}

void QuickTranslationOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // Hide overlay but don't consume the event - let it propagate to ScreenshotWidget
        hide();
        event->ignore(); // Let parent (ScreenshotWidget) handle ESC to exit screenshot mode
        return;
    }
    QWidget::keyPressEvent(event);
}
