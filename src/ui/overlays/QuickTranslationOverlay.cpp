#include "QuickTranslationOverlay.h"
#include "../core/ThemeManager.h"
#include "../core/AppSettings.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QLineF>
#include <QtMath>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QDebug>

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
    
    // Ensure the overlay stays within screen bounds
    int x = qBound(0, m_panelPosition.x(), screenSize.width() - m_panelSize.width());
    int y = qBound(0, m_panelPosition.y(), screenSize.height() - m_panelSize.height());
    
    setGeometry(x, y, m_panelSize.width(), m_panelSize.height());
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

    // Add arrow margin space on all sides so arrow can extend outside the panel content
    // The widget will be larger than the panel to accommodate the arrow
    int widgetWidth = qMax(200, optimalWidth) + (m_arrowMargin * 2);
    int widgetHeight = qMax(80, totalHeight) + (m_arrowMargin * 2);
    
    m_panelSize = QSize(widgetWidth, widgetHeight);
}

void QuickTranslationOverlay::calculateOptimalPosition(const QRect &selectionRect, const QSize &screenSize, const QList<QRect> &avoidRects)
{
    // Store selection rect for drawing arrow
    m_selectionRect = selectionRect;
    
    const int offset = 30; // Fixed offset distance from selection
    const int screenMargin = 10; // Margin from screen edges
    
    // Calculate the actual panel size (excluding arrow margins)
    int panelWidth = m_panelSize.width() - (m_arrowMargin * 2);
    int panelHeight = m_panelSize.height() - (m_arrowMargin * 2);

    qDebug() << "=== Position Calculation ===";
    qDebug() << "Selection:" << selectionRect;
    qDebug() << "Panel size:" << panelWidth << "x" << panelHeight;
    qDebug() << "Screen size:" << screenSize;

    // Try 4 positions and verify no intersection with selection
    QRect panelRect;
    bool positionFound = false;
    
    // Try below first (most natural)
    {
        int x = selectionRect.center().x() - panelWidth / 2;
        int y = selectionRect.bottom() + offset;
        QRect panelCandidate(x, y, panelWidth, panelHeight);
        // Calculate widget rect to check actual boundaries
        QRect widgetCandidate = panelCandidate.adjusted(-m_arrowMargin, -m_arrowMargin, m_arrowMargin, m_arrowMargin);
        
        qDebug() << "Try BELOW:";
        qDebug() << "  Panel:" << panelCandidate;
        qDebug() << "  Widget:" << widgetCandidate;
        
        // Check if WIDGET fits on screen and doesn't intersect selection
        if (widgetCandidate.left() >= screenMargin && 
            widgetCandidate.right() <= screenSize.width() - screenMargin &&
            widgetCandidate.top() >= screenMargin &&
            widgetCandidate.bottom() <= screenSize.height() - screenMargin && 
            !widgetCandidate.intersects(selectionRect)) {
            panelRect = panelCandidate;
            positionFound = true;
            qDebug() << "  ✓ BELOW works!";
        } else {
            qDebug() << "  ✗ BELOW failed - widget intersects:" << widgetCandidate.intersects(selectionRect);
        }
    }
    
    // Try above
    if (!positionFound) {
        int x = selectionRect.center().x() - panelWidth / 2;
        int y = selectionRect.top() - panelHeight - offset;
        QRect panelCandidate(x, y, panelWidth, panelHeight);
        QRect widgetCandidate = panelCandidate.adjusted(-m_arrowMargin, -m_arrowMargin, m_arrowMargin, m_arrowMargin);
        
        qDebug() << "Try ABOVE:";
        qDebug() << "  Panel:" << panelCandidate;
        qDebug() << "  Widget:" << widgetCandidate;
        
        if (widgetCandidate.left() >= screenMargin && 
            widgetCandidate.right() <= screenSize.width() - screenMargin &&
            widgetCandidate.top() >= screenMargin &&
            widgetCandidate.bottom() <= screenSize.height() - screenMargin && 
            !widgetCandidate.intersects(selectionRect)) {
            panelRect = panelCandidate;
            positionFound = true;
            qDebug() << "  ✓ ABOVE works!";
        } else {
            qDebug() << "  ✗ ABOVE failed - widget intersects:" << widgetCandidate.intersects(selectionRect);
        }
    }
    
    // Try right
    if (!positionFound) {
        int x = selectionRect.right() + offset;
        int y = selectionRect.center().y() - panelHeight / 2;
        QRect panelCandidate(x, y, panelWidth, panelHeight);
        QRect widgetCandidate = panelCandidate.adjusted(-m_arrowMargin, -m_arrowMargin, m_arrowMargin, m_arrowMargin);
        
        qDebug() << "Try RIGHT:";
        qDebug() << "  Panel:" << panelCandidate;
        qDebug() << "  Widget:" << widgetCandidate;
        
        if (widgetCandidate.left() >= screenMargin &&
            widgetCandidate.right() <= screenSize.width() - screenMargin &&
            widgetCandidate.top() >= screenMargin &&
            widgetCandidate.bottom() <= screenSize.height() - screenMargin && 
            !widgetCandidate.intersects(selectionRect)) {
            panelRect = panelCandidate;
            positionFound = true;
            qDebug() << "  ✓ RIGHT works!";
        } else {
            qDebug() << "  ✗ RIGHT failed - widget intersects:" << widgetCandidate.intersects(selectionRect);
        }
    }
    
    // Try left
    if (!positionFound) {
        int x = selectionRect.left() - panelWidth - offset;
        int y = selectionRect.center().y() - panelHeight / 2;
        QRect panelCandidate(x, y, panelWidth, panelHeight);
        QRect widgetCandidate = panelCandidate.adjusted(-m_arrowMargin, -m_arrowMargin, m_arrowMargin, m_arrowMargin);
        
        qDebug() << "Try LEFT:";
        qDebug() << "  Panel:" << panelCandidate;
        qDebug() << "  Widget:" << widgetCandidate;
        
        if (widgetCandidate.left() >= screenMargin &&
            widgetCandidate.right() <= screenSize.width() - screenMargin &&
            widgetCandidate.top() >= screenMargin &&
            widgetCandidate.bottom() <= screenSize.height() - screenMargin && 
            !widgetCandidate.intersects(selectionRect)) {
            panelRect = panelCandidate;
            positionFound = true;
            qDebug() << "  ✓ LEFT works!";
        } else {
            qDebug() << "  ✗ LEFT failed - widget intersects:" << widgetCandidate.intersects(selectionRect);
        }
    }
    
    // Fallback: Smart positioning when all else fails
    if (!positionFound) {
        qDebug() << "⚠️ FALLBACK positioning";
        
        // Place in the corner with most space
        int x, y;
        
        // Check which quadrant has most space
        int spaceBelow = screenSize.height() - selectionRect.bottom();
        int spaceAbove = selectionRect.top();
        int spaceRight = screenSize.width() - selectionRect.right();
        int spaceLeft = selectionRect.left();
        
        if (spaceBelow >= spaceAbove && spaceBelow >= spaceRight && spaceBelow >= spaceLeft) {
            // Most space below
            x = qBound(screenMargin, selectionRect.center().x() - panelWidth / 2, 
                      screenSize.width() - panelWidth - screenMargin);
            y = qBound(selectionRect.bottom() + offset, selectionRect.bottom() + offset, 
                      screenSize.height() - panelHeight - screenMargin);
        } else if (spaceAbove > spaceRight && spaceAbove > spaceLeft) {
            // Most space above
            x = qBound(screenMargin, selectionRect.center().x() - panelWidth / 2, 
                      screenSize.width() - panelWidth - screenMargin);
            y = qBound(screenMargin, selectionRect.top() - panelHeight - offset, 
                      selectionRect.top() - panelHeight - offset);
        } else if (spaceRight > spaceLeft) {
            // Most space right
            x = qBound(selectionRect.right() + offset, selectionRect.right() + offset, 
                      screenSize.width() - panelWidth - screenMargin);
            y = qBound(screenMargin, selectionRect.center().y() - panelHeight / 2, 
                      screenSize.height() - panelHeight - screenMargin);
        } else {
            // Most space left
            x = qBound(screenMargin, selectionRect.left() - panelWidth - offset, 
                      selectionRect.left() - panelWidth - offset);
            y = qBound(screenMargin, selectionRect.center().y() - panelHeight / 2, 
                      screenSize.height() - panelHeight - screenMargin);
        }
        
        panelRect = QRect(x, y, panelWidth, panelHeight);
        qDebug() << "  Fallback position:" << panelRect;
    }
    
    // Convert panel rect to widget rect (add arrow margin space on all sides)
    QRect widgetRect = panelRect.adjusted(-m_arrowMargin, -m_arrowMargin, m_arrowMargin, m_arrowMargin);
    m_panelPosition = widgetRect.topLeft();
    
    qDebug() << "Final panel rect:" << panelRect;
    qDebug() << "Final widget rect:" << widgetRect;
    qDebug() << "Intersects selection?" << panelRect.intersects(selectionRect);
    qDebug() << "=== End Position Calculation ===\n";
    
    // Calculate dynamic arrow points (nearest point on panel to nearest point on selection)
    calculateArrowPoints(panelRect, selectionRect);
}

