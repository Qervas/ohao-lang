#include "QuickTranslationOverlay.h"
#include "../core/ThemeManager.h"
#include "../core/AppSettings.h"
#include <QPainter>
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

void QuickTranslationOverlay::setPositionNearRect(const QRect &selectionRect, const QSize &screenSize)
{
    calculateOptimalPosition(selectionRect, screenSize);
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

void QuickTranslationOverlay::calculateOptimalPosition(const QRect &selectionRect, const QSize &screenSize)
{
    const int margin = 20; // Distance from selection

    // Calculate available space in each direction
    int spaceAbove = selectionRect.top();
    int spaceBelow = screenSize.height() - selectionRect.bottom();
    int spaceLeft = selectionRect.left();
    int spaceRight = screenSize.width() - selectionRect.right();

    QPoint position;

    // Prefer positioning below or above the selection
    if (spaceBelow >= m_panelSize.height() + margin) {
        // Position below
        position.setY(selectionRect.bottom() + margin);
        position.setX(qMax(margin, qMin(screenSize.width() - m_panelSize.width() - margin,
                                        selectionRect.center().x() - m_panelSize.width() / 2)));
    } else if (spaceAbove >= m_panelSize.height() + margin) {
        // Position above
        position.setY(selectionRect.top() - m_panelSize.height() - margin);
        position.setX(qMax(margin, qMin(screenSize.width() - m_panelSize.width() - margin,
                                        selectionRect.center().x() - m_panelSize.width() / 2)));
    } else if (spaceRight >= m_panelSize.width() + margin) {
        // Position to the right
        position.setX(selectionRect.right() + margin);
        position.setY(qMax(margin, qMin(screenSize.height() - m_panelSize.height() - margin,
                                        selectionRect.center().y() - m_panelSize.height() / 2)));
    } else if (spaceLeft >= m_panelSize.width() + margin) {
        // Position to the left
        position.setX(selectionRect.left() - m_panelSize.width() - margin);
        position.setY(qMax(margin, qMin(screenSize.height() - m_panelSize.height() - margin,
                                        selectionRect.center().y() - m_panelSize.height() / 2)));
    } else {
        // Fallback: center on screen
        position.setX((screenSize.width() - m_panelSize.width()) / 2);
        position.setY((screenSize.height() - m_panelSize.height()) / 2);
    }

    m_panelPosition = position;
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
    // Draw shadow
    QRect shadowRect = rect().adjusted(3, 3, 3, 3);
    painter.setBrush(m_shadowColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(shadowRect, m_cornerRadius, m_cornerRadius);

    // Draw main panel
    painter.setBrush(m_backgroundColor);
    painter.setPen(QPen(m_borderColor, 1));
    painter.drawRoundedRect(rect().adjusted(0, 0, -3, -3), m_cornerRadius, m_cornerRadius);
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
        emit escapePressed(); // Signal OverlayManager to hide overlays and close screenshot
        event->accept(); // Consume the event, don't propagate
        return;
    }
    QWidget::keyPressEvent(event);
}