bool QuickTranslationOverlay::rectsOverlap(const QRect &rect1, const QRect &rect2, int margin) const
{
    // Expand rect2 by margin to create a buffer zone
    QRect expandedRect = rect2.adjusted(-margin, -margin, margin, margin);
    return rect1.intersects(expandedRect);
}

QPoint QuickTranslationOverlay::closestPointOnRect(const QRect &rect, const QPoint &point) const
{
    // Find the closest point on the rectangle's perimeter to the given point
    int x = qBound(rect.left(), point.x(), rect.right());
    int y = qBound(rect.top(), point.y(), rect.bottom());
    
    // If the point is inside the rect, find the closest edge
    if (rect.contains(point)) {
        int distToLeft = point.x() - rect.left();
        int distToRight = rect.right() - point.x();
        int distToTop = point.y() - rect.top();
        int distToBottom = rect.bottom() - point.y();
        
        int minDist = qMin(qMin(distToLeft, distToRight), qMin(distToTop, distToBottom));
        
        if (minDist == distToLeft) return QPoint(rect.left(), point.y());
        if (minDist == distToRight) return QPoint(rect.right(), point.y());
        if (minDist == distToTop) return QPoint(point.x(), rect.top());
        return QPoint(point.x(), rect.bottom());
    }
    
    return QPoint(x, y);
}

void QuickTranslationOverlay::calculateArrowPoints(const QRect &panelRect, const QRect &selectionRect)
{
    // Find the center points of both rects
    QPoint panelCenter = panelRect.center();
    QPoint selectionCenter = selectionRect.center();
    
    // Find the closest point on the selection to the panel center
    m_arrowTipPoint = closestPointOnRect(selectionRect, panelCenter);
    
    // Find the closest point on the panel to the selection center
    m_arrowBasePoint = closestPointOnRect(panelRect, selectionCenter);
    
    // Only show arrow if the distance is reasonable
    int distance = QLineF(m_arrowBasePoint, m_arrowTipPoint).length();
    m_hasArrow = (distance > 5); // Don't show arrow if too close
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
    // Inset the panel rect by arrow margin on all sides, leaving room for arrow
    QRect panelRect = rect().adjusted(m_arrowMargin, m_arrowMargin, -m_arrowMargin - 3, -m_arrowMargin - 3);
    
    // Create path for rounded rectangle panel
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(panelRect, m_cornerRadius, m_cornerRadius);
    
    // Create dynamic arrow polygon pointing from panel to selection
    QPolygonF arrow;
    if (m_hasArrow) {
        const int arrowWidth = 25;  // Width of arrow base
        
        // Convert arrow points from screen coordinates to widget coordinates
        QPoint baseInWidget = m_arrowBasePoint - rect().topLeft();
        QPoint tipInWidget = m_arrowTipPoint - rect().topLeft();
        
        // Calculate perpendicular vector for arrow width
        QLineF arrowLine(baseInWidget, tipInWidget);
        QLineF perpendicular = arrowLine.normalVector();
        perpendicular.setLength(arrowWidth / 2.0);
        
        // Create arrow triangle
        QPointF p1 = baseInWidget + QPointF(perpendicular.dx(), perpendicular.dy());
        QPointF p2 = tipInWidget;
        QPointF p3 = baseInWidget - QPointF(perpendicular.dx(), perpendicular.dy());
        
        arrow << p1 << p2 << p3;
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
    
    // Draw arrow separately with same style as panel
    if (!arrow.isEmpty()) {
        painter.setBrush(m_backgroundColor);
        painter.setPen(QPen(m_borderColor, 2));
        painter.drawPolygon(arrow);
    }
}

void QuickTranslationOverlay::drawContent(QPainter &painter)
{
    // Adjust content rect to account for arrow margin (panel is inset within widget)
    const QRect contentRect = rect().adjusted(m_arrowMargin + m_padding, m_arrowMargin + m_padding, 
                                             -m_arrowMargin - m_padding - 3, -m_arrowMargin - m_padding - 3);
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
